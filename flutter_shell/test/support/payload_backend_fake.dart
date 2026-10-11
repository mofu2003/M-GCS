import 'dart:typed_data';

import 'package:mgcs_frontend/payload/payload_backend.dart';

/// 相机 / 云台 / 视频 FFI 的替身。
///
/// 读接口按注入的闭包返回脚本化 JSON（原生结构原样），命令接口统一返回
/// [commandResponse] 或抛出 [commandError]，并记录最后一次调用参数，供控制器
/// 与页面测试驱动「有设备 / 无设备 / 命令失败 / 断链 / 换会话」各分支。
class FakePayloadBackend implements PayloadBackend {
  FakePayloadBackend({this.connected = true});

  bool connected;

  @override
  bool get isConnected => connected;

  Map<String, Object?> Function(int cameraId)? onCameraStatus;
  Map<String, Object?> Function(int gimbalId)? onGimbalStatus;
  Map<String, Object?> Function(int gimbalId)? onGimbalAttitude;
  Map<String, Object?> Function()? onVideoStatus;
  Map<String, Object?> Function()? onVideoStreams;
  Map<String, Object?> Function()? onRendererStatus;

  /// 每次 [pollFrame] 依次弹出的帧；耗尽后返回 null（模拟「无新帧」）。
  final List<VideoFrameData> pollFrames = [];

  /// 命令类（拍照、变焦、云台指向、视频启动…）的返回值。
  Map<String, Object?> commandResponse = payloadOk();

  /// 非空时命令直接抛出，模拟 FFI 层异常。
  Object? commandError;

  final List<String> calls = [];
  int? lastCameraId;
  int? lastGimbalId;
  double? lastZoom;
  int? lastMode;
  ({double pitch, double yaw})? lastGimbalControl;
  String? lastVideoUri;

  Map<String, Object?> _command(String name) {
    calls.add(name);
    final failure = commandError;
    if (failure != null) throw failure;
    return commandResponse;
  }

  @override
  Map<String, Object?> cameraStatus(int cameraId) {
    calls.add('cameraStatus');
    lastCameraId = cameraId;
    return onCameraStatus?.call(cameraId) ?? payloadUnavailable();
  }

  @override
  Map<String, Object?> cameraRequestInfo(int cameraId) {
    lastCameraId = cameraId;
    return _command('cameraRequestInfo');
  }

  @override
  Map<String, Object?> cameraTakePhoto(int cameraId) {
    lastCameraId = cameraId;
    return _command('cameraTakePhoto');
  }

  @override
  Map<String, Object?> cameraStartVideo(int cameraId) {
    lastCameraId = cameraId;
    return _command('cameraStartVideo');
  }

  @override
  Map<String, Object?> cameraStopVideo(int cameraId) {
    lastCameraId = cameraId;
    return _command('cameraStopVideo');
  }

  @override
  Map<String, Object?> cameraStopCapture(int cameraId) {
    lastCameraId = cameraId;
    return _command('cameraStopCapture');
  }

  @override
  Map<String, Object?> cameraSetZoom(int cameraId, double zoomLevel) {
    lastCameraId = cameraId;
    lastZoom = zoomLevel;
    return _command('cameraSetZoom');
  }

  @override
  Map<String, Object?> cameraSetMode(int cameraId, int mode) {
    lastCameraId = cameraId;
    lastMode = mode;
    return _command('cameraSetMode');
  }

  @override
  Map<String, Object?> gimbalStatus(int gimbalId) {
    calls.add('gimbalStatus');
    lastGimbalId = gimbalId;
    return onGimbalStatus?.call(gimbalId) ?? payloadUnavailable();
  }

  @override
  Map<String, Object?> gimbalAttitude(int gimbalId) {
    calls.add('gimbalAttitude');
    lastGimbalId = gimbalId;
    return onGimbalAttitude?.call(gimbalId) ?? payloadUnavailable();
  }

  @override
  Map<String, Object?> gimbalRequestInfo() => _command('gimbalRequestInfo');

  @override
  Map<String, Object?> gimbalControl(double pitchDeg, double yawDeg) {
    lastGimbalControl = (pitch: pitchDeg, yaw: yawDeg);
    return _command('gimbalControl');
  }

  @override
  Map<String, Object?> gimbalRecenter() => _command('gimbalRecenter');

  @override
  Map<String, Object?> videoStatus() {
    calls.add('videoStatus');
    return onVideoStatus?.call() ?? payloadUnavailable();
  }

