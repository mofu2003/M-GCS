import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/logging/file_controller.dart';
import 'package:mgcs_frontend/logging/file_index.dart';
import 'package:mgcs_frontend/logging/file_models.dart';

import 'support/file_backend_fake.dart';

const _remoteBin = '@SYS/a.bin';

RemoteFileEntry _file(String name, {int size = 8}) =>
    RemoteFileEntry(name: name, isDirectory: false, sizeBytes: size);

const _dirEntry = RemoteFileEntry(
  name: 'LOGS',
  isDirectory: true,
  sizeBytes: 0,
);

Future<void> _waitFor(
  bool Function() predicate, {
  Duration timeout = const Duration(seconds: 3),
  String reason = '条件未在超时内满足',
}) async {
  final deadline = DateTime.now().add(timeout);
  while (!predicate()) {
    if (DateTime.now().isAfter(deadline)) fail(reason);
    await Future<void>.delayed(const Duration(milliseconds: 4));
  }
}

void main() {
  Directory? tempDir;
  late FakeFileBackend backend;
  FileController? controller;

  FileIndexStore store() => FileIndexStore(() async => tempDir!);

  setUp(() {
    backend = FakeFileBackend();
    tempDir = Directory.systemTemp.createTempSync('mgcs-files-test');
    controller = FileController(
      backend: backend,
      indexStore: store(),
      downloadDirectoryProvider: () async =>
          Directory('${tempDir!.path}/downloads'),
      statusPollInterval: const Duration(milliseconds: 4),
      listTimeout: const Duration(milliseconds: 400),
    )..bindSession(1);
  });

  tearDown(() {
    controller?.dispose();
    tempDir?.deleteSync(recursive: true);
  });

  test('列表成功：目录在前、同类按名排序、写入本地索引', () async {
    backend.statusQueue.add(
      fileStatus(
        'completed',
        items: [
          fileItem('b.bin', size: 10),
          fileItem('LOGS', directory: true),
          fileItem('a.txt', size: 20),
        ],
      ),
    );

    await controller!.refreshList();

    expect(controller!.listing, isFalse);
    expect(controller!.listError, isNull);
    expect(controller!.entriesFromCache, isFalse);
    expect(controller!.entries.map((e) => e.name), ['LOGS', 'a.txt', 'b.bin']);
    // 索引落到磁盘，另一个 store 实例也能读到。
    final persisted = await store().load();
    expect(persisted.directory(FileController.defaultPath), isNotNull);
    expect(
      persisted
          .directory(FileController.defaultPath)!
          .entries
          .map((e) => e.name),
      contains('a.txt'),
    );
  });

  test('断链时回填缓存并标注可能过期', () async {
    backend.statusQueue.add(
      fileStatus('completed', items: [fileItem('a.bin', size: 5)]),
    );
    await controller!.refreshList();
    expect(controller!.entriesFromCache, isFalse);

    backend.connected = false;
    await controller!.refreshList();

    expect(controller!.listError, contains('未连接飞控'));
    expect(controller!.entriesFromCache, isTrue);
    expect(controller!.entries.single.name, 'a.bin');
    expect(controller!.cacheUpdatedAtMs, greaterThan(0));
    expect(backend.listedPaths.length, 1);
  });

  test('列表失败时给出飞控原因并回退缓存', () async {
    backend.statusQueue.add(
      fileStatus('completed', items: [fileItem('a.bin', size: 5)]),
    );
    await controller!.refreshList();

    backend.statusQueue.add(fileStatus('failed', message: '目录不存在'));
    await controller!.refreshList();

    expect(controller!.listing, isFalse);
    expect(controller!.listError, contains('目录不存在'));
    expect(controller!.entriesFromCache, isTrue);
    expect(controller!.entries.single.name, 'a.bin');
  });

  test('列表被取消时进入已取消提示', () async {
    backend.statusQueue.add(fileStatus('cancelled'));
    await controller!.refreshList();

    expect(controller!.listing, isFalse);
    expect(controller!.listError, contains('已取消'));
  });

  test('飞控一直不返回时列表超时', () async {
    backend.persistentStatus = fileStatus('running');
    await controller!.refreshList();

    expect(controller!.listing, isFalse);
    expect(controller!.listError, contains('超时'));
  });

  test('刷新发起失败时直接给出错误', () async {
    backend.listResponse = fileError('NOT_CONNECTED', '未连接飞控');

    await controller!.refreshList();

    expect(controller!.listing, isFalse);
    expect(controller!.listError, contains('未连接飞控'));
    expect(controller!.transfer, isNull);
  });

  test('打开子目录与返回上一级', () async {
    backend.statusQueue.add(
      fileStatus('completed', items: [fileItem('LOGS', directory: true)]),
    );
    await controller!.refreshList();
    expect(controller!.currentPath, FileController.defaultPath);

    backend.statusQueue.add(fileStatus('completed', items: [fileItem('x')]));
    await controller!.openDirectory('LOGS');
    expect(controller!.currentPath, '@SYS/LOGS');
    expect(backend.listedPaths.last, '@SYS/LOGS');

    backend.statusQueue.add(fileStatus('completed', items: [fileItem('y')]));
    await controller!.goUp();
    expect(controller!.currentPath, '@SYS');
  });

  test('下载完成：落盘路径与本地副本索引一致', () async {
    await controller!.downloadEntry(_file('a.bin'));

    await _waitFor(
      () => controller!.transfer?.phase == FileTransferPhase.completed,
      reason: '下载未到达完成态',
    );

    expect(controller!.actionError, isNull);
    expect(controller!.isTransferring, isFalse);
    final localPath = controller!.transfer!.localPath;
    expect(localPath, contains('a.bin'));
    expect(backend.downloads.single.$1, _remoteBin);

    await _waitFor(
      () => controller!.localCopy(_remoteBin) != null,
      reason: '下载未写入本地索引',
    );
    expect(controller!.localCopy(_remoteBin)!.localPath, localPath);
    expect(controller!.localCopy(_remoteBin)!.sizeBytes, 8);
  });

  test('下载失败：保留可读原因', () async {
    backend.autoStatus = false;
    backend.persistentStatus = fileStatus(
      'failed',
      task: 'download',
      message: '磁盘已满',
    );

    await controller!.downloadEntry(_file('a.bin'));
    await _waitFor(
      () => controller!.transfer?.phase == FileTransferPhase.failed,
    );

    expect(controller!.actionError, contains('磁盘已满'));
    expect(controller!.localCopy(_remoteBin), isNull);
  });

  test('上传完成：本地文件送到当前目录', () async {
    final source = File('${tempDir!.path}/up.bin')
      ..writeAsBytesSync(List<int>.filled(4, 0));

    await controller!.uploadFile(source.path);
    await _waitFor(
      () => controller!.transfer?.phase == FileTransferPhase.completed,
      reason: '上传未到达完成态',
    );

    expect(controller!.actionError, isNull);
    expect(controller!.actionMessage, contains('已上传'));
    expect(backend.uploads.single.$2, '@SYS/up.bin');
  });

  test('上传不存在的本地文件会被拒绝', () async {
    await controller!.uploadFile('${tempDir!.path}/missing.bin');

    expect(controller!.actionError, contains('不存在'));
    expect(backend.uploads, isEmpty);
  });

  test('已有传输在跑时拒绝第二次发起', () async {
    backend.autoStatus = false;
    backend.persistentStatus = fileStatus(
      'running',
      task: 'download',
      remotePath: _remoteBin,
      localPath: 'x',
    );

    await controller!.downloadEntry(_file('a.bin'));
    await _waitFor(() => controller!.isTransferring);

    await controller!.downloadEntry(_file('b.bin'));

    expect(controller!.actionError, contains('已有文件传输在进行'));
    expect(backend.downloads.length, 1);
  });

  test('取消传输：调用 native 取消并进入已取消终态', () async {
    backend.autoStatus = false;
    backend.persistentStatus = fileStatus(
      'running',
      task: 'download',
      remotePath: _remoteBin,
      localPath: 'x',
    );

    await controller!.downloadEntry(_file('a.bin'));
    await _waitFor(() => controller!.isTransferring);
    await controller!.cancelTransfer();
    expect(backend.cancelCalls, 1);

    backend.persistentStatus = fileStatus(
      'cancelled',
      task: 'download',
      remotePath: _remoteBin,
      localPath: 'x',
    );
    await _waitFor(
      () => controller!.transfer?.phase == FileTransferPhase.cancelled,
    );
    expect(controller!.actionMessage, contains('已取消'));
  });

  test('断链时拒绝下载且不创建文件', () async {
    backend.connected = false;

    await controller!.downloadEntry(_file('a.bin'));

    expect(controller!.actionError, contains('未连接飞控'));
    expect(backend.downloads, isEmpty);
    expect(Directory('${tempDir!.path}/downloads').existsSync(), isFalse);
  });

  test('目录条目不能被下载', () async {
    await controller!.downloadEntry(_dirEntry);

    expect(controller!.actionError, contains('目录'));
    expect(backend.downloads, isEmpty);
  });

  test('切换会话丢弃进行中的传输，旧状态不写回', () async {
    backend.autoStatus = false;
    backend.persistentStatus = fileStatus(
      'running',
      task: 'download',
      remotePath: _remoteBin,
      localPath: 'x',
    );

    await controller!.downloadEntry(_file('a.bin'));
    await _waitFor(() => controller!.isTransferring);

    controller!.bindSession(2);
    expect(controller!.transfer, isNull);

    backend.persistentStatus = fileStatus(
      'completed',
      task: 'download',
      remotePath: _remoteBin,
      localPath: 'x',
      received: 8,
      total: 8,
    );
    await Future<void>.delayed(const Duration(milliseconds: 40));
    expect(controller!.transfer, isNull);
  });

  test('本地索引损坏时按空索引恢复', () async {
    File(
      '${tempDir!.path}/file_index.json',
    ).writeAsStringSync('{not-json', flush: true);

    final recovered = await store().load();

    expect(recovered.directories, isEmpty);
    expect(recovered.downloads, isEmpty);
  });

  test('索引读写往返保持目录与下载记录', () async {
    final first = store();
    await first.recordDirectory(FileController.defaultPath, [_file('a.bin')]);
    await first.recordDownload(_remoteBin, '/tmp/a.bin', 8);

    final loaded = await store().load();

    expect(
      loaded.directory(FileController.defaultPath)!.entries.single.name,
      'a.bin',
    );
    expect(loaded.download(_remoteBin)!.localPath, '/tmp/a.bin');
    expect(loaded.download(_remoteBin)!.sizeBytes, 8);
  });

  test('状态解析：任务与阶段对齐 native 字段', () {
    expect(fileTaskFromName('list'), FileTaskKind.list);
    expect(fileTaskFromName('upload'), FileTaskKind.upload);
    expect(fileTaskFromName('nope'), FileTaskKind.idle);
    expect(
      fileTransferPhaseFromState('cancelling'),
      FileTransferPhase.cancelling,
    );
    expect(fileTransferPhaseFromState('failed'), FileTransferPhase.failed);
    expect(fileTransferPhaseFromState('unknown'), FileTransferPhase.idle);
  });

  test('进度：完成态为 1，运行态按字节比例', () {
    final running = FileTransfer.fromStatus(
      fileStatus('running', task: 'download', received: 10, total: 20),
    );
    expect(running.progress, closeTo(0.5, 1e-9));
    expect(running.isActive, isTrue);

    final done = FileTransfer.fromStatus(
      fileStatus('completed', task: 'download', received: 10, total: 20),
    );
    expect(done.progress, 1);
    expect(done.isTerminal, isTrue);
  });
}
