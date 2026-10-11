import 'dart:io';

import 'package:mgcs_frontend/logging/file_backend.dart';
import 'package:mgcs_frontend/logging/file_index.dart';
import 'package:mgcs_frontend/logging/file_models.dart';

/// `mg_fs_*` 的替身后端：不接触 `dart:ffi`，按脚本驱动文件传输状态机。
///
/// 目录列表走 [statusQueue]，因为 `FileController.refreshList` 自己轮询；
/// 下载/上传由控制器启动定时轮询，所以默认在发起时自动排入
/// 「running → completed」两帧，测试也可以关掉 [autoStatus] 手动控制。
class FakeFileBackend implements FileBackend {
  bool connected = true;

  /// 关闭后 download/upload 不再自动入队状态，改由测试写 [statusQueue]。
  bool autoStatus = true;

  Map<String, Object?> listResponse = fileOk();
  final List<String> listedPaths = <String>[];

  Map<String, Object?> downloadResponse = fileOk();
  final List<(String remote, String local)> downloads = <(String, String)>[];

  Map<String, Object?> uploadResponse = fileOk();
  final List<(String local, String remote)> uploads = <(String, String)>[];

  int cancelCalls = 0;

  /// 轮询时优先取这里的帧，取空后返回 [persistentStatus]。
  final List<Map<String, Object?>> statusQueue = <Map<String, Object?>>[];

  /// 队列为空时反复返回的状态，用来模拟「一直在跑」或超时。
  Map<String, Object?> persistentStatus = fileStatus('idle');

  @override
  bool get isConnected => connected;

  @override
  Map<String, Object?> listDirectory(String remotePath) {
    listedPaths.add(remotePath);
    return listResponse;
  }

  @override
  Map<String, Object?> downloadFile(String remotePath, String localPath) {
    downloads.add((remotePath, localPath));
    if (autoStatus && downloadResponse['success'] == true) {
      statusQueue.addAll([
        fileStatus(
          'running',
          task: 'download',
          remotePath: remotePath,
          localPath: localPath,
          received: 4,
          total: 8,
        ),
        fileStatus(
          'completed',
          task: 'download',
          remotePath: remotePath,
          localPath: localPath,
          received: 8,
          total: 8,
        ),
      ]);
    }
    return downloadResponse;
  }

  @override
  Map<String, Object?> uploadFile(String localPath, String remotePath) {
    uploads.add((localPath, remotePath));
    if (autoStatus && uploadResponse['success'] == true) {
      statusQueue.addAll([
        fileStatus(
          'running',
          task: 'upload',
          remotePath: remotePath,
          localPath: localPath,
          received: 2,
          total: 4,
        ),
        fileStatus(
          'completed',
          task: 'upload',
          remotePath: remotePath,
          localPath: localPath,
          received: 4,
          total: 4,
        ),
      ]);
    }
    return uploadResponse;
  }

  @override
  Map<String, Object?> transferStatus() =>
      statusQueue.isNotEmpty ? statusQueue.removeAt(0) : persistentStatus;

  @override
  Map<String, Object?> cancel() {
    cancelCalls++;
    return fileOk();
  }
}

Map<String, Object?> fileOk([Map<String, Object?> extra = const {}]) => {
  'success': true,
  ...extra,
};

Map<String, Object?> fileError(String name, String message, {int code = -1}) =>
    {
      'success': false,
      'error': {'code': code, 'name': name, 'message': message},
    };

Map<String, Object?> fileStatus(
  String state, {
  String task = 'list',
  String remotePath = '',
  String localPath = '',
  int received = 0,
  int total = 0,
  int errorCode = 0,
  String message = '',
  List<Map<String, Object?>> items = const [],
}) => fileOk({
  'task': task,
  'state': state,
  'remote_path': remotePath,
  'local_path': localPath,
  'received_bytes': received,
  'total_bytes': total,
  'error_code': errorCode,
  'message': message,
  'items': items,
});

Map<String, Object?> fileItem(
  String name, {
  bool directory = false,
  int size = 0,
}) => {'name': name, 'directory': directory, 'size_bytes': size};

/// 纯内存索引：widget 测试不能做真实磁盘 IO（`testWidgets` 的 fake-async
/// 区里 `dart:io` 的 future 不会完成，会挂住）。磁盘读写由控制器单测覆盖。
class MemoryFileIndexStore extends FileIndexStore {
  MemoryFileIndexStore() : super(() async => Directory.systemTemp);

  final FileIndex _index = FileIndex();

  @override
  bool get writeFailed => false;

  @override
  Future<FileIndex> load() async => _index;

  @override
  Future<void> recordDirectory(
    String remotePath,
    List<RemoteFileEntry> entries, {
    DateTime? now,
  }) async {
    _index.directories[remotePath] = CachedDirectory(
      updatedAtMs: (now ?? DateTime.now()).millisecondsSinceEpoch,
      entries: entries,
    );
  }

  @override
  Future<void> recordDownload(
    String remotePath,
    String localPath,
    int sizeBytes, {
    DateTime? now,
  }) async {
    _index.downloads[remotePath] = CachedDownload(
      localPath: localPath,
      sizeBytes: sizeBytes,
      downloadedAtMs: (now ?? DateTime.now()).millisecondsSinceEpoch,
    );
  }
}
