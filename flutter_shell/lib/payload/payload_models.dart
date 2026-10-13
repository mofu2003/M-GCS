import '../mission/mission_document_models.dart';

/// 一台被发现的相机。字段与 native `mg_camera_get_status` 的 `cameras[]`
/// 摘要一一对应；`componentId` 就是后续读接口/命令接口的 `camera_id`。
///
/// native 按组件 id（`MAV_COMP_ID_CAMERA` = 100 起）寻址相机，所以前端用
/// `componentId`、而不是列表下标来选择设备。
final class CameraDevice {
  const CameraDevice({
    required this.systemId,
    required this.componentId,
    required this.cameraDeviceId,
    this.vendorName = '',
    this.modelName = '',
    this.flags = 0,
    this.hasPhoto = false,
    this.hasVideo = false,
  });

  final int systemId;
  final int componentId;
  final int cameraDeviceId;
  final String vendorName;
  final String modelName;
  final int flags;
  final bool hasPhoto;
  final bool hasVideo;

  factory CameraDevice.fromJson(JsonMap json) => CameraDevice(
    systemId: json['system_id'] as int? ?? 0,
    componentId: json['component_id'] as int? ?? 0,
    cameraDeviceId: json['camera_device_id'] as int? ?? 0,
    vendorName: json['vendor_name'] as String? ?? '',
    modelName: json['model_name'] as String? ?? '',
    flags: json['flags'] as int? ?? 0,
    hasPhoto: json['has_photo'] == true,
    hasVideo: json['has_video'] == true,
  );

  /// 列表展示名：优先厂商 + 型号，缺失时退回「相机 <组件 id>」。
  String get displayName {
    final model = modelName.trim();
    final vendor = vendorName.trim();
    final joined = [vendor, model].where((s) => s.isNotEmpty).join(' ');
    return joined.isEmpty ? '相机 #$componentId' : joined;
  }
}

/// 相机的一个存储桶（`STORAGE_INFORMATION`）。一台相机可能有多个。
final class CameraStorageBucket {
  const CameraStorageBucket({
    required this.storageId,
    required this.status,
    required this.totalCapacityMib,
    required this.usedCapacityMib,
    required this.availableCapacityMib,
    this.name = '',
  });

  final int storageId;
  final int status;
  final double totalCapacityMib;
  final double usedCapacityMib;
  final double availableCapacityMib;
  final String name;

  factory CameraStorageBucket.fromJson(JsonMap json) => CameraStorageBucket(
    storageId: json['storage_id'] as int? ?? 0,
    status: json['status'] as int? ?? 0,
    totalCapacityMib: (json['total_capacity_mib'] as num?)?.toDouble() ?? 0,
    usedCapacityMib: (json['used_capacity_mib'] as num?)?.toDouble() ?? 0,
    availableCapacityMib:
        (json['available_capacity_mib'] as num?)?.toDouble() ?? 0,
    name: json['name'] as String? ?? '',
  );

  /// 已用占比（0..1）；总容量为 0 时返回 null，避免显示无意义的百分比。
  double? get usedFraction => totalCapacityMib <= 0
      ? null
      : (usedCapacityMib / totalCapacityMib).clamp(0.0, 1.0);
}

/// `mg_camera_get_status` 的解析结果。
///
/// `available == false` 表示「已查询、当前没有这台相机」，是合法状态；
/// `devices` 是本次查询发现的全部相机，用于列表与多设备选择。
final class CameraStatus {
  const CameraStatus({
    required this.available,
    this.devices = const [],
    this.systemId,
    this.componentId,
    this.cameraDeviceId,
    this.vendorName = '',
    this.modelName = '',
    this.resolutionH = 0,
    this.resolutionV = 0,
    this.focalLength = 0,
    this.hasPhoto = false,
    this.hasVideo = false,
    this.modeId = -1,
    this.zoomLevel = 1,
    this.recording = false,
    this.takingPhoto = false,
    this.photoCount = 0,
    this.imageStatus = 0,
    this.videoStatus = 0,
    this.availableCapacityMib = 0,
    this.storage = const [],
  });

  final bool available;
  final List<CameraDevice> devices;
  final int? systemId;
  final int? componentId;
  final int? cameraDeviceId;
  final String vendorName;
  final String modelName;
  final int resolutionH;
  final int resolutionV;
  final double focalLength;
  final bool hasPhoto;
  final bool hasVideo;
  final int modeId;
  final double zoomLevel;
  final bool recording;
  final bool takingPhoto;
  final int photoCount;
  final int imageStatus;
  final int videoStatus;
  final double availableCapacityMib;
  final List<CameraStorageBucket> storage;