  @override
  Map<String, Object?> videoStreams() {
    calls.add('videoStreams');
    return onVideoStreams?.call() ??
        const {'success': true, 'count': 0, 'streams': <Object?>[]};
  }

  @override
  Map<String, Object?> videoStart(String uri) {
    lastVideoUri = uri;
    return _command('videoStart');
  }

  @override
  Map<String, Object?> videoStop() => _command('videoStop');

  @override
  Map<String, Object?> rendererStatus() {
    calls.add('rendererStatus');
    return onRendererStatus?.call() ?? rendererStatusResponse();
  }

  @override
  VideoFrameData? pollFrame() {
    calls.add('pollFrame');
    if (pollFrames.isEmpty) return null;
    return pollFrames.removeAt(0);
  }

  @override
  void dispose() {
    calls.add('dispose');
  }
}

/// `{"success":true}` 的标准成功响应。
Map<String, Object?> payloadOk() => const {'success': true};

/// 规范错误对象（`ffi-contract.md` F4）。
Map<String, Object?> payloadError(
  String name, {
  int code = -6,
  String message = '',
}) => {
  'success': false,
  'error': {
    'code': code,
    'name': name,
    'message': message.isEmpty ? name : message,
  },
};

/// `mg_camera_get_status` / `mg_gimbal_get_status` 的无设备响应（D5）。
Map<String, Object?> payloadUnavailable() => const {
  'success': true,
  'available': false,
  'count': 0,
  'cameras': <Object?>[],
  'gimbals': <Object?>[],
};

/// 相机清单摘要元素（对应 native `cameras[]`）。
Map<String, Object?> cameraDevice({
  required int componentId,
  int systemId = 1,
  int cameraDeviceId = 0,
  String vendorName = 'MG',
  String modelName = 'CAM',
  int flags = 3,
}) => {
  'system_id': systemId,
  'component_id': componentId,
  'camera_device_id': cameraDeviceId,
  'vendor_name': vendorName,
  'model_name': modelName,
  'flags': flags,
  'has_photo': flags & 0x01 != 0,
  'has_video': flags & 0x02 != 0,
};

/// 相机选中项详情字段（native 在 `available:true` 时附加的字段）。
Map<String, Object?> cameraSelected({
  required int componentId,
  int systemId = 1,
  int cameraDeviceId = 0,
  String vendorName = 'MG',
  String modelName = 'CAM',
  int flags = 3,
  int resolutionH = 4000,
  int resolutionV = 3000,
  double focalLength = 8,
  int modeId = 1,
  double zoomLevel = 1,
  bool recording = false,
  bool takingPhoto = false,
  int photoCount = 0,
  List<Map<String, Object?>> storage = const [],
}) => {
  'system_id': systemId,
  'component_id': componentId,
  'camera_device_id': cameraDeviceId,
  'vendor_name': vendorName,
  'model_name': modelName,
  'flags': flags,
  'resolution_h': resolutionH,
  'resolution_v': resolutionV,
  'focal_length': focalLength,
  'has_photo': flags & 0x01 != 0,
  'has_video': flags & 0x02 != 0,
  'mode_id': modeId,
  'zoom_level': zoomLevel,
  'recording': recording,
  'taking_photo': takingPhoto,
  'photo_count': photoCount,
  'image_status': takingPhoto ? 1 : 0,
  'video_status': recording ? 1 : 0,
  'storage': {'count': storage.length, 'entries': storage},
};

Map<String, Object?> cameraStatusResponse({
  List<Map<String, Object?>> devices = const [],
  Map<String, Object?>? selected,
}) => {
  'success': true,
  'available': selected != null,
  'count': devices.length,
  'cameras': devices,
  ...?selected,
};

Map<String, Object?> storageBucket({
  int storageId = 0,
  int status = 0,
  double totalCapacityMib = 1000,
  double usedCapacityMib = 250,
  double availableCapacityMib = 750,
  String name = 'SD',
}) => {
  'storage_id': storageId,
  'status': status,
  'total_capacity_mib': totalCapacityMib,
  'used_capacity_mib': usedCapacityMib,
  'available_capacity_mib': availableCapacityMib,
  'name': name,
};

/// 云台清单摘要元素（对应 native `gimbals[]`）。
Map<String, Object?> gimbalDevice({
  required int componentId,
  int systemId = 1,
  int gimbalDeviceId = 0,
  int capFlags = 0,
  String vendorName = 'MG',
  String modelName = 'GIMBAL',
}) => {
  'system_id': systemId,
  'component_id': componentId,
  'gimbal_device_id': gimbalDeviceId,
  'cap_flags': capFlags,
  'vendor_name': vendorName,
  'model_name': modelName,
};

