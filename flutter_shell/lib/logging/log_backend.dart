/// 机载日志 domain 的原生边界。
///
/// 只暴露 `mg_onboard_log_*` 导出；页面与控制器通过它访问原生层，
/// 便于用替身测试下载状态机（正常、取消、写失败、断链）。
abstract interface class LogBackend {
  /// 当前连接是否可用于发起请求（native 侧还有一道 `CHECK_CONNECTED`）。
  bool get isConnected;

  /// 请求飞控回传日志列表（`LOG_REQUEST_LIST`）。
  Map<String, Object?> requestList();

  /// 读取已收到的日志列表（`LOG_ENTRY` 累积结果）。
  Map<String, Object?> entries();

  /// 发起一次日志下载，落到 [filepath]。
  Map<String, Object?> download(int logId, String filepath);

  /// 取消正在进行的下载（发 `LOG_REQUEST_END`）。
  Map<String, Object?> cancel();

  /// 读取当前下载状态。这个查询是纯读取，没有副作用。
  Map<String, Object?> status();

  /// 请求飞控擦除全部机载日志（`LOG_ERASE`）。下载进行中会被拒绝。
  Map<String, Object?> eraseAll();
}

/// 原生库不可用时的空实现。
///
/// 让日志页在 `initializationError != null`（例如 flutter_tester 没有原生库）
/// 时仍能渲染骨架并给出确定的错误，而不是在构建期抛异常。
final class LogUnavailableBackend implements LogBackend {
  const LogUnavailableBackend();

  @override
  bool get isConnected => false;

  Map<String, Object?> _fail() => const {
    'success': false,
    'error': {'code': -1, 'name': 'UNAVAILABLE', 'message': '原生日志后端不可用'},
  };

  @override
  Map<String, Object?> requestList() => _fail();

  @override
  Map<String, Object?> entries() => _fail();

  @override
  Map<String, Object?> download(int logId, String filepath) => _fail();

  @override
  Map<String, Object?> cancel() => _fail();

  @override
  Map<String, Object?> status() => _fail();

  @override
  Map<String, Object?> eraseAll() => _fail();
}
