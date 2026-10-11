import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs_frontend/logging/log_backend.dart';
import 'package:mgcs_frontend/logging/log_controller.dart';
import 'package:mgcs_frontend/logging/log_models.dart';

/// 按脚本响应的替身后端：不接触 dart:ffi，用来驱动下载状态机。
class _FakeLogBackend implements LogBackend {
  bool connected = true;

  // 列表
  List<Map<String, Object?>> listed = [];
  int expected = 0;
  int entriesCalls = 0;

  // 擦除
  int eraseCalls = 0;
  Map<String, Object?> eraseResponse = _ok();

  // 下载
  int cancelCalls = 0;
  Map<String, Object?> downloadResponse = _ok();
  final List<Map<String, Object?>> statusQueue = [];

  @override
  bool get isConnected => connected;

  @override
  Map<String, Object?> requestList() => _ok();

  @override
  Map<String, Object?> entries() {
    entriesCalls++;
    return _ok({
      'logs': listed,
      'expected': expected,
      'complete': expected > 0 && listed.length >= expected,
    });
  }

  @override
  Map<String, Object?> download(int logId, String filepath) => downloadResponse;

  @override
  Map<String, Object?> cancel() {
    cancelCalls++;
    return _ok();
  }

  @override
  Map<String, Object?> eraseAll() {
    eraseCalls++;
    return eraseResponse;
  }

  @override
  Map<String, Object?> status() {
    if (statusQueue.isEmpty) return _status('idle');
    return statusQueue.removeAt(0);
  }
}

Map<String, Object?> _ok([Map<String, Object?> extra = const {}]) => {
  'success': true,
  ...extra,
};

Map<String, Object?> _status(
  String state, {
  int logId = 7,
  int received = 0,
  int total = 0,
  bool sizeDeclared = true,
  int saved = 0,
  String path = '',
  String message = '',
}) => _ok({
  'log_id': logId,
  'state': state,
  'received_bytes': received,
  'total_bytes': total,
  'size_declared': sizeDeclared,
  'saved_bytes': saved,
  'path': path,
  'message': message,
});