Map<String, Object?> gimbalSelected({
  required int componentId,
  int systemId = 1,
  int gimbalDeviceId = 0,
  int capFlags = 0,
  double yawMin = -180,
  double yawMax = 180,
  double pitchMin = -90,
  double pitchMax = 30,
  String deviceVendorName = 'MG',
  String deviceModelName = 'GIMBAL',
}) => {
  'system_id': systemId,
  'component_id': componentId,
  'gimbal_device_id': gimbalDeviceId,
  'cap_flags': capFlags,
  'yaw_min': yawMin,
  'yaw_max': yawMax,
  'pitch_min': pitchMin,
  'pitch_max': pitchMax,
  'manager_flags': 0,
  'primary_control_sysid': 1,
  'primary_control_compid': 1,
  'device_vendor_name': deviceVendorName,
  'device_model_name': deviceModelName,
  'device_cap_flags': 0,
};

Map<String, Object?> gimbalStatusResponse({
  List<Map<String, Object?>> devices = const [],
  Map<String, Object?>? selected,
}) => {
  'success': true,
  'available': selected != null,
  'count': devices.length,
  'gimbals': devices,
  ...?selected,
};

Map<String, Object?> gimbalAttitudeResponse({
  bool available = true,
  int systemId = 1,
  int componentId = 154,
  int gimbalDeviceId = 0,
  double rollDeg = 0,
  double pitchDeg = -12.5,
  double yawDeg = 45,
}) => {
  'success': true,
  'available': available,
  'system_id': systemId,
  'component_id': componentId,
  'gimbal_device_id': gimbalDeviceId,
  'roll_deg': rollDeg,
  'pitch_deg': pitchDeg,
  'yaw_deg': yawDeg,
  'angular_velocity_x': 0.0,
  'angular_velocity_y': 0.0,
  'angular_velocity_z': 0.0,
  'failure_flags': 0,
  'flags': 0,
  'time_boot_ms': 1000,
};

Map<String, Object?> videoStream({
  int streamId = 0,
  int type = 0,
  String name = 'Front',
  String uri = 'rtsp://192.168.1.10:8554/main',
  bool running = false,
  int width = 1920,
  int height = 1080,
  double framerate = 30,
  int bitrate = 4000000,
}) => {
  'stream_id': streamId,
  'type': type,
  'name': name,
  'uri': uri,
  'running': running,
  'width': width,
  'height': height,
  'resolution_h': width,
  'resolution_v': height,
  'framerate': framerate,
  'bitrate': bitrate,
  'rotation': 0,
  'flags': running ? 1 : 0,
  'available': running,
};

Map<String, Object?> videoStatusResponse({
  List<Map<String, Object?>> streams = const [],
}) => {
  'success': true,
  'available': streams.isNotEmpty,
  'playing': false,
  'stream_count': streams.length,
  'streams': streams,
};

Map<String, Object?> videoStreamsResponse({
  List<Map<String, Object?>> streams = const [],
}) => {'success': true, 'count': streams.length, 'streams': streams};

/// `mg_video_renderer_status` 的响应。默认「没有可用后端」，与当前构建一致。
Map<String, Object?> rendererStatusResponse({
  bool available = false,
  String backend = 'none',
  bool running = false,
  String uri = '',
  bool hasFrame = false,
  int width = 0,
  int height = 0,
  int ptsUs = 0,
  int sequence = 0,
}) => {
  'success': true,
  'available': available,
  'backend': backend,
  'running': running,
  'uri': uri,
  'has_frame': hasFrame,
  'width': width,
  'height': height,
  'pts_us': ptsUs,
  'sequence': sequence,
};

/// 一帧合成 RGBA8888（纯色），供控制器/预览测试驱动，不依赖真实解码后端。
VideoFrameData renderedFrame({
  int width = 4,
  int height = 4,
  int sequence = 1,
  int ptsUs = 0,
}) {
  final rgba = Uint8List(width * height * 4);
  for (var i = 0; i < width * height; i++) {
    rgba[i * 4] = 10;
    rgba[i * 4 + 1] = 20;
    rgba[i * 4 + 2] = 30;
    rgba[i * 4 + 3] = 255;
  }
  return VideoFrameData(
    width: width,
    height: height,
    rgba: rgba,
    ptsUs: ptsUs,
    sequence: sequence,
  );
}
