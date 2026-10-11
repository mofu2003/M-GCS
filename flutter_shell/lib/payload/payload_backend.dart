/// 相机 / 云台 / 视频的原生边界。
///
/// 只暴露 `mg_camera_*`、`mg_gimbal_*`、`mg_video_*`、`mg_command_gimbal_*`
/// 导出；页面与控制器通过它访问原生层，便于用替身测试「无设备 / 有设备 /
/// 命令失败 / 断链」各分支。
///
/// 读接口（`cameraStatus`/`gimbalStatus`/`gimbalAttitude`/`videoStatus`）
/// 返回原生 JSON 解析结果；命令接口返回统一的 `{success:...}` 或规范错误对象。
abstract interface class PayloadBackend {
  /// 当前连接是否可用于发起请求（native 侧还有一道 `CHECK_CONNECTED`）。
  bool get isConnected;

  /// 相机状态 + 发现到的全部相机。`cameraId <= 0` 表示活动目标/第一台。
  Map<String, Object?> cameraStatus(int cameraId);

  Map<String, Object?> cameraRequestInfo(int cameraId);
  Map<String, Object?> cameraTakePhoto(int cameraId);
  Map<String, Object?> cameraStartVideo(int cameraId);
  Map<String, Object?> cameraStopVideo(int cameraId);
  Map<String, Object?> cameraStopCapture(int cameraId);
  Map<String, Object?> cameraSetZoom(int cameraId, double zoomLevel);
  Map<String, Object?> cameraSetMode(int cameraId, int mode);

  /// 云台状态 + 发现到的全部云台。`gimbalId <= 0` 表示活动目标/第一台。
  Map<String, Object?> gimbalStatus(int gimbalId);
  Map<String, Object?> gimbalAttitude(int gimbalId);

  Map<String, Object?> gimbalRequestInfo();

  /// 云台指向：pitch/yaw 单位为度。命令发往当前飞机的云台管理器。
  Map<String, Object?> gimbalControl(double pitchDeg, double yawDeg);
  Map<String, Object?> gimbalRecenter();

  /// 视频流元数据（含 `VIDEO_STREAM_STATUS` 配对字段）。
  Map<String, Object?> videoStatus();

  /// 更完整的视频流定义（`VIDEO_STREAM_INFORMATION`）。未连接返回错误对象。
  Map<String, Object?> videoStreams();

  /// 启动本地视频源。S3 前 native 恒返回 `NOT_SUPPORTED`。
  Map<String, Object?> videoStart(String uri);
  Map<String, Object?> videoStop();
}

/// 原生库不可用时的降级实现：明确报错，不静默返回空结果。
///
/// 让相机/云台页在 `initializationError != null`（例如 flutter_tester 没有原生库）
/// 时仍能渲染骨架并给出确定的错误，而不是在构建期抛异常。
final class PayloadUnavailableBackend implements PayloadBackend {
  const PayloadUnavailableBackend();

  static const Map<String, Object?> _failure = {
    'success': false,
    'error': {'code': -1, 'name': 'UNAVAILABLE', 'message': '原生相机/云台后端不可用'},
  };

  @override
  bool get isConnected => false;

  Map<String, Object?> _fail() => _failure;

  @override
  Map<String, Object?> cameraStatus(int cameraId) => _fail();

  @override
  Map<String, Object?> cameraRequestInfo(int cameraId) => _fail();

  @override
  Map<String, Object?> cameraTakePhoto(int cameraId) => _fail();

  @override
  Map<String, Object?> cameraStartVideo(int cameraId) => _fail();

  @override
  Map<String, Object?> cameraStopVideo(int cameraId) => _fail();

  @override
  Map<String, Object?> cameraStopCapture(int cameraId) => _fail();

  @override
  Map<String, Object?> cameraSetZoom(int cameraId, double zoomLevel) => _fail();

  @override
  Map<String, Object?> cameraSetMode(int cameraId, int mode) => _fail();

  @override
  Map<String, Object?> gimbalStatus(int gimbalId) => _fail();

  @override
  Map<String, Object?> gimbalAttitude(int gimbalId) => _fail();

  @override
  Map<String, Object?> gimbalRequestInfo() => _fail();

  @override
  Map<String, Object?> gimbalControl(double pitchDeg, double yawDeg) => _fail();

  @override
  Map<String, Object?> gimbalRecenter() => _fail();

  @override
  Map<String, Object?> videoStatus() => _fail();

  @override
  Map<String, Object?> videoStreams() => _fail();

  @override
  Map<String, Object?> videoStart(String uri) => _fail();

  @override
  Map<String, Object?> videoStop() => _fail();
}
