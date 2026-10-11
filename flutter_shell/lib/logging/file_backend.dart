/// MAVLink FTP 文件访问的原生边界。
///
/// 只暴露 `mg_fs_*` 导出：目录列表、下载、上传三条发起路径，加上状态轮询与取消。
/// 与 `mg_onboard_log_*` 一样是「发起立即返回 → 轮询状态 → 可取消」的异步模式，
/// 页面与控制器通过它访问原生层，便于用替身测试传输状态机。
abstract interface class FileBackend {
  /// 当前连接是否可用于发起传输（native 侧还有一道 `CHECK_CONNECTED`）。
  bool get isConnected;

  /// 列出机载目录 [remotePath]（`mg_fs_list_directory`），立即返回。
  Map<String, Object?> listDirectory(String remotePath);

  /// 把机载文件 [remotePath] 下载到本地 [localPath]（`mg_fs_download_file`）。
  Map<String, Object?> downloadFile(String remotePath, String localPath);

  /// 把本地文件 [localPath] 上传到机载 [remotePath]（`mg_fs_upload_file`）。
  Map<String, Object?> uploadFile(String localPath, String remotePath);

  /// 读取当前传输状态（`mg_fs_transfer_status`）。纯读取，没有副作用，
  /// 断链后仍能读到最终结果。
  Map<String, Object?> transferStatus();

  /// 取消正在进行的传输（`mg_fs_cancel`），无活动传输时返回 `NOT_SUPPORTED`。
  Map<String, Object?> cancel();
}

/// 原生库不可用时的空实现。
///
/// 让文件浏览页在 `initializationError != null`（例如 flutter_tester 没有原生库）
/// 时仍能渲染骨架并给出确定的错误，而不是在构建期抛异常。
final class FileUnavailableBackend implements FileBackend {
  const FileUnavailableBackend();

  @override
  bool get isConnected => false;

  Map<String, Object?> _fail() => const {
    'success': false,
    'error': {'code': -1, 'name': 'UNAVAILABLE', 'message': '原生文件后端不可用'},
  };

  @override
  Map<String, Object?> listDirectory(String remotePath) => _fail();

  @override
  Map<String, Object?> downloadFile(String remotePath, String localPath) => _fail();

  @override
  Map<String, Object?> uploadFile(String localPath, String remotePath) => _fail();

  @override
  Map<String, Object?> transferStatus() => _fail();

  @override
  Map<String, Object?> cancel() => _fail();
}
