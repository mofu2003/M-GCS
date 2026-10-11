import 'dart:typed_data';

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

  /// 启动本地视频源。没有编译进解码后端时 native 返回 `NOT_SUPPORTED`。
  Map<String, Object?> videoStart(String uri);
  Map<String, Object?> videoStop();

  /// 本机解码后端状态（`mg_video_renderer_status`）。即使未连接飞控也可读：
  /// 它描述的是本机后端，不是飞控能力。
  Map<String, Object?> rendererStatus();

  /// 开始把正在播放的本地码流写入 [path]（码流复制，不重编码）。没有解码后端
  /// 返回 `NOT_SUPPORTED`，没有正在运行的本地源返回 `INVALID_STATE`，
  /// 已在录制返回 `BUSY`，打开文件失败返回 `SEND_FAILED`。
  Map<String, Object?> videoStartRecording(String path);

  /// 停止本地录制并返回最终状态快照。未在录制时是无害的成功 no-op。
  Map<String, Object?> videoStopRecording();

  /// 本机录制状态（`mg_video_recording_status`）。始终是可读的成功信封。
  Map<String, Object?> videoRecordingStatus();

  /// 把最新一帧存成图片（`.png`/`.jpg`）。没有解码后端返回 `NOT_SUPPORTED`，
  /// 还没有解码出帧返回 `INVALID_STATE`，写文件失败返回 `SEND_FAILED`。
  Map<String, Object?> videoCaptureFrame(String path);

  /// 取本机解码器的最新一帧（RGBA8888）。没有新帧或没有可用后端时返回 null，
  /// 不阻塞、不抛异常。像素已拷进 Dart 侧的 [VideoFrameData.rgba]。
  VideoFrameData? pollFrame();

  /// 释放后端持有的原生资源（例如帧缓冲）。重复调用安全。
  void dispose();
}

/// 一帧 RGBA8888 像素与元数据。`rgba` 长度为 `width * height * 4`，行间无填充。
final class VideoFrameData {
  const VideoFrameData({
    required this.width,
    required this.height,
    required this.rgba,
    this.ptsUs = 0,
    this.sequence = 0,
  });

  final int width;
  final int height;
  final Uint8List rgba;
  final int ptsUs;
  final int sequence;
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

  @override
  Map<String, Object?> videoStartRecording(String path) => _fail();

  @override
  Map<String, Object?> videoStopRecording() => _fail();

  @override
  Map<String, Object?> videoCaptureFrame(String path) => _fail();

  /// 原生库不可用时，本机解码后端同样「不可用」——这是一个合法的能力状态，
  /// 不是错误，所以返回 `available:false` 而不是失败对象，避免污染页面错误区。
  @override
  Map<String, Object?> rendererStatus() => const {
    'success': true,
    'available': false,
    'backend': 'none',
    'running': false,
    'uri': '',
    'has_frame': false,
    'width': 0,
    'height': 0,
    'pts_us': 0,
    'sequence': 0,
  };

  /// 同理，录制能力也随解码后端一起缺失；返回可读的空状态而不是错误。
  @override
  Map<String, Object?> videoRecordingStatus() => const {
    'success': true,
    'available': false,
    'recording': false,
    'path': '',
    'error': '',
    'bytes': 0,
    'duration_us': 0,
  };

  @override
  VideoFrameData? pollFrame() => null;

  @override
  void dispose() {}
}