  int get count => devices.length;

  factory CameraStatus.fromJson(JsonMap json) {
    final devices = [
      for (final raw in (json['cameras'] as List<Object?>? ?? const []))
        if (raw is JsonMap) CameraDevice.fromJson(raw),
    ];
    final available = json['available'] == true;
    return CameraStatus(
      available: available,
      devices: devices,
      systemId: json['system_id'] as int?,
      componentId: json['component_id'] as int?,
      cameraDeviceId: json['camera_device_id'] as int?,
      vendorName: json['vendor_name'] as String? ?? '',
      modelName: json['model_name'] as String? ?? '',
      resolutionH: json['resolution_h'] as int? ?? 0,
      resolutionV: json['resolution_v'] as int? ?? 0,
      focalLength: (json['focal_length'] as num?)?.toDouble() ?? 0,
      hasPhoto: json['has_photo'] == true,
      hasVideo: json['has_video'] == true,
      modeId: json['mode_id'] as int? ?? -1,
      zoomLevel: (json['zoom_level'] as num?)?.toDouble() ?? 1,
      recording: json['recording'] == true,
      takingPhoto: json['taking_photo'] == true,
      photoCount: json['photo_count'] as int? ?? 0,
      imageStatus: json['image_status'] as int? ?? 0,
      videoStatus: json['video_status'] as int? ?? 0,
      availableCapacityMib:
          (json['available_capacity_mib'] as num?)?.toDouble() ?? 0,
      storage: _parseStorage(json['storage']),
    );
  }

  static List<CameraStorageBucket> _parseStorage(Object? raw) {
    if (raw is! JsonMap) return const [];
    final entries = raw['entries'];
    if (entries is! List) return const [];
    return [
      for (final item in entries)
        if (item is JsonMap) CameraStorageBucket.fromJson(item),
    ];
  }
}

/// 一台被发现的云台管理器（`GIMBAL_MANAGER_INFORMATION` 摘要）。
///
/// 与相机一样，`componentId` 是后续读接口的 `gimbal_id`。
final class GimbalDevice {
  const GimbalDevice({
    required this.systemId,
    required this.componentId,
    required this.gimbalDeviceId,
    this.capFlags = 0,
    this.vendorName = '',
    this.modelName = '',
  });

  final int systemId;
  final int componentId;
  final int gimbalDeviceId;
  final int capFlags;
  final String vendorName;
  final String modelName;

  factory GimbalDevice.fromJson(JsonMap json) => GimbalDevice(
    systemId: json['system_id'] as int? ?? 0,
    componentId: json['component_id'] as int? ?? 0,
    gimbalDeviceId: json['gimbal_device_id'] as int? ?? 0,
    capFlags: json['cap_flags'] as int? ?? 0,
    vendorName: json['vendor_name'] as String? ?? '',
    modelName: json['model_name'] as String? ?? '',
  );

  String get displayName {
    final model = modelName.trim();
    final vendor = vendorName.trim();
    final joined = [vendor, model].where((s) => s.isNotEmpty).join(' ');
    return joined.isEmpty ? '云台 #$componentId' : joined;
  }
}

/// `mg_gimbal_get_status` 的解析结果。`available == false` 同相机语义。
final class GimbalStatus {
  const GimbalStatus({
    required this.available,
    this.devices = const [],
    this.systemId,
    this.componentId,
    this.gimbalDeviceId,
    this.capFlags = 0,
    this.yawMin = 0,
    this.yawMax = 0,
    this.pitchMin = 0,
    this.pitchMax = 0,
    this.managerFlags = 0,
    this.primaryControlSysid = 0,
    this.primaryControlCompid = 0,
    this.deviceVendorName = '',
    this.deviceModelName = '',
    this.deviceCapFlags = 0,
  });

  final bool available;
  final List<GimbalDevice> devices;
  final int? systemId;
  final int? componentId;
  final int? gimbalDeviceId;
  final int capFlags;
  final double yawMin;
  final double yawMax;
  final double pitchMin;
  final double pitchMax;
  final int managerFlags;
  final int primaryControlSysid;
  final int primaryControlCompid;
  final String deviceVendorName;
  final String deviceModelName;
  final int deviceCapFlags;