Map<String, Object?> _entry(int id, {int timeUtc = 0, int size = 0}) => {
  'id': id,
  'time_utc': timeUtc,
  'size_bytes': size,
};

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
  late _FakeLogBackend backend;
  LogController? controller;

  setUp(() {
    backend = _FakeLogBackend();
    tempDir = Directory.systemTemp.createTempSync('mgcs-logs-test');
    controller = LogController(
      backend: backend,
      directoryProvider: () async => tempDir!,
      statusPollInterval: const Duration(milliseconds: 4),
      listPollInterval: const Duration(milliseconds: 1),
      listTimeout: const Duration(milliseconds: 400),
    )..bindSession(1);
  });

  tearDown(() {
    controller?.dispose();
    tempDir?.deleteSync(recursive: true);
  });

  test('列表按 expected/complete 收齐并按编号倒序', () async {
    backend.listed = [_entry(7, size: 4096), _entry(8, size: 8192)];
    backend.expected = 2;

    await controller!.refreshList();

    expect(controller!.loadingList, isFalse);
    expect(controller!.listError, isNull);
    expect(controller!.listComplete, isTrue);
    expect(controller!.entries.map((e) => e.id), [8, 7]);
    expect(controller!.entries.first.sizeBytes, 8192);
  });

  test('列表没有 expected 时条目数稳定后结束且不误报错误', () async {
    backend.listed = [_entry(3, size: 1024)];
    backend.expected = 0;

    await controller!.refreshList();

    expect(controller!.listComplete, isFalse);
    expect(controller!.entries.single.id, 3);
    // 有内容但飞控没给 num_logs：既不算收齐，也不能报“未响应”。
    expect(controller!.listError, isNull);
  });

  test('断链时刷新给出明确错误而不是停在加载态', () async {
    backend.connected = false;

    await controller!.refreshList();

    expect(controller!.loadingList, isFalse);
    expect(controller!.listError, contains('未连接飞控'));
    expect(backend.entriesCalls, 0);
  });

  test('下载正常完成后携带落盘字节与路径', () async {
    final entry = OnboardLogEntry.fromJson(_entry(7, timeUtc: 0, size: 300));
    backend.downloadResponse = _ok();
    backend.statusQueue.addAll([
      _status('downloading', received: 150, total: 300),
      _status(
        'completed',
        received: 300,
        total: 300,
        saved: 300,
        path: '${tempDir!.path}/log_7_unknown.bin',
      ),
    ]);

    await controller!.downloadEntry(entry);
    await _waitFor(
      () => controller!.download?.phase == LogDownloadPhase.completed,
      reason: '下载未到达完成态',
    );

    final download = controller!.download!;
    expect(download.logId, 7);
    expect(download.savedBytes, 300);
    expect(download.path, contains('log_7_unknown.bin'));
    expect(download.progress, 1);
    expect(controller!.isDownloading, isFalse);
  });

  test('取消会回到 cancelled 终态', () async {
    final entry = OnboardLogEntry.fromJson(_entry(7, size: 300));
    backend.statusQueue.addAll([
      _status('downloading', received: 60, total: 300),
      _status('cancelled', received: 60, total: 300),
    ]);

    await controller!.downloadEntry(entry);
    await _waitFor(() => controller!.isDownloading);
    await controller!.cancelDownload();

    expect(backend.cancelCalls, 1);
    await _waitFor(
      () => controller!.download?.phase == LogDownloadPhase.cancelled,
    );
    expect(controller!.download!.phase, LogDownloadPhase.cancelled);
  });

  test('落盘失败进入 failed 且带可读原因', () async {
    final entry = OnboardLogEntry.fromJson(_entry(7, size: 300));
    backend.statusQueue.addAll([
      _status('downloading', received: 100, total: 300),
      _status('failed', received: 300, total: 300, message: '写文件失败：空间不足'),
    ]);

    await controller!.downloadEntry(entry);
    await _waitFor(
      () => controller!.download?.phase == LogDownloadPhase.failed,
    );

    expect(controller!.download!.message, contains('空间不足'));
  });

  test('未连接时发起下载会给出错误且不生成文件', () async {
    final entry = OnboardLogEntry.fromJson(_entry(7, size: 300));
    backend.connected = false;

    await controller!.downloadEntry(entry);

    expect(controller!.actionError, contains('未连接飞控'));
    expect(controller!.download, isNull);
    expect(tempDir!.listSync(), isEmpty);
  });

  test('切换会话丢弃进行中的下载，旧状态不会写回', () async {
    final entry = OnboardLogEntry.fromJson(_entry(7, size: 300));
    backend.statusQueue.add(_status('downloading', received: 50, total: 300));

    await controller!.downloadEntry(entry);
    await _waitFor(() => controller!.isDownloading);

    controller!.bindSession(2);
    expect(controller!.download, isNull);

    // 旧会话的迟到状态不应复活下载卡片。
    backend.statusQueue.add(
      _status('completed', received: 300, total: 300, saved: 300),
    );
    await Future<void>.delayed(const Duration(milliseconds: 40));
    expect(controller!.download, isNull);
  });

  test('飞控未声明大小时完成态标记为未校验', () async {
    final entry = OnboardLogEntry.fromJson(_entry(7, size: 0));
    backend.statusQueue.add(
      _status(
        'completed',
        received: 90,
        total: 90,
        sizeDeclared: false,
        saved: 90,
        path: '${tempDir!.path}/log_7_unknown.bin',
      ),
    );

    await controller!.downloadEntry(entry);
    await _waitFor(
      () => controller!.download?.phase == LogDownloadPhase.completed,
    );

    final download = controller!.download!;
    expect(download.sizeDeclared, isFalse);
    expect(download.sizeVerified, isFalse);
  });

  test('擦除成功后清空列表并给出提示', () async {
    backend.listed = [_entry(7, size: 4096)];
    backend.expected = 1;
    await controller!.refreshList();
    expect(controller!.entries, isNotEmpty);

    await controller!.eraseAll();

    expect(backend.eraseCalls, 1);
    expect(controller!.entries, isEmpty);
    expect(controller!.actionMessage, contains('擦除'));
  });

  test('下载进行中拒绝擦除', () async {
    backend.statusQueue.add(_status('downloading', received: 10, total: 300));
    final entry = OnboardLogEntry.fromJson(_entry(7, size: 300));
    await controller!.downloadEntry(entry);
    await _waitFor(() => controller!.isDownloading);

    await controller!.eraseAll();

    expect(backend.eraseCalls, 0);
    expect(controller!.actionError, contains('下载'));
  });

  test('未连接时擦除给出明确错误', () async {
    backend.connected = false;

    await controller!.eraseAll();

    expect(backend.eraseCalls, 0);
    expect(controller!.actionError, contains('未连接飞控'));
  });
}