  int get count => devices.length;

  factory GimbalStatus.fromJson(JsonMap json) => GimbalStatus(
    available: json['available'] == true,
    devices: [
      for (final raw in (json['gimbals'] as List<Object?>? ?? const []))
        if (raw is JsonMap) GimbalDevice.fromJson(raw),
    ],
    systemId: json['system_id'] as int?,
    componentId: json['component_id'] as int?,
    gimbalDeviceId: json['gimbal_device_id'] as int?,
    capFlags: json['cap_flags'] as int? ?? 0,
    yawMin: (json['yaw_min'] as num?)?.toDouble() ?? 0,
    yawMax: (json['yaw_max'] as num?)?.toDouble() ?? 0,
    pitchMin: (json['pitch_min'] as num?)?.toDouble() ?? 0,
    pitchMax: (json['pitch_max'] as num?)?.toDouble() ?? 0,
    managerFlags: json['manager_flags'] as int? ?? 0,
    primaryControlSysid: json['primary_control_sysid'] as int? ?? 0,
    primaryControlCompid: json['primary_control_compid'] as int? ?? 0,
    deviceVendorName: json['device_vendor_name'] as String? ?? '',
    deviceModelName: json['device_model_name'] as String? ?? '',
    deviceCapFlags: json['device_cap_flags'] as int? ?? 0,
  );
}

/// `mg_gimbal_get_attitude` 的解析结果。native 已把四元数换算成欧拉角。
final class GimbalAttitude {
  const GimbalAttitude({
    required this.available,
    this.systemId,
    this.componentId,
    this.gimbalDeviceId,
    this.rollDeg = 0,
    this.pitchDeg = 0,
    this.yawDeg = 0,
    this.angularVelocityX = 0,
    this.angularVelocityY = 0,
    this.angularVelocityZ = 0,
    this.failureFlags = 0,
    this.timeBootMs = 0,
  });

  final bool available;
  final int? systemId;
  final int? componentId;
  final int? gimbalDeviceId;
  final double rollDeg;
  final double pitchDeg;
  final double yawDeg;
  final double angularVelocityX;
  final double angularVelocityY;
  final double angularVelocityZ;
  final int failureFlags;
  final int timeBootMs;

  factory GimbalAttitude.fromJson(JsonMap json) => GimbalAttitude(
    available: json['available'] == true,
    systemId: json['system_id'] as int?,
    componentId: json['component_id'] as int?,
    gimbalDeviceId: json['gimbal_device_id'] as int?,
    rollDeg: (json['roll_deg'] as num?)?.toDouble() ?? 0,
    pitchDeg: (json['pitch_deg'] as num?)?.toDouble() ?? 0,
    yawDeg: (json['yaw_deg'] as num?)?.toDouble() ?? 0,
    angularVelocityX: (json['angular_velocity_x'] as num?)?.toDouble() ?? 0,
    angularVelocityY: (json['angular_velocity_y'] as num?)?.toDouble() ?? 0,
    angularVelocityZ: (json['angular_velocity_z'] as num?)?.toDouble() ?? 0,
    failureFlags: json['failure_flags'] as int? ?? 0,
    timeBootMs: (json['time_boot_ms'] as num?)?.toInt() ?? 0,
  );
}

/// 一路视频流的元数据。`mg_video_status`（`streams[]`）与
/// `mg_video_get_streams`（更完整字段）合并到同一个模型，缺省字段保持 0/空。
final class VideoStreamInfo {
  const VideoStreamInfo({
    required this.streamId,
    this.systemId = 0,
    this.componentId = 0,
    this.type = 0,
    this.name = '',
    this.uri = '',
    this.running = false,
    this.available = false,
    this.width = 0,
    this.height = 0,
    this.framerate = 0,
    this.bitrate = 0,
    this.rotation = 0,
    this.flags = 0,
  });

  final int streamId;
  final int systemId;
  final int componentId;
  final int type;
  final String name;
  final String uri;
  final bool running;
  final bool available;
  final int width;
  final int height;
  final double framerate;
  final int bitrate;
  final int rotation;
  final int flags;

  factory VideoStreamInfo.fromJson(JsonMap json) => VideoStreamInfo(
    streamId: json['stream_id'] as int? ?? 0,
    systemId: json['system_id'] as int? ?? 0,
    componentId: json['component_id'] as int? ?? 0,
    type: json['type'] as int? ?? 0,
    name: json['name'] as String? ?? '',
    uri: json['uri'] as String? ?? '',
    running: json['running'] == true,
    available: json['available'] == true,
    width: json['width'] as int? ?? (json['resolution_h'] as int? ?? 0),
    height: json['height'] as int? ?? (json['resolution_v'] as int? ?? 0),
    framerate: (json['framerate'] as num?)?.toDouble() ?? 0,
    bitrate: (json['bitrate'] as num?)?.toInt() ?? 0,
    rotation: json['rotation'] as int? ?? 0,
    flags: json['flags'] as int? ?? 0,
  );

  String get displayName =>
      name.trim().isNotEmpty ? name.trim() : '视频流 #$streamId';

  /// MAVLink `VIDEO_STREAM_TYPE` 的可读标签；未识别时给原始数值。
  static String typeLabel(int type) => switch (type) {
    0 => 'RTSP',
    1 => 'RTPUDP',
    2 => 'RTMP',
    3 => 'WebRTC',
    _ => '类型 $type',
  };
}

/// `mg_video_status` 的解析结果。
///
/// `playing` 只有本地解码后端落地（S3）后才可能为 true；S1 起 native 恒返回
/// false，页面据此明确显示「本地播放未支持」。
final class VideoStatus {
  const VideoStatus({
    required this.available,
    this.playing = false,
    this.streams = const [],
  });

  final bool available;
  final bool playing;
  final List<VideoStreamInfo> streams;

  factory VideoStatus.fromJson(JsonMap json) => VideoStatus(
    available: json['available'] == true,
    playing: json['playing'] == true,
    streams: [
      for (final raw in (json['streams'] as List<Object?>? ?? const []))
        if (raw is JsonMap) VideoStreamInfo.fromJson(raw),
    ],
  );
}

/// `mg_video_renderer_status` 的解析结果：本机（native core 内）解码后端的
/// 可用性、运行状态与最新帧元数据。
///
/// 与 [VideoStatus] 的区别：[VideoStatus] 描述飞控上报的**远端视频流**；
/// 本模型描述**本机解码器**。`available:false`（`backend:"none"`）表示这个
/// 构建没编译进解码后端，页面据此如实说明「本平台未启用本地解码」，而不是
/// 把「没有流」伪装成「未播放」。
final class VideoRendererStatus {
  const VideoRendererStatus({
    required this.available,
    this.backend = 'none',
    this.running = false,
    this.uri = '',
    this.hasFrame = false,
    this.width = 0,
    this.height = 0,
    this.ptsUs = 0,
    this.sequence = 0,
  });

  final bool available;
  final String backend;
  final bool running;
  final String uri;
  final bool hasFrame;
  final int width;
  final int height;
  final int ptsUs;
  final int sequence;

  factory VideoRendererStatus.fromJson(JsonMap json) => VideoRendererStatus(
    available: json['available'] == true,
    backend: json['backend'] as String? ?? 'none',
    running: json['running'] == true,
    uri: json['uri'] as String? ?? '',
    hasFrame: json['has_frame'] == true,
    width: json['width'] as int? ?? 0,
    height: json['height'] as int? ?? 0,
    ptsUs: (json['pts_us'] as num?)?.toInt() ?? 0,
    sequence: (json['sequence'] as num?)?.toInt() ?? 0,
  );
}

/// 本机录制状态（`mg_video_recording_status` / start/stop 的返回体）。
///
/// 与飞控无关：它描述的是本机解码器把码流写进本地文件的状态。
/// `available:false` 表示这个构建没有编译进解码后端，是合法能力状态而非错误。
final class VideoRecordingStatus {
  const VideoRecordingStatus({
    required this.available,
    this.recording = false,
    this.path = '',
    this.error = '',
    this.bytes = 0,
    this.durationUs = 0,
  });

  final bool available;
  final bool recording;
  final String path;

  /// 最近一次录制失败的说明；正常停止后为空。录制中通常也为空。
  final String error;
  final int bytes;
  final int durationUs;

  factory VideoRecordingStatus.fromJson(JsonMap json) => VideoRecordingStatus(
    available: json['available'] == true,
    recording: json['recording'] == true,
    path: json['path'] as String? ?? '',
    error: json['error'] as String? ?? '',
    bytes: (json['bytes'] as num?)?.toInt() ?? 0,
    durationUs: (json['duration_us'] as num?)?.toInt() ?? 0,
  );
}
