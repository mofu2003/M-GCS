import 'dart:async';
import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:latlong2/latlong.dart';

import '../ffi_response.dart';
import '../fleet/fleet_controller.dart';
import '../fleet/fleet_ffi_backend.dart';
import '../fleet/fleet_models.dart';
import '../logging/file_backend.dart';
import '../logging/file_controller.dart';
import '../logging/file_ffi_backend.dart';
import '../logging/log_backend.dart';
import '../logging/log_controller.dart';
import '../logging/log_ffi_backend.dart';
import '../logging/tlog_backend.dart';
import '../logging/tlog_controller.dart';
import '../logging/tlog_ffi_backend.dart';
import '../mission/mission_document_controller.dart';
import '../mission/mission_geo.dart';
import '../mission/mission_document_ffi_backend.dart';
import '../mission/mission_document_models.dart';
import '../payload/payload_backend.dart';
import '../payload/payload_controller.dart';
import '../payload/payload_ffi_backend.dart';
import '../terrain/elevation_provider.dart';
import '../terrain/terrain_backend.dart';
import '../terrain/terrain_controller.dart';
import '../terrain/terrain_ffi_backend.dart';
import '../survey/survey_backend.dart';
import '../survey/survey_controller.dart';
import '../survey/survey_ffi_backend.dart';
import 'telemetry_ffi_backend.dart';
import 'parameter_ffi_backend.dart';
import 'calibration_ffi_backend.dart';
import 'flight_command_ffi_backend.dart';
import 'native_core.dart';
import 'signing_controller.dart';
import 'signing_key_store.dart';

final class SerialPortInfo {
  const SerialPortInfo({
    required this.path,
    required this.description,
    required this.likelyFlightController,
  });

  final String path;
  final String description;
  final bool likelyFlightController;
}

/// 波形选择器里的两类实例：IMU（加速度计/陀螺仪）和磁力计。
enum WaveformInstanceKind { imu, mag }

enum CalibrationType {
  gyroscope,
  accelerometer,
  magnetometer,
  barometer,
  radio,
}

enum AccelerometerCalibrationMode { sixFace, level, simple }

final class CalibrationStatus {
  const CalibrationStatus({
    this.active = false,
    this.cancelling = false,
    this.type = CalibrationType.gyroscope,
    this.stage = 'idle',
    this.progress = 0,
    this.currentStep = 0,
    this.totalSteps = 0,
    this.message = '',
    this.failureReason = '',
    this.accelerometerMode = '',
    this.compassProgress = const {},
  });

  final bool active;

  /// 取消请求已发出、正在等 native 返回。此时 [active] 仍为 true：在飞控
  /// 确认停止之前界面不得显示“已停止”（safety.md 的 S8）。
  final bool cancelling;

  final CalibrationType type;
  final String stage;
  final int progress;
  final int currentStep;
  final int totalSteps;
  final String message;
  final String failureReason;
  final String accelerometerMode;

  /// 分罗盘进度：compass_id（0 起）→ completion_pct。
  ///
  /// [progress] 是所有罗盘的最大值；界面要按罗盘各画一条进度条时读这张表。
  final Map<int, int> compassProgress;

  bool get isAccelerometer => type == CalibrationType.accelerometer;
  bool get isWaitingOrientation => stage == 'waiting_orientation';

  Map<String, Object?> toJson() => {
    'active': active,
    'cancelling': cancelling,
    'type': type.name,
    'stage': stage,
    'progress': progress,
    'currentStep': currentStep,
    'totalSteps': totalSteps,
    'message': message,
    'failureReason': failureReason,
    'accelerometerMode': accelerometerMode,
    // jsonEncode requires string keys. Keep the in-memory map integer-keyed
    // for direct compass_id lookup, but serialize it with JSON-safe keys.
    'compassProgress': {
      for (final entry in compassProgress.entries)
        entry.key.toString(): entry.value,
    },
  };
}

/// 后端识别到的一个传感器实例。
final class WaveformInstance {
  const WaveformInstance({
    required this.index,
    required this.active,
    required this.name,
  });

  /// 实例号，0 对应 IMU1 / 磁力计 1，直接作为取数时的 instance 参数。
  final int index;

  /// 最近是否真的收到过这个实例的数据。
  final bool active;

  /// 解码出的传感器型号，参数未下发时是空串。
  final String name;

  /// 选择器上的显示名，例如 `IMU 1`。
  String label(String prefix) => '$prefix ${index + 1}';

  @override
  bool operator ==(Object other) =>
      other is WaveformInstance &&
      other.index == index &&
      other.active == active &&
      other.name == name;

  @override
  int get hashCode => Object.hash(index, active, name);
}

final class ParameterOption {
  const ParameterOption({required this.value, required this.label});

  final double value;
  final String label;
}

/// 飞控上报的一个位置点：WGS84 经纬度 + 海拔，航向可选（Home 点没有）。
final class VehicleFix {
  const VehicleFix({
    required this.latitude,
    required this.longitude,
    this.altitudeMeters,
    this.headingDegrees,
  });

  final double latitude;
  final double longitude;
  final double? altitudeMeters;
  final double? headingDegrees;

  Map<String, Object?> toJson() => {
    'latitude': latitude,
    'longitude': longitude,
    'altitudeMeters': altitudeMeters,
    'headingDegrees': headingDegrees,
  };
}

final class VehicleParameter {
  const VehicleParameter({
    required this.name,
    required this.value,
    required this.mavlinkType,
    this.index,
    this.count,
    this.displayName,
    this.description,
    this.group,
    this.valueType,
    this.unit,
    this.minimum,
    this.maximum,
    this.increment,
    this.requiresReboot = false,
    this.readOnly = false,
    this.options = const [],
    this.valid = true,
  });

  final String name;
  final double value;
  final int mavlinkType;
  final int? index;
  final int? count;
  final String? displayName;
  final String? description;
  final String? group;
  final String? valueType;
  final String? unit;
  final double? minimum;
  final double? maximum;
  final double? increment;
  final bool requiresReboot;
  final bool readOnly;
  final List<ParameterOption> options;
  final bool valid;

  VehicleParameter copyWith({
    double? value,
    String? displayName,
    String? description,
    String? group,
    String? valueType,
    String? unit,
    double? minimum,
    double? maximum,
    double? increment,
    bool? requiresReboot,
    bool? readOnly,
    List<ParameterOption>? options,
    bool? valid,
  }) => VehicleParameter(
    name: name,
    value: value ?? this.value,
    mavlinkType: mavlinkType,
    index: index,
    count: count,
    displayName: displayName ?? this.displayName,
    description: description ?? this.description,
    group: group ?? this.group,
    valueType: valueType ?? this.valueType,
    unit: unit ?? this.unit,
    minimum: minimum ?? this.minimum,
    maximum: maximum ?? this.maximum,
    increment: increment ?? this.increment,
    requiresReboot: requiresReboot ?? this.requiresReboot,
    readOnly: readOnly ?? this.readOnly,
    options: options ?? this.options,
    valid: valid ?? this.valid,
  );

  factory VehicleParameter.fromTelemetry(Map<String, Object?> json) =>
      VehicleParameter(
        name: json['name'] as String? ?? '',
        value: (json['value'] as num?)?.toDouble() ?? double.nan,
        mavlinkType: json['type'] as int? ?? 0,
        index: json['index'] as int?,
        count: json['count'] as int?,
      );

  VehicleParameter withDescriptor(Map<String, Object?> json) {
    final optionData = json['options'] as List<Object?>? ?? const [];
    final parsedOptions = optionData
        .whereType<Map<String, Object?>>()
        .where((option) => option['value'] is num)
        .map(
          (option) => ParameterOption(
            value: (option['value'] as num).toDouble(),
            label:
                option['label'] as String? ??
                option['english_label'] as String? ??
                '${option['value']}',
          ),
        )
        .toList(growable: false);
    return copyWith(
      displayName: json['display_name'] as String?,
      description: json['description'] as String?,
      group: json['group'] as String?,
      valueType: json['type'] as String?,
      unit: json['unit'] as String?,
      minimum: (json['min'] as num?)?.toDouble(),
      maximum: (json['max'] as num?)?.toDouble(),
      increment: (json['increment'] as num?)?.toDouble(),
      requiresReboot: json['requires_reboot'] as bool? ?? false,
      readOnly: json['read_only'] as bool? ?? false,
      options: parsedOptions,
      valid: json['valid'] as bool? ?? valid,
    );
  }
}

enum SerialConnectionPhase { disconnected, waitingForHeartbeat, connected }

/// 飞控消息（STATUSTEXT）条目。
class FlightMessage {
  const FlightMessage({
    required this.seq,
    required this.time,
    required this.severity,
    required this.severityName,
    required this.text,
  });

  final int seq;
  final DateTime? time;

  /// MAV_SEVERITY：0 emergency … 4 warning … 6 info、7 debug。
  final int severity;
  final String severityName;
  final String text;
}

enum ConnectionTransport { serial, udp, tcp }

/// 飞行模式表里的一项。`id` 是 MAVLink custom_mode（PX4 为 main/sub 打包值）。
class FlightModeOption {
  const FlightModeOption({
    required this.id,
    required this.name,
    required this.settable,
  });

  final int id;
  final String name;
  final bool settable;
}

/// Selects the tuning UI surface for the detected ArduPilot vehicle profile.
enum VehicleControlProfile { unknown, copter, fixedWing, planeVtol }

/// Per-vehicle UI state captured while another aircraft owns the native focus.
///
/// This mirrors the mutable fields of ConnectionController that are not shared
/// across vehicles. It is deliberately private; callers interact through the
/// controller's normal getters and switch actions.
final class _VehicleRuntimeSnapshot {
  const _VehicleRuntimeSnapshot({
    required this.phase,
    required this.message,
    required this.telemetryError,
    required this.calibrationStatus,
    required this.calibrationError,
    required this.satellitesVisible,
    required this.gpsFixType,
    required this.headingDegrees,
    required this.airspeedMetersPerSecond,
    required this.groundSpeedMetersPerSecond,
    required this.relativeAltitudeMeters,
    required this.absoluteAltitudeMeters,
    required this.climbRateMetersPerSecond,
    required this.throttlePercent,
    required this.rollDegrees,
    required this.pitchDegrees,
    required this.batteryRemainingPercent,
    required this.batteryVoltage,
    required this.firmwareInfo,
    required this.vehicleIdentity,
    required this.vehicleArmed,
    required this.vehicleFlightMode,
    required this.flightModesAvailable,
    required this.flightModeOptions,
    required this.flightMessages,
    required this.lastMessageSeq,
    required this.vehicleType,
    required this.vehicleAutopilotType,
    required this.vehicleLandedState,
    required this.vehicleVtolState,
    required this.guidedTargetPosition,
    required this.guidedTargetValid,
    required this.guidedTargetModeObserved,
    required this.guidedTargetModeDeadline,
    required this.flightTimeSeconds,
    required this.flightDistanceMeters,
    required this.flightStatsArmed,
    required this.lastFlightLatitude,
    required this.lastFlightLongitude,
    required this.firmwareVersionInfo,
    required this.sensorHealth,
    required this.waveformWindowMs,
    required this.waveformImuInstances,
    required this.waveformMagInstances,
    required this.waveformImuInstance,
    required this.waveformMagInstance,
    required this.parameters,
    required this.parameterWriteErrors,
    required this.parameterMetadataLoaded,
    required this.parameterMetadataAttempts,
    required this.pendingParameterMetadata,
    required this.parameterReceived,
    required this.parameterTotal,
    required this.parametersLoaded,
    required this.parametersLoading,
    required this.parameterError,
    required this.servoOutputs,
    required this.servoOutputsValid,
    required this.rcChannels,
    required this.rcChannelsValid,
    required this.vehiclePosition,
    required this.vehiclePositionValid,
    required this.homePosition,
    required this.homePositionValid,
    required this.serialHardwareMappings,
    required this.serialHardwareMappingLoading,
    required this.serialHardwareMappingError,
    required this.serialHardwareMappingRequested,
    required this.firmwareVersionRequested,
    required this.parameterRequestStarted,
    required this.loadingParameterMetadata,
  });

  final SerialConnectionPhase phase;
  final String? message;
  final String? telemetryError;
  final CalibrationStatus calibrationStatus;
  final String? calibrationError;
  final int? satellitesVisible;
  final int? gpsFixType;
  final double? headingDegrees;
  final double? airspeedMetersPerSecond;
  final double? groundSpeedMetersPerSecond;
  final double? relativeAltitudeMeters;
  final double? absoluteAltitudeMeters;
  final double? climbRateMetersPerSecond;
  final double? throttlePercent;
  final double? rollDegrees;
  final double? pitchDegrees;
  final int? batteryRemainingPercent;
  final double? batteryVoltage;
  final Map<String, Object?>? firmwareInfo;
  final Map<String, Object?>? vehicleIdentity;
  final bool vehicleArmed;
  final String vehicleFlightMode;
  final bool flightModesAvailable;
  final List<FlightModeOption> flightModeOptions;
  final List<FlightMessage> flightMessages;
  final int lastMessageSeq;
  final int vehicleType;
  final int vehicleAutopilotType;
  final int vehicleLandedState;
  final int vehicleVtolState;
  final VehicleFix? guidedTargetPosition;
  final bool guidedTargetValid;
  final bool guidedTargetModeObserved;
  final DateTime? guidedTargetModeDeadline;
  final double flightTimeSeconds;
  final double flightDistanceMeters;
  final bool flightStatsArmed;
  final double? lastFlightLatitude;
  final double? lastFlightLongitude;
  final Map<String, Object?>? firmwareVersionInfo;
  final Map<String, Object?>? sensorHealth;
  final int waveformWindowMs;
  final List<WaveformInstance> waveformImuInstances;
  final List<WaveformInstance> waveformMagInstances;
  final int waveformImuInstance;
  final int waveformMagInstance;
  final Map<String, VehicleParameter> parameters;
  final Map<String, String> parameterWriteErrors;
  final Set<String> parameterMetadataLoaded;
  final Map<String, int> parameterMetadataAttempts;
  final Set<String> pendingParameterMetadata;
  final int parameterReceived;
  final int parameterTotal;
  final bool parametersLoaded;
  final bool parametersLoading;
  final String? parameterError;
  final List<int> servoOutputs;
  final bool servoOutputsValid;
  final List<int> rcChannels;
  final bool rcChannelsValid;
  final VehicleFix? vehiclePosition;
  final bool vehiclePositionValid;
  final VehicleFix? homePosition;
  final bool homePositionValid;
  final Map<String, String> serialHardwareMappings;
  final bool serialHardwareMappingLoading;
  final String? serialHardwareMappingError;
  final bool serialHardwareMappingRequested;
  final bool firmwareVersionRequested;
  final bool parameterRequestStarted;
  final bool loadingParameterMetadata;
}

final class ConnectionController extends ChangeNotifier {
  /// [telemetryBackend] 等可选参数用于注入替身（测试）或复用已绑定的后端；
  /// 不传时按 [NativeCore] 懒创建。
  ConnectionController({
    SigningKeyStore? signingKeyStore,
    this._telemetryBackend,
    this._parameterBackend,
    this._calibrationBackend,
    this._flightCommandBackend,
  }) {
    _signing = SigningController(
      native: () => _native,
      ready: () => initializationError == null,
      sessionId: () => sessionId,
      refreshVehicles: () => fleet.refresh(),
      vehicleIds: () => fleetSnapshot.vehicles.map((vehicle) => vehicle.id),
      keyStore: signingKeyStore,
    )..addListener(notifyListeners);
    _initialize();
    unawaited(_signing.loadLibrary());
  }

  late final SigningController _signing;

  /// MV1 seam: connection registry and command target tokens.
  /// Multi-link UI is intentionally not exposed until per-vehicle rebinding is verified.
  FleetController? _fleet;
  FleetSnapshot get fleetSnapshot =>
      _fleet?.snapshot ??
      FleetSnapshot(vehicles: const [], activeVehicleId: null);
  FleetController get fleet => _fleet ?? (throw StateError('多机后端尚未就绪'));
  Timer? _fleetTimer;

  /// mgcore 原生库与句柄。全部原生调用都经过它，控制器本身不接触 dart:ffi。
  /// 初始化失败时为 null，此时仅 [initializationError] 可用。
  NativeCore? _native;

  MissionDocumentController? _offlineMissionPlanning;
  final Map<int, MissionDocumentController> _missionPlanningBySession = {};

  final Map<int, _VehicleRuntimeSnapshot> _runtimeSnapshots = {};
  TelemetryFfiBackend? _telemetryBackend;
  ParameterFfiBackend? _parameterBackend;
  CalibrationFfiBackend? _calibrationBackend;
  FlightCommandFfiBackend? _flightCommandBackend;
  LogBackend? _logBackend;
  LogController? _logController;
  TlogBackend? _tlogBackend;
  TlogController? _tlogController;
  FileBackend? _fileBackend;
  FileController? _fileController;
  TerrainBackend? _terrainBackend;
  TerrainController? _terrainController;
  ElevationProvider? _elevationProvider;
  SurveyBackend? _surveyBackend;
  SurveyController? _surveyController;
  PayloadBackend? _payloadBackend;
  PayloadController? _payloadController;

  MissionDocumentController get missionPlanning {
    final id = sessionId;
    if (id == null) {
      final controller = _offlineMissionPlanning ??= _newMissionController(
        null,
      );
      controller.bindSession(null);
      return controller;
    }
    final controller = _missionPlanningBySession.putIfAbsent(
      id,
      () => _newMissionController(id),
    );
    controller.bindSession(id);
    return controller;
  }

  MissionDocumentController _newMissionController(int? ownerId) =>
      MissionDocumentController(
        backend: MissionDocumentFfiBackend(
          _requireNative().library,
          _requireNative().handle,
          isConnected: () => isConnected && sessionId == ownerId,
          beforeAccess: () {
            if (_disposed ||
                sessionId != ownerId ||
                fleetSnapshot.activeVehicleId != ownerId) {
              throw StateError('任务所属飞机已切换或断开，请在当前飞机重新操作');
            }
          },
        ),
      );

  TelemetryFfiBackend get telemetryBackend => _telemetryBackend ??=
      TelemetryFfiBackend(_requireNative().library, _requireNative().handle);

  ParameterFfiBackend get parameterBackend => _parameterBackend ??=
      ParameterFfiBackend(_requireNative().library, _requireNative().handle);

  CalibrationFfiBackend get calibrationBackend => _calibrationBackend ??=
      CalibrationFfiBackend(_requireNative().library, _requireNative().handle);

  FlightCommandFfiBackend get flightCommandBackend =>
      _flightCommandBackend ??= FlightCommandFfiBackend(
        _requireNative().library,
        _requireNative().handle,
        beforeCommand: () {
          _requireFreshVehicleState('发送指令');
          fleet.refresh();
          if (fleet.snapshot.activeVehicleId != sessionId) {
            throw StateError('当前飞机焦点与连接会话不一致，拒绝发送指令');
          }
          fleet.validatePinnedTarget();
        },
        targetVehicleId: () {
          fleet.refresh();
          final id = fleet.snapshot.activeVehicleId;
          return id == sessionId ? id : null;
        },
      );

  /// 机载日志页的控制器。按当前连接会话绑定；会话切换或断开会丢弃
  /// 旧列表与下载状态，避免把上一架飞机的日志结果写回界面。
  LogController get logController {
    final controller = _logController ??= LogController(
      backend: _logBackend ??= _makeLogBackend(),
    );
    controller.bindSession(sessionId);
    return controller;
  }

  LogBackend _makeLogBackend() {
    final native = _native;
    if (native == null) {
      // 原生库不可用时也要让页面渲染骨架并给出确定的错误，而不是构建期抛异常。
      return const LogUnavailableBackend();
    }
    return LogFfiBackend(
      native.library,
      native.handle,
      isConnected: () => isConnected && sessionId != null,
    );
  }

  /// 「飞行记录」页的控制器：链路 `.tlog` 录制 + 本地离线回放。
  ///
  /// 回放只读本地文件、与连接无关，所以这里不做会话绑定；录制挂在活动车辆上，
  /// 由页面在进入时读取 `mg_tlog_status` 并对未连接状态禁用开始按钮。
  TlogController get tlogController => _tlogController ??= TlogController(
    backend: _tlogBackend ??= _makeTlogBackend(),
  );

  TlogBackend _makeTlogBackend() {
    final native = _native;
    if (native == null) {
      return const TlogUnavailableBackend();
    }
    return TlogFfiBackend(
      native.library,
      native.handle,
      isConnected: () => isConnected && sessionId != null,
    );
  }

  /// 「机载文件」页的控制器：MAVLink FTP 目录浏览、下载与上传。
  ///
  /// 与机载日志一样按当前连接会话绑定；换飞机或断开会丢弃列表与传输状态，
  /// 避免把上一架飞机的文件结果写回界面。
  FileController get fileController {
    final controller = _fileController ??= FileController(
      backend: _fileBackend ??= _makeFileBackend(),
    );
    controller.bindSession(sessionId);
    return controller;
  }

  FileBackend _makeFileBackend() {
    final native = _native;
    if (native == null) {
      // 原生库不可用时页面仍要能渲染骨架并给出确定的错误。
      return const FileUnavailableBackend();
    }
    return FileFfiBackend(
      native.library,
      native.handle,
      isConnected: () => isConnected && sessionId != null,
    );
  }

  /// 地形高程控制器：native 缓存 + 在线 provider。
  ///
  /// S1 只交付取数能力（缓存命中/回填），尚未接入具体页面——前端入口待
  /// docs/features/survey-and-terrain.md 的 D7 决策。provider 默认 open-elevation，
  /// 可替换为自建端点。
  TerrainController get terrainController =>
      _terrainController ??= TerrainController(
        backend: _terrainBackend ??= _makeTerrainBackend(),
        provider: _elevationProvider ??= OpenElevationProvider(),
      );

  TerrainBackend _makeTerrainBackend() {
    final native = _native;
    if (native == null) {
      return const TerrainUnavailableBackend();
    }
    return TerrainFfiBackend(native.library, native.handle);
  }

  /// 测绘规划控制器：native 纯计算生成航线，前端负责配置与并入任务。
  ///
  /// 生成不需要连接飞控；并把结果并入任务文档由 [SurveyController] 处理，
  /// 上传仍走任务面板。入口见 docs/constraints/ui-pages.md 的「测绘规划」。
  SurveyController get surveyController => _surveyController ??=
      SurveyController(backend: _surveyBackend ??= _makeSurveyBackend());

  SurveyBackend _makeSurveyBackend() {
    final native = _native;
    if (native == null) {
      return const SurveyUnavailableBackend();
    }
    return SurveyFfiBackend(native.library, native.handle);
  }

  /// 「相机与云台」页的控制器：设备清单、状态与拍摄/指向命令。
  ///
  /// 与日志/文件一样按当前连接会话绑定；换飞机或断开会丢弃设备缓存，
  /// 避免把上一架飞机的相机状态写回界面。命令入口仍要求已连接。
  PayloadController get payloadController {
    final controller = _payloadController ??= PayloadController(
      backend: _payloadBackend ??= _makePayloadBackend(),
    );
    controller.bindSession(sessionId);
    return controller;
  }

  PayloadBackend _makePayloadBackend() {
    final native = _native;
    if (native == null) {
      // 原生库不可用时页面仍要能渲染骨架并给出确定的错误。
      return const PayloadUnavailableBackend();
    }
    return PayloadFfiBackend(
      native.library,
      native.handle,
      isConnected: () => isConnected && sessionId != null,
    );
  }

  /// 会话切换/断开时把日志控制器绑到新会话；`null` 表示当前无会话。
  void _bindLogSession(int? id) {
    _logController?.bindSession(id);
    _fileController?.bindSession(id);
    _payloadController?.bindSession(id);
  }

  final List<SerialPortInfo> ports = [];
  final List<int> baudRates = const [57600, 115200, 230400, 460800, 921600];

  /// 波形缓冲区上限。50s 窗口 @10Hz 约 500 帧，取 600 留余量，
  /// 正常就是逐帧原始点，抽稀只在采样率异常高时才兜底。
  /// 点数直接决定每帧的绘制和分配开销，这个值不要为了"看起来更细"随意调大。
  static const int _waveformMaxPoints = 600;

  /// 每个采样点：相对毫秒 + 加速度 3 轴 + 角速度 3 轴 + 磁力 3 轴。
  static const int _waveformFloatsPerPoint = 10;

  /// 波形页面默认显示的时间窗口（毫秒）。
  ///
  /// 这个值同时决定曲线的横坐标范围：后端按它裁出返回的点，图表用它算 maxX。
  static const int _waveformDefaultWindowMs = 40000;

  /// 波形页面取数间隔。
  ///
  /// 必须跟后端的 10Hz 采样对齐：这里曾经是 1 秒一次，结果飞控在持续采集，
  /// 界面却每秒才读一次缓冲，读数会在动作结束后成批跳出，表现为"摇完才变"。
  /// 波形与曲线统一按 10Hz 刷新，不要降到秒级。
  static const Duration _waveformPollInterval = Duration(milliseconds: 100);

  /// 波形页面可见时请求的采样间隔（10Hz）。SCALED_IMU/2/3 各请求一次，
  /// 按显示需要取数，把串口带宽和缓冲写入量压到常规链路水平。
  static const int _waveformStreamIntervalUs = 100000;
  static const int _waveformIdleStreamIntervalUs = 500000;

  /// 波形页面按采样间隔请求的消息 ID：SCALED_IMU / SCALED_IMU2 / SCALED_IMU3。
  static const List<int> _waveformMessageIds = [26, 116, 129];

  /// 常规遥测轮询间隔。位置/姿态/VFR_HUD 提到 20Hz，地图和仪表更顺滑。
  /// 波形仍保持独立的 10Hz 采样与轮询，不跟着一起加倍。
  static const Duration _telemetryPollInterval = Duration(milliseconds: 50);

  /// MAV_CMD_DO_SET_MODE 的 base mode 必须声明 custom mode 有效。
  /// QGC 在 Vehicle::setFlightMode() 里也是发送这个标志。
  static const int _mavModeFlagCustomModeEnabled = 1;
  static const Duration _commandStatePollInterval = Duration(milliseconds: 100);
  static const Duration _flightModeValidationWindow = Duration(
    milliseconds: 1300,
  );
  static const Duration _armValidationWindow = Duration(milliseconds: 1500);

  /// Go To 下发后，等待飞控切到对应模式的窗口。超过后仍未进入目标模式，
  /// 就把尚未激活的显示目标作废，避免以后手工切回 Guided 时旧点复活。
  static const Duration _guidedTargetModeTransitionTimeout = Duration(
    seconds: 10,
  );

  final ValueNotifier<int> parameterChanges = ValueNotifier(0);
  final ValueNotifier<int> telemetryChanges = ValueNotifier(0);
  final ValueNotifier<int> profileChanges = ValueNotifier(0);
  final ValueNotifier<int> connectionChanges = ValueNotifier(0);
  final ValueNotifier<int> waveformChanges = ValueNotifier(0);
  final ValueNotifier<int> calibrationChanges = ValueNotifier(0);

  String? selectedPort;
  int baudRate = 115200;
  ConnectionTransport transport = ConnectionTransport.serial;
  String udpHost = '0.0.0.0';
  String udpPortText = '14550';
  String tcpHost = '127.0.0.1';
  String tcpPortText = '5760';
  int? sessionId;

  /// 当前 UI 关注的消息链路。与 [sessionId] 不同：链路可以尚未识别出飞控，
  /// 此时 sessionId 为空，但连接面板和状态栏仍应显示该链路正在等待心跳。
  int? _activeLinkId;
  SerialConnectionPhase phase = SerialConnectionPhase.disconnected;
  CalibrationStatus calibrationStatus = const CalibrationStatus();
  String? calibrationError;
  String? message;
  String? telemetryError;
  int? satellitesVisible;
  int? gpsFixType;
  double? headingDegrees;
  double? airspeedMetersPerSecond;
  double? groundSpeedMetersPerSecond;
  double? relativeAltitudeMeters;
  double? absoluteAltitudeMeters;

  /// QGC instrument panel 的 Climb Rate / Throttle，来源同为 VFR_HUD。
  double? climbRateMetersPerSecond;
  double? throttlePercent;

  /// 姿态角（度），来源 ATTITUDE。姿态仪用。
  double? rollDegrees;
  double? pitchDegrees;
  int? batteryRemainingPercent;
  double? batteryVoltage;
  Map<String, Object?>? firmwareInfo;
  Map<String, Object?>? vehicleIdentity;

  /// 心跳摘要：QGC 的引导操作门控按 armed / 飞行模式 / 机型判断。
  bool vehicleArmed = false;
  String vehicleFlightMode = '';

  /// 飞行模式操作台数据：后端按机型给出的完整模式表（含不可切换项）。
  bool flightModesAvailable = false;
  List<FlightModeOption> flightModeOptions = const [];

  /// 飞控消息（STATUSTEXT）历史，供消息框显示。只追加，断开连接时清空。
  List<FlightMessage> flightMessages = const [];
  int _lastMessageSeq = 0;
  int vehicleType = 0;
  int vehicleAutopilotType = 0;

  /// EXTENDED_SYS_STATE 的 landed_state（MAV_LANDED_STATE）：
  /// 1=undefined、2=on_ground、3=takeoff、4=in_air、5=landing。
  int vehicleLandedState = 0;

  /// EXTENDED_SYS_STATE.vtol_state (MAV_VTOL_STATE):
  /// 0=undefined, 1=transition_to_fw, 2=transition_to_mc, 3=MC, 4=FW.
  int vehicleVtolState = 0;

  /// Last accepted guided "move to here" target. This is display state only;
  /// it does not change mission or guided command logic.
  VehicleFix? guidedTargetPosition;
  bool guidedTargetValid = false;

  /// 显示目标是否已经真正进入过 Go To 模式。
  ///
  /// 只在“已进入 Guided/Auto Loiter，随后离开”时永久作废。这样刚下发命令、
  /// 飞控尚未切模式的短暂窗口不会误清目标，而 RTL/降落/自稳之后再回到
  /// Guided 也不会把旧点重新显示出来。
  bool _guidedTargetModeObserved = false;
  DateTime? _guidedTargetModeDeadline;

  /// 当前是否处于 Go To 会保持目标的飞行模式。
  ///
  /// 对应 QGC `FlyViewMap.qml` 的
  /// `_activeVehicle.flightMode === _activeVehicle.gotoFlightMode`：
  /// ArduPilot 是 Guided，PX4 的 Go To 模式是 Auto Loiter。离开该模式后，
  /// Go To 图标和固定翼盘旋圆都不再代表当前目的地。
  bool get isGuidedGotoFlightMode {
    final mode = vehicleFlightMode.trim().toLowerCase();
    if (mode.isEmpty) return false;
    if (vehicleAutopilotType == 12) return mode == 'auto loiter';
    return mode == 'guided';
  }

  /// 当前是否有仍处于活动飞行模式下的 Go To 目标。
  bool get hasActiveGuidedTarget =>
      guidedTargetValid &&
      guidedTargetPosition != null &&
      isGuidedGotoFlightMode;

  /// QGC `Vehicle::_flightTimerStart()` 的等价统计。
  ///
  /// 飞控不直接提供 FlightTime / FlightDistance，QGC 也是地面站本地累计：
  /// 解锁时清零并开始计时，上锁后停止；每次定位刷新把位移累进总里程。
  double flightTimeSeconds = 0;
  double flightDistanceMeters = 0;
  final Stopwatch _flightStopwatch = Stopwatch();
  double _flightStopwatchOffsetSeconds = 0;
  bool _flightStatsArmed = false;
  double? _lastFlightLatitude;
  double? _lastFlightLongitude;

  /// QGC `Vehicle::flying` 的等价判断，外加 ArduPilot VTOL 的兼容回退。
  ///
  /// ArduPlane 在 QuadPlane 垂直起飞阶段可能仍上报 landed_state=ON_GROUND，
  /// 即使飞机已经离地爬升。此时仅看 landed_state 会把引导操作全部误判为
  /// 不可用，所以再用已解锁和相对高度做保守回退。
  bool get vehicleFlying =>
      vehicleLandedState == 3 ||
      vehicleLandedState == 4 ||
      vehicleLandedState == 5 ||
      (vehicleArmed && (relativeAltitudeMeters ?? 0).abs() > 2.0);
  Map<String, Object?>? firmwareVersionInfo;
  Map<String, Object?>? sensorHealth;

  /// 波形采样点，由后端环形缓冲直接写入：每点 10 个 float
  /// （相对毫秒 + accel XYZ + gyro XYZ + mag XYZ）。
  /// IMU 和磁力计各自一份，因为两个面板会选不同的实例；
  /// 这些都是复用的缓冲区，不是每次轮询新建的列表。
  late final Float32List waveformImuPoints;
  int waveformImuPointCount = 0;
  late final Float32List waveformMagPoints;
  int waveformMagPointCount = 0;
  final Map<int, List<double>> waveformMagLatestByInstance = {};

  /// 波形页面显示的时间窗口（毫秒）。
  int waveformWindowMs = _waveformDefaultWindowMs;

  /// 后端识别到的实例清单，以及当前选中的实例号。
  List<WaveformInstance> waveformImuInstances = const [
    _waveformDefaultInstance,
  ];
  List<WaveformInstance> waveformMagInstances = const [
    _waveformDefaultInstance,
  ];
  int waveformImuInstance = 0;
  int waveformMagInstance = 0;

  /// 后端还没上报任何实例时的占位：只有 IMU1 / 磁力计 1，且未激活。
  static const WaveformInstance _waveformDefaultInstance = WaveformInstance(
    index: 0,
    active: false,
    name: '',
  );
  final Map<String, VehicleParameter> parameters = {};
  final Map<String, Future<void>> _parameterWriteJobs = {};
  final Map<String, String> _parameterWriteErrors = {};
  final Set<String> _parameterMetadataLoaded = {};
  final Map<String, int> _parameterMetadataAttempts = {};
  final Set<String> _pendingParameterMetadata = {};
  int parameterReceived = 0;
  int parameterTotal = 0;
  bool parametersLoaded = false;
  bool parametersLoading = false;
  String? parameterError;
  List<int> servoOutputs = const [];
  bool servoOutputsValid = false;
  List<int> rcChannels = const [];
  bool rcChannelsValid = false;

  /// 飞控上报的当前位置（有
  /// `WGS84` 经纬度、海拔与航向）。[vehiclePositionValid] 为 false 时地图不画。
  VehicleFix? vehiclePosition;
  bool vehiclePositionValid = false;

  /// 飞控上报的 Home 点，用于任务规划时显示起飞位置。
  VehicleFix? homePosition;
  bool homePositionValid = false;
  final Map<String, String> serialHardwareMappings = {};
  bool serialHardwareMappingLoading = false;
  String? serialHardwareMappingError;
  bool _serialHardwareMappingRequested = false;
  String? initializationError;
  Timer? _stateTimer;
  Timer? _telemetryTimer;
  Timer? _waveformTimer;
  Timer? _calibrationTimer;
  bool _waveformActive = false;
  int _lastSystemStatusPollMs = 0;
  bool _busy = false;
  bool _switchingSession = false;
  int _sessionGeneration = 0;
  bool _disposed = false;
  bool _firmwareVersionRequested = false;
  bool _parameterRequestStarted = false;
  bool _loadingParameterMetadata = false;

  /// 本次会话是否收到过遥测（用于区分"还没连上"与"连上后掉线"）。
  bool _telemetrySnapshotValid = false;

  /// 链路仍在，但 core 报告的 heartbeat age 已超过新鲜度阈值。
  bool _telemetryGroupsStale = false;
  VehicleControlProfile? _lastPublishedControlProfile;

  bool get busy => _busy;
  bool get isConnected => phase == SerialConnectionPhase.connected;
  bool get hasSession => sessionId != null;

  /// 最后一次有效遥测是否已过期。
  ///
  /// core 的 `TelemetryFreshness` 为每类遥测维护 age（默认 3000ms 即过期）。链路断开
  /// 或心跳丢失后 `_pollTelemetry` 会提前返回，界面保留的是最后一帧；此时必须把读数
  /// 标记为过期，不能继续当成当前值展示，否则操作者会照着冻结的高度/电压/模式判断。
  bool get telemetryStale =>
      _telemetrySnapshotValid && (!isConnected || _telemetryGroupsStale);

  /// Safety-sensitive flight state is usable only while the heartbeat-backed
  /// telemetry snapshot is fresh.
  bool get vehicleStateFresh =>
      isConnected && _telemetrySnapshotValid && !_telemetryGroupsStale;
  int? get activeLinkId => _activeLinkId;
  SigningController get signing => _signing;
  List<FleetLinkSummary> get fleetLinks => fleetSnapshot.links;

  /// True while a fleet-scoped asynchronous operation pins the active target.
  bool get fleetOperationsActive => _fleet?.targetOperationActive ?? false;
  VehicleControlProfile get controlProfile {
    final firmware = firmwareInfo?['firmware']?.toString().toLowerCase();
    final vehicle = firmwareInfo?['vehicle_type']?.toString().toLowerCase();
    if (firmware != 'ardupilot') return VehicleControlProfile.unknown;
    if (vehicle == 'copter') return VehicleControlProfile.copter;
    if (vehicle == 'vtol') return VehicleControlProfile.planeVtol;
    if (vehicle == 'fixedwing' || vehicle == 'plane') {
      final qEnable = parameters['Q_ENABLE']?.value ?? 0;
      final tailsitter = parameters['Q_TAILSIT_ENABLE']?.value ?? 0;
      final tilt = parameters['Q_TILT_ENABLE']?.value ?? 0;
      return qEnable != 0 || tailsitter != 0 || tilt != 0
          ? VehicleControlProfile.planeVtol
          : VehicleControlProfile.fixedWing;
    }
    return VehicleControlProfile.unknown;
  }

  /// “设置航向”传给飞控的最大偏航速率，0 表示用飞控默认值。
  static const double _headingYawRateDegreesPerSecond = 0;

  bool get supportsGuidedMode => _firmwareCapability('guided_mode');
  bool get supportsPause => _firmwareCapability('pause');

  /// 地图菜单“环绕飞行”入口的开关，来自固件插件（QGC `OrbitModeCapability`）。
  ///
  /// QGC `GuidedActionsController.qml:138` 的 `showOrbit` 就是按 `orbitMode`
  /// 能力位门控的，不再靠 `guidedMode` 顺带打开，避免地图上出现飞控其实
  /// 不执行的入口。
  bool get supportsOrbit => _firmwareCapability('orbit');

  /// 地图菜单“在此外设置兴趣点”入口的开关（QGC `ROIModeCapability`）。
  bool get supportsRoi => _firmwareCapability('roi');

  /// QGC `_guidedModeTakeoff`：多旋翼和 VTOL 支持引导起飞，固定翼需要跑道起飞，
  /// 交给任务/AUTO 流程。以固件插件给的能力位为准。
  ///
  /// 刚连上、固件信息还没拉到时（`firmwareInfo == null`）退回按 MAV_TYPE 判断，
  /// 避免起飞按钮在遥测到达前抖动。
  bool get supportsGuidedTakeoff {
    final capabilities = firmwareInfo?['capabilities'];
    if (capabilities is Map) return capabilities['takeoff'] == true;
    return const {
      2, 3, 4, 13, 14, 15, // MAV_TYPE multirotor family
      19, 20, 21, 22, 23, 24, // MAV_TYPE VTOL family
    }.contains(vehicleType);
  }

  /// QGC `APMFirmwarePlugin::minimumTakeoffAltitudeMeters` 的等价实现。
  ///
  /// VTOL 依次读 `Q_PILOT_TKO_ALT_M`（4.7+，米）→ `Q_PILOT_TKOFF_ALT`（旧版，厘米）
  /// → `Q_RTL_ALT`；多旋翼读 `PILOT_TKO_ALT_M` → `PILOT_TKOFF_ALT`（厘米）。
  /// 用户在界面上填的高度只会把它抬高，不会降低（QGC 同）。
  double get minimumTakeoffAltitudeMeters {
    double? read(String name) {
      final value = parameters[name]?.value;
      return value != null && value > 0 ? value : null;
    }

    // PX4 没有 ArduPilot 那套 `PILOT_TKO_ALT_M` 参数：QGC 的 PX4 插件直接用
    // `FirmwarePlugin::minimumTakeoffAltitudeMeters` 基类默认值（10 ft = 3.048 m）。
    // core 固件插件把它经 `mg_firmware_get_type` 的 `minimum_takeoff_altitude_m`
    // 暴露出来，这里优先采用。
    final firmware = firmwareInfo?['firmware']?.toString().toLowerCase();
    if (firmware == 'px4') {
      return (firmwareInfo?['minimum_takeoff_altitude_m'] as num?)
              ?.toDouble() ??
          0;
    }

    if (controlProfile == VehicleControlProfile.planeVtol) {
      return read('Q_PILOT_TKO_ALT_M') ??
          (read('Q_PILOT_TKOFF_ALT') != null
              ? read('Q_PILOT_TKOFF_ALT')! / 100.0
              : null) ??
          read('Q_RTL_ALT') ??
          0;
    }
    return read('PILOT_TKO_ALT_M') ??
        (read('PILOT_TKOFF_ALT') != null
            ? read('PILOT_TKOFF_ALT')! / 100.0
            : null) ??
        0;
  }

  /// 固定翼盘旋半径（米），来自飞控参数 `WP_LOITER_RAD`。
  ///
  /// ArduPilot 的 `WP_LOITER_RAD`（`ArduPlane/Parameters.cpp`）定义固定翼盘旋时
  /// 距航点中心的距离，**负值表示逆时针**。我们的 `DO_REPOSITION` 里 param3 传 0，
  /// 飞控就沿用这个参数，所以地图上按它画的圆就是飞机实际会飞的圆——不是随便画的。
  ///
  /// 参数还没下载到、或飞控没给这个参数时返回 null（多旋翼也不需要）。
  double? get loiterRadiusMeters {
    final value = parameters['WP_LOITER_RAD']?.value;
    if (value == null || value == 0) return null;
    return value.abs();
  }

  /// 盘旋方向：`WP_LOITER_RAD` 为正（或缺失）是顺时针，为负是逆时针。
  bool get loiterClockwise => (parameters['WP_LOITER_RAD']?.value ?? 1) >= 0;

  /// 是否处于"按固定翼方式飞"的状态。
  ///
  /// 纯固定翼，或 VTOL 已经转到固定翼（`MAV_VTOL_STATE_FW = 4`）。多旋翼/悬停
  /// 状态下 go to location 是平移加悬停，没有盘旋圆。
  bool get inForwardFlight =>
      controlProfile == VehicleControlProfile.fixedWing ||
      (controlProfile == VehicleControlProfile.planeVtol &&
          vehicleVtolState == 4);

  /// 是否支持“设置航向”（原地转向）。
  ///
  /// QGC `APMFirmwarePlugin::_isCapable` 只给多旋翼和潜航器
  /// `ChangeHeadingCapability`，固定翼和 VTOL 都不给，所以这里保持一致。
  bool get supportsChangeHeading {
    final capabilities = firmwareInfo?['capabilities'];
    if (capabilities is Map) return capabilities['change_heading'] == true;
    return const {
      2, 3, 4, 13, 14, 15, // MAV_TYPE multirotor family
      12, // MAV_TYPE_SUBMARINE
    }.contains(vehicleType);
  }

  /// 飞控是否上报了 GPS 传感器。
  ///
  /// QGC 只在 `!(sensorsPresentBits & MAV_SYS_STATUS_SENSOR_GPS)` 时才显示
  /// “Set Estimator Origin”（室内/视觉定位场景）。拿不到传感器表时返回 true，
  /// 宁可少显示这个室内专用入口。
  bool get hasGpsSensor {
    final health = sensorHealth;
    if (health == null) return true;
    final groups = health['groups'];
    if (groups is! Map) return true;
    final navigation = groups['navigation'];
    if (navigation is! List) return true;
    for (final item in navigation) {
      if (item is Map && item['key'] == 'gps') return item['present'] == true;
    }
    return true;
  }

  bool _firmwareCapability(String name) {
    final capabilities = firmwareInfo?['capabilities'];
    return capabilities is Map && capabilities[name] == true;
  }

  /// 参数是否正在写入飞控；位掩码这类需要读-改-写的控件用它避免并发覆盖。
  bool isParameterWriting(String name) => _parameterWriteJobs.containsKey(name);
  String? get parameterWriteError =>
      _parameterWriteErrors.isEmpty ? null : _parameterWriteErrors.values.last;

  void _notifyParameterChanges() {
    if (!_disposed) parameterChanges.value++;
  }

  void _notifyTelemetryChanges() {
    if (!_disposed) telemetryChanges.value++;
  }

  void _notifyProfileChanges() {
    final profile = controlProfile;
    if (_disposed || profile == _lastPublishedControlProfile) return;
    _lastPublishedControlProfile = profile;
    profileChanges.value++;
  }

  void _notifyConnectionChanges() {
    if (!_disposed) connectionChanges.value++;
  }

  void _notifyWaveformChanges() {
    if (!_disposed) waveformChanges.value++;
  }

  void _notifyCalibrationChanges() {
    if (!_disposed) calibrationChanges.value++;
  }

  Future<void> startAccelerometerCalibration(
    AccelerometerCalibrationMode mode,
  ) async {
    if (!isConnected || calibrationStatus.active) return;
    calibrationError = null;
    final nativeMode = switch (mode) {
      AccelerometerCalibrationMode.sixFace => 1,
      AccelerometerCalibrationMode.level => 2,
      AccelerometerCalibrationMode.simple => 4,
    };
    try {
      final result = calibrationBackend.start(1, nativeMode);
      if (result['success'] != true) {
        throw StateError(ffiErrorText(result, '启动加速度计校准失败'));
      }
      _startCalibrationPolling();
      await _pollCalibrationStatus();
    } on Object catch (error) {
      calibrationError = _friendlyError(error);
      _notifyCalibrationChanges();
      notifyListeners();
    }
  }

  Future<void> startCalibration(CalibrationType type) async {
    if (type == CalibrationType.accelerometer) {
      return startAccelerometerCalibration(
        AccelerometerCalibrationMode.sixFace,
      );
    }
    if (!isConnected || calibrationStatus.active) return;
    calibrationError = null;
    try {
      final nativeType = switch (type) {
        CalibrationType.gyroscope => 0,
        CalibrationType.accelerometer => 1,
        CalibrationType.magnetometer => 2,
        CalibrationType.barometer => 3,
        CalibrationType.radio => 4,
      };
      final result = calibrationBackend.start(nativeType, 1);
      if (result['success'] != true) {
        throw StateError(ffiErrorText(result, '启动校准失败'));
      }
      _startCalibrationPolling();
      await _pollCalibrationStatus();
    } on Object catch (error) {
      calibrationError = _friendlyError(error);
      _notifyCalibrationChanges();
      notifyListeners();
    }
  }

  Future<void> confirmAccelerometerPosition(int position) async {
    if (!isConnected || !calibrationStatus.active) return;
    try {
      final result = calibrationBackend.confirmAccelerometerPosition(position);
      if (result['success'] != true) {
        throw StateError(ffiErrorText(result, '确认姿态失败'));
      }
      await _pollCalibrationStatus();
    } on Object catch (error) {
      calibrationError = _friendlyError(error);
      _notifyCalibrationChanges();
      notifyListeners();
    }
  }

  Future<void> cancelCalibration() async {
    if (!calibrationStatus.active || calibrationStatus.cancelling) return;
    final requestedType = calibrationStatus.type;
    // S8：先把状态标成“取消中”并保持轮询。以前这里一进来就把界面写成
    // “已取消”并停掉轮询，而飞控可能还在采样。
    calibrationStatus = CalibrationStatus(
      active: true,
      cancelling: true,
      type: requestedType,
      stage: calibrationStatus.stage,
      progress: calibrationStatus.progress,
      currentStep: calibrationStatus.currentStep,
      totalSteps: calibrationStatus.totalSteps,
      message: '正在取消校准…',
      accelerometerMode: calibrationStatus.accelerometerMode,
      compassProgress: calibrationStatus.compassProgress,
    );
    calibrationError = null;
    _notifyCalibrationChanges();
    notifyListeners();

    Object? cancellationError;
    try {
      final result = calibrationBackend.cancel();
      if (result['success'] != true) {
        final message = ffiErrorText(result, '取消校准失败');
        // A stale UI snapshot can outlive the native calibration state. In
        // that case the local clear above is already the correct end state.
        if (!message.contains('no calibration in progress')) {
          throw StateError(message);
        }
      }
    } on Object catch (error) {
      cancellationError = error;
    }
    if (_disposed) return;
    // native 会停掉本地校准状态（即使取消帧没发出去也会），这里读回真实状态：
    // 可能是 idle/cancelled，也可能是仍在进行的原状态。停在“取消中”不动会让
    // 界面永远卡住，所以一定要读回一次。
    await _pollCalibrationStatus();
    if (_disposed) return;
    if (cancellationError != null) {
      calibrationError = _friendlyError(cancellationError);
      _notifyCalibrationChanges();
      notifyListeners();
    }
  }

  void _startCalibrationPolling() {
    _calibrationTimer?.cancel();
    _calibrationTimer = Timer.periodic(
      const Duration(milliseconds: 250),
      (_) => unawaited(_pollCalibrationStatus()),
    );
  }

  Future<void> _pollCalibrationStatus() async {
    final id = sessionId;
    if (_disposed || id == null) return;
    try {
      final raw = calibrationBackend.status();
      if (_disposed || sessionId != id) return;
      final next = CalibrationStatus(
        active: raw['active'] == true,
        cancelling: calibrationStatus.cancelling && raw['active'] == true,
        type: _calibrationType(raw['type']?.toString()),
        stage: raw['stage']?.toString() ?? 'idle',
        progress: (raw['progress'] as num?)?.toInt() ?? 0,
        currentStep: (raw['current_step'] as num?)?.toInt() ?? 0,
        totalSteps: (raw['total_steps'] as num?)?.toInt() ?? 0,
        message: raw['message']?.toString() ?? '',
        failureReason: raw['failure_reason']?.toString() ?? '',
        accelerometerMode: raw['accelerometer_mode']?.toString() ?? '',
        compassProgress: {
          for (final entry
              in (raw['compass_progress'] as List<Object?>? ?? const [])
                  .whereType<Map<String, Object?>>())
            if (entry['compass_id'] is num && entry['completion_pct'] is num)
              (entry['compass_id'] as num).toInt():
                  (entry['completion_pct'] as num).toInt(),
        },
      );
      final changed =
          jsonEncode(next.toJson()) != jsonEncode(calibrationStatus.toJson());
      calibrationStatus = next;
      if (!next.active) _calibrationTimer?.cancel();
      if (changed) {
        calibrationError = null;
        _notifyCalibrationChanges();
        notifyListeners();
      }
    } on Object catch (error) {
      calibrationError = _friendlyError(error);
      _notifyCalibrationChanges();
      notifyListeners();
    }
  }

  CalibrationType _calibrationType(String? value) => switch (value) {
    'accelerometer' => CalibrationType.accelerometer,
    'magnetometer' => CalibrationType.magnetometer,
    'barometer' => CalibrationType.barometer,
    'radio' => CalibrationType.radio,
    _ => CalibrationType.gyroscope,
  };

  String _friendlyError(Object error) =>
      error.toString().replaceFirst('Bad state: ', '');

  bool _isProfileParameter(String name) =>
      name == 'Q_ENABLE' ||
      name == 'Q_TAILSIT_ENABLE' ||
      name == 'Q_TILT_ENABLE';

  void _recordParameterWriteError(String name, String message) {
    _parameterWriteErrors.remove(name);
    _parameterWriteErrors[name] = message;
  }

  List<String> get serialParameterPorts {
    final prefixes = <String>{};
    final pattern = RegExp(
      r'^(SERIAL\d+|SER_TEL\d+|SER_GPS\d+)_(?:PROTOCOL|PROT|BAUD)$',
    );
    for (final name in parameters.keys) {
      final match = pattern.firstMatch(name);
      if (match != null) prefixes.add(match.group(1)!);
    }
    return prefixes.toList()..sort(_compareSerialParameterPorts);
  }

  int _compareSerialParameterPorts(String left, String right) {
    int rank(String value) => value.startsWith('SERIAL')
        ? 0
        : value.startsWith('SER_TEL')
        ? 1
        : 2;
    final rankComparison = rank(left).compareTo(rank(right));
    if (rankComparison != 0) return rankComparison;
    final leftIndex =
        int.tryParse(RegExp(r'\d+$').stringMatch(left) ?? '') ?? 0;
    final rightIndex =
        int.tryParse(RegExp(r'\d+$').stringMatch(right) ?? '') ?? 0;
    return leftIndex.compareTo(rightIndex);
  }

  bool get canConnect {
    if (_busy || initializationError != null) return false;
    if (transport == ConnectionTransport.serial) return selectedPort != null;
    final host = transport == ConnectionTransport.udp ? udpHost : tcpHost;
    final portText = transport == ConnectionTransport.udp
        ? udpPortText
        : tcpPortText;
    final port = int.tryParse(portText.trim());
    return host.trim().isNotEmpty &&
        !host.contains(RegExp(r'\s')) &&
        !host.contains(':') &&
        port != null &&
        port > 0 &&
        port <= 65535;
  }

  String get selectedTransportLabel {
    final endpoint =
        (_activeLinkId == null ? null : fleetSnapshot.link(_activeLinkId!))
            ?.endpoint ??
        fleetSnapshot.activeVehicle?.endpoint;
    if (endpoint != null) {
      return switch (endpoint.split('://').first) {
        'serial' => '串口',
        'tcp' => 'TCP',
        _ => 'UDP',
      };
    }
    return switch (transport) {
      ConnectionTransport.serial => '串口',
      ConnectionTransport.udp => 'UDP',
      ConnectionTransport.tcp => 'TCP',
    };
  }

  void selectTransport(ConnectionTransport value) {
    if (_busy || transport == value) return;
    transport = value;
    message = null;
    _notifyConnectionChanges();
    notifyListeners();
    if (value == ConnectionTransport.serial && ports.isEmpty) {
      unawaited(refreshPorts());
    }
  }

  void updateNetworkEndpoint({required String host, required String port}) {
    if (_busy) return;
    if (transport == ConnectionTransport.udp) {
      udpHost = host;
      udpPortText = port;
    } else if (transport == ConnectionTransport.tcp) {
      tcpHost = host;
      tcpPortText = port;
    }
    _notifyConnectionChanges();
    notifyListeners();
  }

  void requestParameters({bool force = false}) {
    if (!isConnected || initializationError != null) return;
    if (_parameterRequestStarted && !force) return;
    parameters.clear();
    _parameterMetadataLoaded.clear();
    _parameterMetadataAttempts.clear();
    parameterReceived = 0;
    parameterTotal = 0;
    parametersLoaded = false;
    parametersLoading = true;
    parameterError = null;
    _parameterWriteErrors.clear();
    _parameterRequestStarted = false;
    try {
      final result = parameterBackend.requestAll();
      if (result['success'] != true) {
        throw StateError(ffiErrorText(result, '无法请求飞控参数'));
      }
      _parameterRequestStarted = true;
    } on Object catch (error) {
      parametersLoading = false;
      parameterError = _friendlyError(error);
    }
    _notifyParameterChanges();
    notifyListeners();
  }

  Future<void> refreshSerialHardwareMappings({bool force = false}) async {
    if (!isConnected || serialHardwareMappingLoading) return;
    if (_serialHardwareMappingRequested && !force) return;
    final generation = _sessionGeneration;
    _serialHardwareMappingRequested = true;
    serialHardwareMappingLoading = true;
    serialHardwareMappingError = null;
    _notifyParameterChanges();
    notifyListeners();
    await Future<void>.delayed(Duration.zero);
    if (_disposed || generation != _sessionGeneration) return;
    try {
      final result = _native!.serialUartNames() as Map<String, Object?>;
      final items = result['items'] as List<Object?>? ?? const [];
      serialHardwareMappings
        ..clear()
        ..addEntries(
          items
              .whereType<Map<String, Object?>>()
              .where((item) {
                return item['serial_index'] is int && item['name'] is String;
              })
              .map((item) {
                final serial = 'SERIAL${item['serial_index']}';
                return MapEntry(serial, item['name'] as String);
              }),
        );
      if (result['success'] != true) {
        serialHardwareMappingError = ffiErrorText(result, '读取串口硬件映射失败');
      }
    } on Object catch (error) {
      serialHardwareMappings.clear();
      serialHardwareMappingError = error.toString();
    } finally {
      serialHardwareMappingLoading = false;
      _notifyParameterChanges();
      notifyListeners();
    }
  }

  void loadParameterMetadata(Iterable<String> names) {
    _pendingParameterMetadata.addAll(names);
    if (parametersLoaded && !_loadingParameterMetadata) {
      unawaited(_loadParameterMetadataInBatches());
    }
  }

  Future<void> _loadParameterMetadataInBatches() async {
    if (_loadingParameterMetadata || !parametersLoaded) return;
    final generation = _sessionGeneration;
    _loadingParameterMetadata = true;
    try {
      while (_pendingParameterMetadata.isNotEmpty &&
          !_disposed &&
          generation == _sessionGeneration) {
        final names = _pendingParameterMetadata.take(8).toList();
        var changed = false;
        for (final name in names) {
          _pendingParameterMetadata.remove(name);
          final parameter = parameters[name];
          if (parameter == null || _parameterMetadataLoaded.contains(name)) {
            continue;
          }
          try {
            final descriptor = parameterBackend.descriptor(name);
            parameters[name] = parameter.withDescriptor(descriptor);
            _parameterMetadataLoaded.add(name);
            _parameterMetadataAttempts.remove(name);
            changed = true;
          } on Object {
            // 读取失败时保留原始值并稍后重试；否则枚举参数会永久退化成
            // 只显示数字的输入框。重试上限用于避免后端持续失败时空转。
            final attempts = (_parameterMetadataAttempts[name] ?? 0) + 1;
            _parameterMetadataAttempts[name] = attempts;
            if (attempts < 3) {
              _pendingParameterMetadata.add(name);
            }
          }
        }
        if (changed) {
          _notifyParameterChanges();
        }
        if (_pendingParameterMetadata.isNotEmpty) {
          await Future<void>.delayed(const Duration(milliseconds: 1));
        }
      }
    } finally {
      if (!_disposed && generation == _sessionGeneration) {
        _loadingParameterMetadata = false;
      }
    }
  }

  VehicleParameter? parameter(String name) => parameters[name];

  Future<bool> setParameter(String name, double value) async {
    final parameter = parameters[name];
    if (!isConnected || sessionId == null) {
      _recordParameterWriteError(name, '未连接飞控，参数未写入');
      _notifyParameterChanges();
      notifyListeners();
      return false;
    }
    // Capture the target at request time. If this write queues behind another
    // same-name write, it must never drift onto a newly connected vehicle.
    final requestedSessionId = sessionId!;
    if (parameter == null || parameter.readOnly || !value.isFinite) {
      _recordParameterWriteError(
        name,
        parameter == null
            ? '飞控未提供参数 $name'
            : parameter.readOnly
            ? '参数 $name 为只读项'
            : '参数值必须是有效数值',
      );
      _notifyParameterChanges();
      notifyListeners();
      return false;
    }

    final previous = _parameterWriteJobs[name];
    final completion = Completer<void>();
    final job = completion.future;
    _parameterWriteJobs[name] = job;
    _parameterWriteErrors.remove(name);
    _notifyParameterChanges();
    notifyListeners();

    VehicleParameter? latest;
    try {
      if (previous != null) await previous;
      if (_disposed) return false;
      _ensureWriteSession(requestedSessionId, name);
      _parameterWriteErrors.remove(name);
      _notifyParameterChanges();
      notifyListeners();

      // A value queued behind an in-flight write may already be current by
      // the time its turn arrives.
      latest = parameters[name];
      if (latest == null || latest.readOnly) {
        throw StateError('参数会话已变化，未能写入 $name');
      }
      if (latest.value == value) return true;

      // 乐观更新：点击后立即按目标值渲染，飞控确认后再用回读值校准。
      parameters[name] = latest.copyWith(value: value);
      _notifyParameterChanges();
      notifyListeners();

      for (var attempt = 0; attempt < 3; attempt++) {
        _ensureWriteSession(requestedSessionId, name);
        final writeBaseline = _latestParameterWriteId();
        final result = parameterBackend.write(name, value);
        if (result['success'] != true) {
          throw StateError(ffiErrorText(result, '参数写入失败'));
        }

        final acknowledgementTimer = Stopwatch()..start();
        while (acknowledgementTimer.elapsed < const Duration(seconds: 1)) {
          await Future<void>.delayed(const Duration(milliseconds: 100));
          _ensureWriteSession(requestedSessionId, name);
          final rejection = _parameterWriteRejection(name, writeBaseline);
          if (rejection != null) {
            throw StateError(rejection);
          }
          final acknowledgedValue = _readParameterValue(name);
          if (acknowledgedValue != null &&
              _parameterValuesMatch(
                value,
                acknowledgedValue,
                latest.mavlinkType,
              )) {
            parameters[name] = latest.copyWith(value: acknowledgedValue);
            _parameterWriteErrors.remove(name);
            if (_isProfileParameter(name)) _notifyProfileChanges();
            _notifyParameterChanges();
            notifyListeners();
            return true;
          }
        }
      }

      _ensureWriteSession(requestedSessionId, name);
      final actualValue = _readParameterValue(name);
      if (actualValue != null) {
        parameters[name] = latest.copyWith(value: actualValue);
      }
      throw StateError(
        actualValue == null
            ? '飞控未确认参数 $name 的新值，请检查连接后重试'
            : '飞控未接受参数 $name 的新值，当前值为 '
                  '${actualValue == actualValue.roundToDouble() ? actualValue.toInt() : actualValue}',
      );
    } on Object catch (error) {
      // 若写入失败，把乐观更新回滚到写入前的值。
      final previousValue = latest;
      if (previousValue != null) {
        final current = parameters[name];
        if (current != null &&
            _parameterValuesMatch(value, current.value, current.mavlinkType)) {
          parameters[name] = previousValue;
        }
      }
      _recordParameterWriteError(name, _friendlyError(error));
      _notifyParameterChanges();
      notifyListeners();
    } finally {
      if (identical(_parameterWriteJobs[name], job)) {
        _parameterWriteJobs.remove(name);
      }
      completion.complete();
      _notifyParameterChanges();
      notifyListeners();
    }
    return !_parameterWriteErrors.containsKey(name);
  }

  /// 发送前记录当前最大写入记录 id；之后只认这次写入新产生的终态，避免把
  /// 同名参数的历史失败记录误判成本次结果（阶段 1D 的 generation 语义）。
  int _latestParameterWriteId() {
    var maxId = 0;
    for (final record in parameterBackend.writes()) {
      final id = record['id'];
      if (id is num && id.toInt() > maxId) maxId = id.toInt();
    }
    return maxId;
  }

  /// 飞控用 PARAM_ERROR 明确拒绝时必须立即失败，并带上飞控给出的原因，
  /// 不再等到回读超时后显示笼统的“未确认”。仍处于 pending 的写入返回
  /// null，继续靠回读参数值确认。
  String? _parameterWriteRejection(String name, int baselineId) {
    const rejectedPrefix = 'parameter write rejected: ';
    for (final record in parameterBackend.writes()) {
      final id = record['id'];
      if (id is! num || id.toInt() <= baselineId) continue;
      if (record['name'] != name || record['state'] != 'failed') continue;
      final reason = record['error'];
      if (reason is! String || !reason.startsWith(rejectedPrefix)) continue;
      return '飞控拒绝写入参数 $name：${reason.substring(rejectedPrefix.length)}';
    }
    return null;
  }

  void _ensureWriteSession(int expectedSessionId, String name) {
    if (_disposed || !isConnected || sessionId != expectedSessionId) {
      throw StateError('连接已断开或切换，参数 $name 未能确认写入');
    }
  }

  double? _readParameterValue(String name) {
    final result = parameterBackend.byName(name);
    if (result['found'] != true) return null;
    return (result['value'] as num?)?.toDouble();
  }

  bool _parameterValuesMatch(double expected, double actual, int mavlinkType) {
    if (!expected.isFinite || !actual.isFinite) return false;
    if (mavlinkType >= 1 && mavlinkType <= 8) return expected == actual;
    final tolerance = (expected.abs() * 1e-6).clamp(1e-6, 1e-3);
    return (expected - actual).abs() <= tolerance;
  }

  void _initialize() {
    try {
      _native = NativeCore.open();
      _fleet = FleetController(
        FleetFfiBackend(_native!.library, _native!.handle),
        canChangeFocus: _canSwitchActiveVehicle,
        executeBatchCommand: _executeFleetBatchCommand,
      )..addListener(_handleFleetChanged);
      _fleetTimer = Timer.periodic(const Duration(milliseconds: 750), (_) {
        if (_disposed) return;
        try {
          fleet.refresh();
        } on Object {
          // Keep last fleet snapshot on transient reads; never change focus here.
        }
      });
      waveformImuPoints = Float32List(
        _waveformMaxPoints * _waveformFloatsPerPoint,
      );
      waveformMagPoints = Float32List(
        _waveformMaxPoints * _waveformFloatsPerPoint,
      );
      unawaited(refreshPorts());
    } on Object catch (error) {
      initializationError = '串口连接后端初始化失败：$error';
      notifyListeners();
    }
  }

  /// 取原生核心；未初始化成功时抛出，避免各处重复判空。
  NativeCore _requireNative() {
    final native = _native;
    if (native == null) {
      throw StateError(initializationError ?? '原生后端未初始化');
    }
    return native;
  }

  /// Cancellable long operations no longer pin the UI focus. Parameter
  /// writes and metadata/hardware reads remain non-movable because switching
  /// them mid-command could write to the wrong aircraft.
  bool _canSwitchActiveVehicle() {
    // First focus binding happens while connect() intentionally holds _busy.
    if (sessionId == null) return true;
    if (_busy) return false;
    if (_parameterWriteJobs.isNotEmpty) return false;
    if (serialHardwareMappingLoading || _loadingParameterMetadata) {
      return false;
    }
    final mission = sessionId == null
        ? _offlineMissionPlanning
        : _missionPlanningBySession[sessionId];
    if (mission?.transferActive == true) return true;
    return !(mission?.busy ?? false);
  }

  /// Switches UI focus after cancelling cancellable long operations belonging
  /// to the old vehicle. Command confirmations still remain pinned.
  void selectVehicle(int vehicleId) {
    if (_busy || vehicleId == sessionId) return;
    final previousVehicleId = sessionId;
    if (previousVehicleId != null) {
      _cancelOperationsForVehicle(previousVehicleId);
    }
    fleet.selectVehicle(vehicleId);
  }

  FleetLinkSummary? _preferredLinkForVehicle(int? vehicleId) {
    final links = fleetSnapshot.links;
    if (links.isEmpty) return null;
    if (vehicleId == null) return links.first;
    final candidates = links
        .where((link) => link.vehicleId == vehicleId)
        .toList(growable: false);
    if (candidates.isEmpty) return null;
    for (final link in candidates) {
      if (link.primary && link.connected) return link;
    }
    for (final link in candidates) {
      if (link.connected) return link;
    }
    for (final link in candidates) {
      if (link.primary) return link;
    }
    return candidates.first;
  }

  /// Keeps the physical link focus separate from the vehicle focus. A waiting
  /// link may have no vehicle yet, so it is intentionally retained until the
  /// user selects another link or it is closed.
  void _syncActiveLink({int? targetVehicleId}) {
    final links = fleetSnapshot.links;
    if (links.isEmpty) {
      _activeLinkId = null;
      return;
    }
    FleetLinkSummary? current;
    for (final link in links) {
      if (link.id == _activeLinkId) {
        current = link;
        break;
      }
    }
    final target = targetVehicleId ?? sessionId;
    if (current != null) {
      if (current.vehicleId == null) return;
      if (current.vehicleId == target) {
        final preferred = _preferredLinkForVehicle(target);
        if (preferred != null && preferred.primary && !current.primary) {
          _activeLinkId = preferred.id;
        }
        return;
      }
    }
    _activeLinkId = _preferredLinkForVehicle(target)?.id ?? links.first.id;
  }

  void _handleFleetChanged() {
    if (_disposed || _switchingSession) return;
    final target = fleet.snapshot.activeVehicleId;
    _syncActiveLink(targetVehicleId: target);
    if (target != sessionId) {
      // Backend removal is not a user switch: it must invalidate the old
      // session even while a download, disconnect or command is unfinished.
      if (target != null) {
        _switchToSession(target, forced: true);
      } else {
        _sessionGeneration++;
        _stopSessionPolling();
        _calibrationTimer?.cancel();
        sessionId = null;
        _bindLogSession(null);
        _resetRuntimeState();
        final waitingLink = _activeLinkId == null
            ? null
            : fleet.snapshot.link(_activeLinkId!);
        if (waitingLink != null) {
          phase = SerialConnectionPhase.waitingForHeartbeat;
          message = '$selectedTransportLabel 链路已打开，等待飞控心跳…';
          _startSessionPolling();
        } else {
          phase = SerialConnectionPhase.disconnected;
          message = '飞控连接已关闭';
        }
        _notifyTelemetryChanges();
        _notifyParameterChanges();
        _notifyProfileChanges();
        _notifyCalibrationChanges();
      }
    }
    final liveIds = fleet.snapshot.vehicles.map((v) => v.id).toSet();
    _runtimeSnapshots.removeWhere((id, _) => !liveIds.contains(id));
    // Keep detached mission controllers alive until UI listeners detach;
    // their beforeAccess guard rejects edits against a removed session.
    _notifyConnectionChanges();
    notifyListeners();
  }

  void _switchToSession(int target, {bool forced = false}) {
    if (!forced && !_canSwitchActiveVehicle()) {
      throw StateError('当前飞控有未完成操作，暂不能切换飞机');
    }
    final targetSummary = fleet.snapshot.vehicle(target);
    if (targetSummary == null) throw StateError('目标飞控会话不存在');
    _activeLinkId = _preferredLinkForVehicle(target)?.id ?? _activeLinkId;
    _switchingSession = true;
    try {
      final previous = sessionId;
      if (previous != null) {
        _runtimeSnapshots[previous] = _captureRuntimeSnapshot();
      }
      _stopSessionPolling();
      _calibrationTimer?.cancel();
      _sessionGeneration++;
      final snapshot = _runtimeSnapshots.remove(target);
      sessionId = target;
      if (snapshot == null) {
        _resetRuntimeState();
        phase = targetSummary.connected
            ? SerialConnectionPhase.connected
            : SerialConnectionPhase.waitingForHeartbeat;
        message = targetSummary.connected ? '飞控已连接' : '等待飞控心跳…';
      } else {
        _applyRuntimeSnapshot(snapshot);
      }
      // A cached goto marker is not proof that the aircraft still follows it.
      // Do not resurrect one after returning from another vehicle.
      _clearGuidedTarget();
      _lastFlightLatitude = null;
      _lastFlightLongitude = null;
      waveformImuPointCount = 0;
      waveformMagPointCount = 0;
      phase = targetSummary.connected
          ? SerialConnectionPhase.connected
          : SerialConnectionPhase.waitingForHeartbeat;
      _pollTelemetry(target); // Fresh native cache before publishing UI state.
      missionPlanning.bindSession(target);
      _bindLogSession(target);
      _startSessionPolling();
      unawaited(_signing.applyIfNeeded());
      _lastPublishedControlProfile = null;
      _notifyConnectionChanges();
      _notifyParameterChanges();
      _notifyTelemetryChanges();
      _notifyProfileChanges();
      _notifyCalibrationChanges();
      notifyListeners();
      unawaited(_pollSession());
      unawaited(refreshFlightModes());
      unawaited(refreshFlightMessages());
    } finally {
      _switchingSession = false;
    }
  }

  void _stopSessionPolling() {
    _stateTimer?.cancel();
    _stateTimer = null;
    _telemetryTimer?.cancel();
    _telemetryTimer = null;
    _waveformTimer?.cancel();
    _waveformTimer = null;
  }

  void _startSessionPolling() {
    _stateTimer?.cancel();
    _stateTimer = Timer.periodic(
      const Duration(milliseconds: 750),
      (_) => unawaited(_pollSession()),
    );
    _telemetryTimer?.cancel();
    _telemetryTimer = Timer.periodic(_telemetryPollInterval, (_) {
      final id = sessionId;
      if (id != null) _pollTelemetry(id);
    });
    _restartWaveformTimer();
  }

  _VehicleRuntimeSnapshot _captureRuntimeSnapshot() => _VehicleRuntimeSnapshot(
    phase: phase,
    message: message,
    telemetryError: telemetryError,
    calibrationStatus: calibrationStatus,
    calibrationError: calibrationError,
    satellitesVisible: satellitesVisible,
    gpsFixType: gpsFixType,
    headingDegrees: headingDegrees,
    airspeedMetersPerSecond: airspeedMetersPerSecond,
    groundSpeedMetersPerSecond: groundSpeedMetersPerSecond,
    relativeAltitudeMeters: relativeAltitudeMeters,
    absoluteAltitudeMeters: absoluteAltitudeMeters,
    climbRateMetersPerSecond: climbRateMetersPerSecond,
    throttlePercent: throttlePercent,
    rollDegrees: rollDegrees,
    pitchDegrees: pitchDegrees,
    batteryRemainingPercent: batteryRemainingPercent,
    batteryVoltage: batteryVoltage,
    firmwareInfo: firmwareInfo == null ? null : Map.of(firmwareInfo!),
    vehicleIdentity: vehicleIdentity == null ? null : Map.of(vehicleIdentity!),
    vehicleArmed: vehicleArmed,
    vehicleFlightMode: vehicleFlightMode,
    flightModesAvailable: flightModesAvailable,
    flightModeOptions: List.of(flightModeOptions),
    flightMessages: List.of(flightMessages),
    lastMessageSeq: _lastMessageSeq,
    vehicleType: vehicleType,
    vehicleAutopilotType: vehicleAutopilotType,
    vehicleLandedState: vehicleLandedState,
    vehicleVtolState: vehicleVtolState,
    guidedTargetPosition: guidedTargetPosition,
    guidedTargetValid: guidedTargetValid,
    guidedTargetModeObserved: _guidedTargetModeObserved,
    guidedTargetModeDeadline: _guidedTargetModeDeadline,
    flightTimeSeconds: flightTimeSeconds,
    flightDistanceMeters: flightDistanceMeters,
    flightStatsArmed: _flightStatsArmed,
    lastFlightLatitude: _lastFlightLatitude,
    lastFlightLongitude: _lastFlightLongitude,
    firmwareVersionInfo: firmwareVersionInfo == null
        ? null
        : Map.of(firmwareVersionInfo!),
    sensorHealth: sensorHealth == null ? null : Map.of(sensorHealth!),
    waveformWindowMs: waveformWindowMs,
    waveformImuInstances: List.of(waveformImuInstances),
    waveformMagInstances: List.of(waveformMagInstances),
    waveformImuInstance: waveformImuInstance,
    waveformMagInstance: waveformMagInstance,
    parameters: Map.of(parameters),
    parameterWriteErrors: Map.of(_parameterWriteErrors),
    parameterMetadataLoaded: Set.of(_parameterMetadataLoaded),
    parameterMetadataAttempts: Map.of(_parameterMetadataAttempts),
    pendingParameterMetadata: Set.of(_pendingParameterMetadata),
    parameterReceived: parameterReceived,
    parameterTotal: parameterTotal,
    parametersLoaded: parametersLoaded,
    parametersLoading: parametersLoading,
    parameterError: parameterError,
    servoOutputs: List.of(servoOutputs),
    servoOutputsValid: servoOutputsValid,
    rcChannels: List.of(rcChannels),
    rcChannelsValid: rcChannelsValid,
    vehiclePosition: vehiclePosition,
    vehiclePositionValid: vehiclePositionValid,
    homePosition: homePosition,
    homePositionValid: homePositionValid,
    serialHardwareMappings: Map.of(serialHardwareMappings),
    serialHardwareMappingLoading: serialHardwareMappingLoading,
    serialHardwareMappingError: serialHardwareMappingError,
    serialHardwareMappingRequested: _serialHardwareMappingRequested,
    firmwareVersionRequested: _firmwareVersionRequested,
    parameterRequestStarted: _parameterRequestStarted,
    loadingParameterMetadata: _loadingParameterMetadata,
  );

  void _applyRuntimeSnapshot(_VehicleRuntimeSnapshot snapshot) {
    _resetRuntimeState();
    phase = snapshot.phase;
    message = snapshot.message;
    telemetryError = snapshot.telemetryError;
    calibrationStatus = snapshot.calibrationStatus;
    calibrationError = snapshot.calibrationError;
    satellitesVisible = snapshot.satellitesVisible;
    gpsFixType = snapshot.gpsFixType;
    headingDegrees = snapshot.headingDegrees;
    airspeedMetersPerSecond = snapshot.airspeedMetersPerSecond;
    groundSpeedMetersPerSecond = snapshot.groundSpeedMetersPerSecond;
    relativeAltitudeMeters = snapshot.relativeAltitudeMeters;
    absoluteAltitudeMeters = snapshot.absoluteAltitudeMeters;
    climbRateMetersPerSecond = snapshot.climbRateMetersPerSecond;
    throttlePercent = snapshot.throttlePercent;
    rollDegrees = snapshot.rollDegrees;
    pitchDegrees = snapshot.pitchDegrees;
    batteryRemainingPercent = snapshot.batteryRemainingPercent;
    batteryVoltage = snapshot.batteryVoltage;
    firmwareInfo = snapshot.firmwareInfo == null
        ? null
        : Map.of(snapshot.firmwareInfo!);
    vehicleIdentity = snapshot.vehicleIdentity == null
        ? null
        : Map.of(snapshot.vehicleIdentity!);
    vehicleArmed = snapshot.vehicleArmed;
    vehicleFlightMode = snapshot.vehicleFlightMode;
    flightModesAvailable = snapshot.flightModesAvailable;
    flightModeOptions = List.of(snapshot.flightModeOptions);
    flightMessages = List.of(snapshot.flightMessages);
    _lastMessageSeq = snapshot.lastMessageSeq;
    vehicleType = snapshot.vehicleType;
    vehicleAutopilotType = snapshot.vehicleAutopilotType;
    vehicleLandedState = snapshot.vehicleLandedState;
    vehicleVtolState = snapshot.vehicleVtolState;
    guidedTargetPosition = snapshot.guidedTargetPosition;
    guidedTargetValid = snapshot.guidedTargetValid;
    _guidedTargetModeObserved = snapshot.guidedTargetModeObserved;
    _guidedTargetModeDeadline = snapshot.guidedTargetModeDeadline;
    flightTimeSeconds = snapshot.flightTimeSeconds;
    flightDistanceMeters = snapshot.flightDistanceMeters;
    _flightStatsArmed = snapshot.flightStatsArmed;
    _lastFlightLatitude = snapshot.lastFlightLatitude;
    _lastFlightLongitude = snapshot.lastFlightLongitude;
    _flightStopwatch
      ..stop()
      ..reset();
    _flightStopwatchOffsetSeconds = snapshot.flightTimeSeconds;
    if (_flightStatsArmed) _flightStopwatch.start();
    firmwareVersionInfo = snapshot.firmwareVersionInfo == null
        ? null
        : Map.of(snapshot.firmwareVersionInfo!);
    sensorHealth = snapshot.sensorHealth == null
        ? null
        : Map.of(snapshot.sensorHealth!);
    waveformWindowMs = snapshot.waveformWindowMs;
    waveformImuInstances = List.of(snapshot.waveformImuInstances);
    waveformMagInstances = List.of(snapshot.waveformMagInstances);
    waveformImuInstance = snapshot.waveformImuInstance;
    waveformMagInstance = snapshot.waveformMagInstance;
    parameters
      ..clear()
      ..addAll(snapshot.parameters);
    _parameterWriteErrors
      ..clear()
      ..addAll(snapshot.parameterWriteErrors);
    _parameterMetadataLoaded
      ..clear()
      ..addAll(snapshot.parameterMetadataLoaded);
    _parameterMetadataAttempts
      ..clear()
      ..addAll(snapshot.parameterMetadataAttempts);
    _pendingParameterMetadata
      ..clear()
      ..addAll(snapshot.pendingParameterMetadata);
    parameterReceived = snapshot.parameterReceived;
    parameterTotal = snapshot.parameterTotal;
    parametersLoaded = snapshot.parametersLoaded;
    parametersLoading = snapshot.parametersLoading;
    parameterError = snapshot.parameterError;
    servoOutputs = List.of(snapshot.servoOutputs);
    servoOutputsValid = snapshot.servoOutputsValid;
    rcChannels = List.of(snapshot.rcChannels);
    rcChannelsValid = snapshot.rcChannelsValid;
    vehiclePosition = snapshot.vehiclePosition;
    vehiclePositionValid = snapshot.vehiclePositionValid;
    homePosition = snapshot.homePosition;
    homePositionValid = snapshot.homePositionValid;
    serialHardwareMappings
      ..clear()
      ..addAll(snapshot.serialHardwareMappings);
    serialHardwareMappingLoading = snapshot.serialHardwareMappingLoading;
    serialHardwareMappingError = snapshot.serialHardwareMappingError;
    _serialHardwareMappingRequested = snapshot.serialHardwareMappingRequested;
    _firmwareVersionRequested = snapshot.firmwareVersionRequested;
    _parameterRequestStarted = snapshot.parameterRequestStarted;
    _loadingParameterMetadata = snapshot.loadingParameterMetadata;
    _lastSystemStatusPollMs = 0;
  }

  // ================================================================
  // 引导操作与任务读取（对标 QGC GuidedActionsController / PlanView）
  // ================================================================

  void _requireConnected(String action) {
    if (!isConnected) throw StateError('未连接飞控，无法$action');
    fleet.refresh();
    if (fleet.snapshot.activeVehicleId != sessionId) {
      throw StateError('目标飞控会话已改变，无法$action');
    }
  }

  void _requireFreshVehicleState(String action) {
    _requireConnected(action);
    if (!vehicleStateFresh) {
      throw StateError('飞控状态尚未刷新，无法$action');
    }
  }

  /// 增量拉取飞控消息（只取 seq 比上次大的），追加到本地列表。
  ///
  /// 后端保留最近 200 条，这里同样限长，避免长时间连接后无限增长。
  Future<void> refreshFlightMessages() async {
    if (!isConnected) return;
    final id = sessionId;
    if (id == null) return;
    try {
      final response =
          _native!.messages(_lastMessageSeq) as Map<String, Object?>;
      if (_disposed || sessionId != id) return;
      if (response['success'] != true) return;
      final items = (response['messages'] as List<Object?>? ?? const [])
          .whereType<Map<Object?, Object?>>()
          .toList(growable: false);
      if (items.isEmpty) return;
      final next = <FlightMessage>[...flightMessages];
      for (final raw in items) {
        final seq = (raw['seq'] as num?)?.toInt() ?? 0;
        if (seq <= _lastMessageSeq) continue;
        _lastMessageSeq = seq;
        final timestamp = (raw['timestamp_ms'] as num?)?.toInt() ?? 0;
        next.add(
          FlightMessage(
            seq: seq,
            time: timestamp > 0
                ? DateTime.fromMillisecondsSinceEpoch(timestamp)
                : null,
            severity: (raw['severity'] as num?)?.toInt() ?? 6,
            severityName: raw['severity_name'] as String? ?? '',
            text: raw['text'] as String? ?? '',
          ),
        );
      }
      const maxMessages = 200;
      if (next.length > maxMessages) {
        next.removeRange(0, next.length - maxMessages);
      }
      flightMessages = next;
      notifyListeners();
    } on Object {
      // 消息拉取失败不影响其它遥测。
    }
  }

  /// 只清空本地消息列表；后端缓冲保留（重新连接后不会把清掉的又拉回来，
  /// 因为 _lastMessageSeq 不回退）。
  void clearFlightMessages() {
    flightMessages = const [];
    notifyListeners();
  }

  /// 拉取飞行模式表（模式名 + 是否可切换）。参考 QGC `Vehicle::flightModes()`
  /// 与 MP `CMB_modes`：列表按机型给，MC 与固定翼的模式号不同，不能混用。
  Future<void> refreshFlightModes() async {
    if (!isConnected) {
      if (!flightModesAvailable && flightModeOptions.isEmpty) return;
      flightModesAvailable = false;
      flightModeOptions = const [];
      notifyListeners();
      return;
    }
    final id = sessionId;
    if (id == null) return;
    try {
      final response = _native!.flightModes() as Map<String, Object?>;
      if (_disposed || sessionId != id) return;
      if (response['success'] != true) return;
      final options = (response['modes'] as List<Object?>? ?? const [])
          .whereType<Map<Object?, Object?>>()
          .map(
            (raw) => FlightModeOption(
              id: (raw['id'] as num?)?.toInt() ?? 0,
              name: raw['name'] as String? ?? '',
              settable: raw['settable'] == true,
            ),
          )
          .where((option) => option.name.isNotEmpty)
          .toList(growable: false);
      flightModesAvailable = response['available'] == true;
      flightModeOptions = options;
      final current = response['current_mode'] as String? ?? '';
      // S5：心跳过期时最后一帧模式不是当前值，宁可保留旧值让界面标记过期。
      if (current.isNotEmpty && response['heartbeat_fresh'] == true) {
        vehicleFlightMode = current;
      }
      notifyListeners();
    } on Object {
      // 轮询失败保持上一次结果，不打断其它遥测。
    }
  }

  /// 按名字切换飞行模式（MP 的 setMode(name) / QGC 的 flightMode 属性写法）。
  Future<void> setFlightMode(String name) async {
    _requireFreshVehicleState('切换飞行模式');
    final options = flightModeOptions.where(
      (mode) => mode.name == name && mode.settable,
    );
    if (options.isEmpty) throw StateError('当前飞控不支持模式 $name');
    final target = fleet.captureTarget(expectedVehicleId: sessionId);
    await fleet.runForTarget(target, () async {
      _requireCommandSuccess(
        await flightCommandBackend.setMode(
          vehicleArmed ? 0x81 : 0x01,
          options.first.id,
        ),
        '切换到 $name',
      );
      await refreshFlightModes();
    });
  }

  /// 飞控明确拒绝的 MAV_RESULT 字符串，与 bridge/ffi/ffi_flight_commands.cc 一致。
  static const _rejectedAckResults = {
    'temporarily_rejected',
    'denied',
    'unsupported',
    'failed',
    'in_progress',
  };

  /// 失败原因文案：飞控明确拒绝时用本地化说明，否则用规范错误 message。
  String _commandFailureText(JsonMap raw) {
    final result = raw['result_str'];
    if (result is String && _rejectedAckResults.contains(result)) {
      return _commandAckReason(raw);
    }
    return ffiErrorText(
      raw,
      raw['result_text'] as String? ?? _commandAckReason(raw),
    );
  }

  void _requireCommandSuccess(Object? raw, String action) {
    if (raw is! JsonMap) throw const FormatException('指令响应格式无效');
    if (raw['success'] == true) return;
    throw StateError('$action失败：${_commandFailureText(raw)}');
  }

  /// MV6 批量动作的显式目标执行器。每个目标都经
  /// 显式提交/轮询接口按 vehicle_id 下发，不读取或修改
  /// active focus；单架失败只记录在该架飞机的结果中，继续执行其余目标。
  Future<FleetBatchCommandResult> _executeFleetBatchCommand(
    FleetVehicleSummary vehicle,
    FleetBatchAction action,
  ) async {
    if (_disposed || initializationError != null) {
      return FleetBatchCommandResult.failure(
        vehicleId: vehicle.id,
        systemId: vehicle.systemId,
        status: FleetBatchCommandStatus.error,
        message: initializationError ?? '多机后端已关闭',
      );
    }
    fleet.refresh();
    final current = fleet.snapshot.vehicle(vehicle.id);
    if (current == null || !current.stateFresh || !current.identified) {
      return FleetBatchCommandResult.failure(
        vehicleId: vehicle.id,
        systemId: vehicle.systemId,
        status: FleetBatchCommandStatus.error,
        message: '飞机已断开或尚未完成识别',
      );
    }
    final (command, args) = switch (action) {
      FleetBatchAction.arm => ('arm', <String, Object?>{'force': 0}),
      FleetBatchAction.disarm => ('disarm', <String, Object?>{'force': 0}),
      FleetBatchAction.returnToLaunch => ('rtl', <String, Object?>{}),
      FleetBatchAction.startMission => (
        'start_mission',
        <String, Object?>{'arm_if_needed': 1},
      ),
    };
    try {
      final raw = await flightCommandBackend.executeRawForVehicle(
        vehicle.id,
        command,
        args,
      );
      if (raw['success'] == true) {
        return FleetBatchCommandResult.success(
          vehicleId: vehicle.id,
          systemId: vehicle.systemId,
        );
      }
      final status = _fleetBatchFailureStatus(raw);
      return FleetBatchCommandResult.failure(
        vehicleId: vehicle.id,
        systemId: vehicle.systemId,
        status: status,
        message: _fleetBatchFailureText(raw),
      );
    } on Object catch (error) {
      return FleetBatchCommandResult.failure(
        vehicleId: vehicle.id,
        systemId: vehicle.systemId,
        status: FleetBatchCommandStatus.error,
        message: error.toString().replaceFirst('Bad state: ', ''),
      );
    }
  }

  FleetBatchCommandStatus _fleetBatchFailureStatus(JsonMap raw) {
    final result = raw['result_str'];
    if (result is String && _rejectedAckResults.contains(result)) {
      return FleetBatchCommandStatus.rejected;
    }
    if (ffiErrorCode(raw) == -6 ||
        raw['result_text'].toString().toLowerCase().contains('timeout')) {
      return FleetBatchCommandStatus.timeout;
    }
    return FleetBatchCommandStatus.error;
  }

  String _fleetBatchFailureText(JsonMap raw) => _commandFailureText(raw);

  String _commandAckReason(JsonMap raw) {
    return switch (raw['result_str']) {
      'temporarily_rejected' => '飞控暂时拒绝了该指令',
      'denied' => '飞控拒绝了该指令',
      'unsupported' => '飞控不支持该指令',
      'failed' => '飞控执行该指令失败',
      'in_progress' => '飞控仍在执行该指令',
      _ => '飞控拒绝了该指令',
    };
  }

  /// 移动到此处（QGC 的 Go To Location，走 COMMAND_INT 保证精度）。
  Future<void> guidedGoto(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) async {
    _requireFreshVehicleState('移动到此处');
    // QGC `APMFirmwarePlugin::guidedModeGotoLocation` 在高度未知时直接返回、不发指令。
    // DO_REPOSITION 走 MAV_FRAME_GLOBAL，高度必须是 AMSL 绝对值；这里如果拿到 0
    // 或非有限值就拒发——否则等于命令飞机飞向地面高度。
    if (!altitudeMeters.isFinite || altitudeMeters == 0) {
      throw StateError('移动到此处失败：没有有效的海拔高度');
    }
    _requireCommandSuccess(
      await flightCommandBackend.guidedPoint(
        latitude,
        longitude,
        altitudeMeters,
        -1,
        -1,
      ),
      '移动到此处',
    );
    guidedTargetPosition = VehicleFix(
      latitude: latitude,
      longitude: longitude,
      altitudeMeters: altitudeMeters,
    );
    guidedTargetValid = true;
    _guidedTargetModeObserved = isGuidedGotoFlightMode;
    _guidedTargetModeDeadline = _guidedTargetModeObserved
        ? null
        : DateTime.now().add(_guidedTargetModeTransitionTimeout);
    _notifyTelemetryChanges();
    notifyListeners();
  }

  Future<void> guidedReturnToLaunch() async {
    _requireFreshVehicleState('返航');
    _requireCommandSuccess(await flightCommandBackend.rtl(), '返航');
  }

  Future<void> guidedLand() async {
    _requireFreshVehicleState('降落');
    _requireCommandSuccess(await flightCommandBackend.land(), '降落');
  }

  Future<void> guidedTakeoff(double altitudeMeters) async {
    _requireFreshVehicleState('起飞');
    if (vehicleFlying) {
      throw StateError('起飞失败：飞机已经在空中');
    }
    if (!supportsGuidedTakeoff) {
      throw StateError('起飞失败：当前机型不支持引导起飞');
    }
    // QGC `_guidedModeTakeoff` 在 `altitudeAMSL` 为 NaN 时直接拒绝。PX4 把
    // 相对高度换算成 AMSL 后才能发命令，所以还要求位置有效（bridge 侧也会拒绝）。
    if (absoluteAltitudeMeters == null ||
        (vehicleAutopilotType == 12 && !vehiclePositionValid)) {
      throw StateError('起飞失败：飞机位置未知');
    }
    // QGC：起飞高度取 `max(飞控最小起飞高度, 用户值)`，用户不可能把它调低。
    var takeoffAltitude = altitudeMeters;
    final minimumAltitude = minimumTakeoffAltitudeMeters;
    if (!takeoffAltitude.isFinite || takeoffAltitude < minimumAltitude) {
      takeoffAltitude = minimumAltitude;
    }

    // QGC PX4 flow sends takeoff first and arms after the command is accepted.
    // ArduPilot requires GUIDED and an armed vehicle before NAV_TAKEOFF.
    if (vehicleAutopilotType == 12) {
      _requireCommandSuccess(
        await flightCommandBackend.takeoff(takeoffAltitude),
        '起飞',
      );
      if (!vehicleArmed) await _armVehicleAndValidate();
      return;
    }

    await _setFlightModeAndValidate(_guidedCustomMode, 'Guided');
    if (!vehicleArmed) await _armVehicleAndValidate();
    _requireCommandSuccess(
      await flightCommandBackend.takeoff(takeoffAltitude),
      '起飞',
    );
  }

  /// Shared native workflow, also used by explicitly targeted batch starts.
  /// QGC APM: Plane/VTOL AUTO before arm; Copter GUIDED before arm.
  /// PX4 uses AUTO_MISSION. Worker validates ACK and heartbeat state.
  Future<void> startMission() async {
    _requireFreshVehicleState('启动任务');
    _requireCommandSuccess(await flightCommandBackend.startMission(), '启动任务');
  }

  Future<void> _setFlightModeAndValidate(
    int customMode,
    String expectedMode,
  ) async {
    if (_flightModeMatches(expectedMode)) return;

    for (var attempt = 0; attempt < 3; attempt++) {
      _requireCommandSuccess(
        await flightCommandBackend.setMode(
          _mavModeFlagCustomModeEnabled,
          customMode,
        ),
        '切换飞行模式',
      );
      if (await _waitForVehicleState(
        mode: expectedMode,
        timeout: _flightModeValidationWindow,
      )) {
        return;
      }
    }
    throw StateError('切换飞行模式失败：飞控未进入 $expectedMode');
  }

  /// 解锁。安全敏感动作，调用方（界面）必须先确认。
  ///
  /// 返回 true 表示确实下发了命令并已被飞控确认；返回 false 表示飞控已经处于
  /// 解锁状态、没有下发任何命令。调用方必须区分这两种结果：以前这里静默返回，
  /// 界面在“已经解锁”时按了长按确认会什么都不发生也不提示（S1）。
  Future<bool> armVehicle() async {
    _requireFreshVehicleState('解锁');
    return _armVehicleAndValidate();
  }

  /// 上锁。飞行中上锁会掉飞机，界面必须确认。返回值语义同 [armVehicle]。
  Future<bool> disarmVehicle() async {
    _requireFreshVehicleState('上锁');
    return _disarmVehicleAndValidate();
  }

  /// 急停：强制上锁（COMPONENT_ARM_DISARM，param2=21196）。
  ///
  /// 与 [disarmVehicle] 的区别是它让飞控跳过解锁前检查，用于常规上锁被飞控拒绝
  /// 的场合——`safety.md` 的 S3 里列的"急停"就是这一条，可恢复（可以再解锁）。
  /// 它**不是**终止飞行（DO_FLIGHTTERMINATION，不可撤销），两者在界面上必须分开。
  /// 返回 true 表示下发了强制上锁；false 表示飞控本来就未解锁，无需急停。
  Future<bool> emergencyDisarmVehicle() async {
    _requireFreshVehicleState('急停');
    if (!vehicleArmed) return false;
    _requireCommandSuccess(await flightCommandBackend.disarm(1), '急停');
    if (await _waitForVehicleState(
      armed: false,
      timeout: _armValidationWindow,
    )) {
      return true;
    }
    throw StateError('急停失败：飞控仍报告 armed=true');
  }

  /// 返回是否真的下发了命令。已经解锁时返回 false，不当作失败（S1）。
  Future<bool> _armVehicleAndValidate() async {
    if (vehicleArmed) return false;
    _requireCommandSuccess(await flightCommandBackend.arm(0), '解锁');
    if (await _waitForVehicleState(
      armed: true,
      timeout: _armValidationWindow,
    )) {
      return true;
    }
    throw StateError('解锁失败：飞控未报告 armed=true');
  }

  /// 返回是否真的下发了命令。已经上锁时返回 false，不当作失败（S1）。
  Future<bool> _disarmVehicleAndValidate() async {
    if (!vehicleArmed) return false;
    _requireCommandSuccess(await flightCommandBackend.disarm(0), '上锁');
    if (await _waitForVehicleState(
      armed: false,
      timeout: _armValidationWindow,
    )) {
      return true;
    }
    throw StateError('上锁失败：飞控仍报告 armed=true');
  }

  bool _flightModeMatches(String expectedMode) =>
      vehicleFlightMode.trim().toLowerCase() ==
      expectedMode.trim().toLowerCase();

  Future<bool> _waitForVehicleState({
    String? mode,
    bool? armed,
    required Duration timeout,
  }) async {
    final target = fleet.captureTarget(expectedVehicleId: sessionId);
    final deadline = DateTime.now().add(timeout);
    while (!_disposed && DateTime.now().isBefore(deadline)) {
      fleet.validateTarget(target);
      if (_readVehicleSummaryForCommand()) {
        final modeMatches = mode == null || _flightModeMatches(mode);
        final armedMatches = armed == null || vehicleArmed == armed;
        if (modeMatches && armedMatches) return true;
      }
      await Future<void>.delayed(_commandStatePollInterval);
    }
    if (_disposed) return false;
    fleet.validateTarget(target);
    if (!_readVehicleSummaryForCommand()) return false;
    return (mode == null || _flightModeMatches(mode)) &&
        (armed == null || vehicleArmed == armed);
  }

  bool _readVehicleSummaryForCommand() {
    try {
      final envelope = telemetryBackend.vehicleSummary();
      final data = envelope['data'] as Map<String, Object?>? ?? const {};
      // S5/S2：只有新鲜心跳才能用来确认命令结果。过期帧可能仍停在命令前的
      // armed/mode，用它判断会把“命令没生效”误判成“已生效”。
      if (data['heartbeat_fresh'] != true) return false;
      vehicleArmed = data['armed'] == true;
      vehicleFlightMode = data['flight_mode'] as String? ?? '';
      vehicleType = data['vehicle_type'] as int? ?? vehicleType;
      vehicleAutopilotType =
          data['autopilot_type'] as int? ?? vehicleAutopilotType;
      notifyListeners();
      return true;
    } on Object {
      return false;
    }
  }

  /// Guided 模式的自定义模式号：固定翼/VTOL 用 15，多旋翼用 4。
  int get _guidedCustomMode =>
      controlProfile == VehicleControlProfile.fixedWing ||
          controlProfile == VehicleControlProfile.planeVtol
      ? 15
      : 4;

  /// 引导菜单的“改变高度”。
  ///
  /// 与 QGC `APMFirmwarePlugin::guidedModeChangeAltitude` 一致：
  /// 目标高度是**相对 Home** 的高度；先切 GUIDED，再发 `LOCAL_OFFSET_NED` 的
  /// 高度偏移（只动 z）。偏差小于 1 cm 时直接返回，避免无意义地切模式。
  Future<void> guidedChangeAltitude(double targetAltitudeAboveHome) async {
    _requireFreshVehicleState('改变高度');
    final current = relativeAltitudeMeters;
    if (!vehiclePositionValid || current == null) {
      throw StateError('改变高度失败：飞机高度未知');
    }
    final delta = targetAltitudeAboveHome - current;
    if (delta.abs() < 0.01) return;
    await _setFlightModeAndValidate(_guidedCustomMode, 'Guided');
    _requireCommandSuccess(
      await flightCommandBackend.changeAltitude(delta),
      '改变高度',
    );
  }

  Future<void> guidedPause() async {
    _requireFreshVehicleState('暂停');
    // ArduPilot's guided pause is a flight-mode change, not just a zero
    // velocity target. A one-shot velocity command can expire and silently
    // let the mission continue.
    if (vehicleAutopilotType != 12) {
      if (vehicleType == 10 || vehicleType == 11) {
        await _setFlightModeAndValidate(3, 'Hold');
        return;
      }
      if (controlProfile == VehicleControlProfile.copter) {
        await _setFlightModeAndValidate(17, 'Brake');
        return;
      }
      if (controlProfile == VehicleControlProfile.fixedWing ||
          controlProfile == VehicleControlProfile.planeVtol) {
        await _setFlightModeAndValidate(12, 'Loiter');
        return;
      }
    }
    _requireCommandSuccess(await flightCommandBackend.guidedPause(), '暂停');
  }

  Future<void> guidedOrbit({
    required double latitude,
    required double longitude,
    double radiusMeters = 50,
    double velocityMetersPerSecond = 5,
    double altitudeMeters = 50,
  }) async {
    _requireFreshVehicleState('环绕飞行');
    _requireCommandSuccess(
      await flightCommandBackend.orbit(
        latitude: latitude,
        longitude: longitude,
        radiusMeters: radiusMeters,
        velocityMetersPerSecond: velocityMetersPerSecond,
        altitudeMeters: altitudeMeters,
      ),
      '环绕飞行',
    );
    // 环绕会替换当前的水平目的地；旧 Go To 图标不应继续留在图上。
    _clearGuidedTarget();
  }

  /// 设置飞控 home。会改变返航基准，调用方必须先向用户确认。
  Future<void> setVehicleHome(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) async {
    _requireConnected('设置 Home');
    _requireCommandSuccess(
      await flightCommandBackend.setHome(latitude, longitude, altitudeMeters),
      '设置 Home',
    );
  }

  /// 地图菜单的“在此外设置兴趣点”：让云台/相机对准该坐标，飞机不移动。
  /// 参数 [altitudeMeters] 是**相对 Home 的高度**：ArduPilot 把 COMMAND_LONG 形式的
  /// `MAV_CMD_DO_SET_ROI_LOCATION` 映射成 `MAV_FRAME_GLOBAL_RELATIVE_ALT`，
  /// 这正是 QGC `APMFirmwarePlugin` 的 ROI 分支所用的坐标系（地图点击默认传 0）。
  Future<void> guidedSetRoi(
    double latitude,
    double longitude, [
    double altitudeMeters = 0,
  ]) async {
    _requireConnected('设置兴趣点');
    _requireCommandSuccess(
      await flightCommandBackend.setRoi(latitude, longitude, altitudeMeters),
      '设置兴趣点',
    );
  }

  /// 地图菜单的“设置航向”：机头转向点击坐标。
  ///
  /// 与 QGC `APMFirmwarePlugin::guidedModeChangeHeading` 一致：先算飞机到目标点
  /// 的方位角，再与当前航向求最短转向差，最后发相对角的 `MAV_CMD_CONDITION_YAW`。
  /// 偏航速率传 0，交给飞控用默认值（QGC 取不到 `ATC_RATE_Y_MAX` 时也是 0）。
  Future<void> guidedSetHeading(double latitude, double longitude) async {
    _requireConnected('设置航向');
    final fix = vehiclePosition;
    if (!vehiclePositionValid || fix == null) {
      throw StateError('设置航向失败：飞机位置未知');
    }
    final bearing = const Distance().bearing(
      LatLng(fix.latitude, fix.longitude),
      LatLng(latitude, longitude),
    );
    var delta = bearing - (headingDegrees ?? 0);
    while (delta < -180) {
      delta += 360;
    }
    while (delta > 180) {
      delta -= 360;
    }
    _requireCommandSuccess(
      await flightCommandBackend.changeHeading(
        delta.abs(),
        _headingYawRateDegreesPerSecond,
        delta >= 0 ? 1 : -1,
      ),
      '设置航向',
    );
  }

  /// 地图菜单的“设置估计原点”：给无 GPS 的室内/视觉定位指定本地坐标原点。
  Future<void> setEstimatorOrigin(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) async {
    _requireConnected('设置估计原点');
    _requireCommandSuccess(
      await flightCommandBackend.setGlobalOrigin(
        latitude,
        longitude,
        altitudeMeters,
      ),
      '设置估计原点',
    );
  }

  Future<void> refreshPorts() async {
    if (initializationError != null || _busy) return;
    _busy = true;
    message = null;
    _notifyConnectionChanges();
    notifyListeners();
    try {
      final result = _native!.listPorts();
      ports
        ..clear()
        ..addAll(
          (result as List<Object?>)
              .whereType<Map<String, Object?>>()
              .map(
                (item) => SerialPortInfo(
                  path: item['port_name'] as String? ?? '',
                  description: item['description'] as String? ?? '',
                  likelyFlightController:
                      item['is_likely_fc'] as bool? ?? false,
                ),
              )
              .where((port) => port.path.isNotEmpty),
        );
      if (selectedPort == null ||
          !ports.any((port) => port.path == selectedPort)) {
        selectedPort = ports.isEmpty
            ? null
            : ports
                  .firstWhere(
                    (port) => port.likelyFlightController,
                    orElse: () => ports.first,
                  )
                  .path;
      }
      if (ports.isEmpty) message = '未发现串口设备，请检查飞控连接后刷新。';
    } on Object catch (error) {
      message = '读取串口列表失败：$error';
    } finally {
      _busy = false;
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  void selectPort(String? path) {
    if (_busy || path == null) return;
    selectedPort = path;
    _notifyConnectionChanges();
    notifyListeners();
  }

  void selectBaudRate(int? value) {
    if (_busy || value == null) return;
    baudRate = value;
    _notifyConnectionChanges();
    notifyListeners();
  }

  Future<void> connect() async {
    if (!canConnect) return;
    final previousSessionId = sessionId;
    final transportAtConnect = transport;
    final serialPort = selectedPort;
    final host = transportAtConnect == ConnectionTransport.udp
        ? udpHost.trim()
        : tcpHost.trim();
    final portText = transportAtConnect == ConnectionTransport.udp
        ? udpPortText.trim()
        : tcpPortText.trim();
    final uri = switch (transportAtConnect) {
      ConnectionTransport.serial => 'serial://$serialPort?baud=$baudRate',
      ConnectionTransport.udp => 'udp://$host:$portText',
      ConnectionTransport.tcp => 'tcp://$host:$portText',
    };
    final transportLabel = switch (transportAtConnect) {
      ConnectionTransport.serial => '串口',
      ConnectionTransport.udp => 'UDP',
      ConnectionTransport.tcp => 'TCP',
    };
    _busy = true;
    message = transportAtConnect == ConnectionTransport.serial
        ? '正在打开串口并等待飞控心跳…'
        : '正在打开 $transportLabel 链路并等待飞控心跳…';
    _notifyConnectionChanges();
    notifyListeners();
    int? openedLinkId;
    try {
      final result = fleet.openLink(FleetConnectionSpec(uri));
      openedLinkId = result;
      await _attachLink(result, transportLabel: transportLabel);
      openedLinkId = null;
    } on Object catch (error) {
      message = _friendlyError(error);
      final openedId = openedLinkId;
      if (openedId != null) {
        try {
          fleet.closeLink(openedId);
        } on Object {
          // Keep the original connect error; backend disposal is best effort.
        }
      }
      if (previousSessionId == null || sessionId == null) {
        phase = SerialConnectionPhase.disconnected;
        _firmwareVersionRequested = false;
        _parameterRequestStarted = false;
        _stopSessionPolling();
      }
      _notifyConnectionChanges();
    } finally {
      _busy = false;
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  /// Attach the controller to a native link handle. A link can exist before a
  /// heartbeat has identified its vehicle, so this method must not select a
  /// vehicle by treating the handle as a vehicle ID.
  Future<void> _attachLink(int linkId, {required String transportLabel}) async {
    fleet.refresh();
    final link = fleetSnapshot.link(linkId);
    if (link == null) throw StateError('链路打开后未注册');

    if (sessionId == null) {
      _activeLinkId = linkId;
      final vehicleId = link.vehicleId;
      if (vehicleId != null) {
        _switchToSession(vehicleId, forced: true);
      } else {
        _lastSystemStatusPollMs = 0;
        phase = SerialConnectionPhase.waitingForHeartbeat;
        message = '$transportLabel 链路已打开，正在等待飞控心跳…';
        _startSessionPolling();
      }
    } else {
      final focusedVehicle = fleetSnapshot.activeVehicleId;
      if (focusedVehicle != null && focusedVehicle != sessionId) {
        _switchToSession(focusedVehicle, forced: true);
      }
      // QGC keeps the first vehicle focused when another link is added.
      message = '$transportLabel 链路已添加，等待飞控心跳…';
    }

    _notifyConnectionChanges();
    _notifyParameterChanges();
    await _pollSession();
  }

  Future<void> _pollSession() async {
    final capturedVehicleId = sessionId;
    final capturedLinkId = _activeLinkId;
    final id = capturedVehicleId ?? capturedLinkId;
    if (id == null || _disposed) return;
    final previousPhase = phase;
    final previousMessage = message;
    try {
      final Map<String, Object?> state;
      if (capturedVehicleId != null) {
        final envelope = telemetryBackend.sessionStatus(capturedVehicleId);
        if (envelope['success'] == true) {
          state =
              (envelope['data'] as Map<Object?, Object?>? ??
                      const <Object?, Object?>{})
                  .cast<String, Object?>();
        } else {
          state = <String, Object?>{'success': false};
        }
      } else {
        state = _native!.sessionState(id) as Map<String, Object?>;
      }
      fleet.refresh();
      // refresh() may synchronously rebind to a replacement session/link.
      if (_disposed ||
          sessionId != capturedVehicleId ||
          _activeLinkId != capturedLinkId) {
        return;
      }
      if (state['success'] != true) {
        final linkStillPresent =
            capturedLinkId != null &&
            fleetSnapshot.link(capturedLinkId) != null;
        if (linkStillPresent) {
          message = '读取连接状态失败，链路仍在等待飞控心跳。';
        } else {
          _stopSessionPolling();
          _calibrationTimer?.cancel();
          sessionId = null;
          _bindLogSession(null);
          phase = SerialConnectionPhase.disconnected;
          message = previousPhase == SerialConnectionPhase.waitingForHeartbeat
              ? '未收到飞控心跳，连接已关闭。请检查所选设备、波特率或数据链路。'
              : '飞控连接已关闭，请检查线缆或数据链路。';
          _firmwareVersionRequested = false;
          _parameterRequestStarted = false;
          _clearTelemetry();
        }
      } else if (state['heartbeat_alive'] == true) {
        if (capturedVehicleId != null) {
          unawaited(_signing.refreshAutoDetected(capturedVehicleId));
        }
        if (capturedVehicleId == null) {
          final discoveredVehicleId = capturedLinkId == null
              ? null
              : fleetSnapshot.link(capturedLinkId)?.vehicleId;
          if (discoveredVehicleId != null) {
            _switchToSession(discoveredVehicleId, forced: true);
            return;
          }
        }
        phase = SerialConnectionPhase.connected;
        message = '飞控已连接';
        unawaited(_signing.applyIfNeeded());
        if (previousPhase != SerialConnectionPhase.connected &&
            _waveformActive) {
          _applyWaveformStreamRate();
          _scheduleWaveformStreamRateRetry();
        }
        if (capturedVehicleId != null && !_firmwareVersionRequested) {
          _firmwareVersionRequested = true;
          try {
            _native!.requestFirmwareVersion();
          } on Object {
            // Firmware identification remains available from HEARTBEAT.
          }
        }
      } else {
        phase = SerialConnectionPhase.waitingForHeartbeat;
        message = '$selectedTransportLabel 链路已打开，等待飞控心跳…';
      }
      if (sessionId != null) {
        if (phase == SerialConnectionPhase.connected &&
            !_parameterRequestStarted &&
            !parametersLoading &&
            parameterError == null) {
          requestParameters();
        }
        _pollParameterDownload();
      }
      if (phase != previousPhase || message != previousMessage) {
        _notifyConnectionChanges();
        notifyListeners();
      }
    } on Object catch (error) {
      if (_disposed ||
          sessionId != capturedVehicleId ||
          _activeLinkId != capturedLinkId) {
        return;
      }
      message = '读取连接状态失败：$error';
      if (message != previousMessage) notifyListeners();
    }
  }

  void _ensureActiveSession(int id) {
    fleet.refresh();
    if (fleet.snapshot.activeVehicleId != id) {
      throw StateError('当前飞控焦点与连接会话不匹配');
    }
  }

  void _cancelOperationsForVehicle(int vehicleId) {
    for (final operation in const [
      'params',
      'mission',
      'fence',
      'rally',
      'calibration',
      'log',
    ]) {
      try {
        _native!.cancelOperationForVehicle(vehicleId, operation);
      } on Object {
        // Best effort: closing the link still tears down the transport.
      }
    }
  }

  /// Closes exactly one physical link. Closing a non-active link must not
  /// disturb the active aircraft; closing the active route lets Fleet select a
  /// remaining session and then rebinds the UI runtime state to it.
  Future<void> disconnectLink(int linkId) async {
    if (_busy) return;
    final link = fleetSnapshot.link(linkId);
    final legacyVehicle = link == null ? fleetSnapshot.vehicle(linkId) : null;
    if (link == null && legacyVehicle == null) {
      message = '链路不存在或已经断开';
      _notifyConnectionChanges();
      notifyListeners();
      return;
    }

    final linkedVehicleId = link?.vehicleId ?? legacyVehicle?.id;
    final wasActiveVehicle =
        linkedVehicleId != null && sessionId == linkedVehicleId;
    final wasActiveLink = _activeLinkId == linkId;
    final wasActive = wasActiveVehicle || wasActiveLink;

    _busy = true;
    _notifyConnectionChanges();
    notifyListeners();
    try {
      if (fleet.targetOperationActive) {
        throw StateError('指令执行中，暂不能关闭链路');
      }
      if (wasActiveVehicle) {
        _ensureActiveSession(linkedVehicleId);
      }
      if (wasActive && linkedVehicleId != null) {
        _cancelOperationsForVehicle(linkedVehicleId);
      }
      fleet.closeLink(linkId);
      if (linkedVehicleId != null &&
          fleetSnapshot.vehicle(linkedVehicleId) == null) {
        _runtimeSnapshots.remove(linkedVehicleId);
      }
      _syncActiveLink();

      if (wasActive) {
        // FleetController refreshes synchronously after close. Its listener
        // normally performs this rebind; the explicit fallback covers a
        // listener being detached during teardown or a failed notification.
        final replacement = fleetSnapshot.activeVehicleId;
        if (replacement != null && replacement != sessionId) {
          _switchToSession(replacement, forced: true);
          message = '已断开旧链路，已切换到飞机 $replacement';
        } else if (replacement != null) {
          _activeLinkId = _preferredLinkForVehicle(replacement)?.id;
          message = '链路已断开，当前飞机仍有其他链路';
          unawaited(_pollSession());
        } else {
          _sessionGeneration++;
          _stopSessionPolling();
          _calibrationTimer?.cancel();
          sessionId = null;
          _bindLogSession(null);
          _resetRuntimeState();
          final waitingLink = _activeLinkId == null
              ? null
              : fleetSnapshot.link(_activeLinkId!);
          if (waitingLink != null) {
            phase = SerialConnectionPhase.waitingForHeartbeat;
            message = '链路已打开，等待飞控心跳…';
            _startSessionPolling();
          } else {
            phase = SerialConnectionPhase.disconnected;
            message = '飞控连接已关闭';
          }
          _clearTelemetry();
        }
      } else {
        message = '链路已断开';
      }
    } on Object catch (error) {
      message = _friendlyError(error);
    } finally {
      _busy = false;
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  /// Closes one route selected in the fleet panel. A shared UDP/TCP link stays
  /// open for the other discovered vehicles; a waiting link has no route yet,
  /// so it falls back to closing the physical link.
  Future<void> disconnectFleetLink(FleetLinkSummary link) async {
    if (link.routeId < 0 || link.vehicleId == null) {
      await disconnectLink(link.id);
      return;
    }
    await disconnectRoute(link.routeId);
  }

  Future<void> disconnectRoute(int routeId) async {
    if (_busy) return;
    FleetLinkSummary? route;
    for (final link in fleetSnapshot.links) {
      if (link.routeId == routeId) {
        route = link;
        break;
      }
    }
    if (route == null) {
      message = '链路 route 不存在';
      _notifyConnectionChanges();
      notifyListeners();
      return;
    }
    final linkedVehicleId = route.vehicleId;
    final wasActive = linkedVehicleId != null && sessionId == linkedVehicleId;
    _busy = true;
    _notifyConnectionChanges();
    notifyListeners();
    try {
      if (fleet.targetOperationActive) {
        throw StateError('指令执行中，暂不能关闭链路');
      }
      if (wasActive) {
        _ensureActiveSession(linkedVehicleId);
        _cancelOperationsForVehicle(linkedVehicleId);
      }
      fleet.closeRoute(routeId);
      if (linkedVehicleId != null &&
          fleetSnapshot.vehicle(linkedVehicleId) == null) {
        _runtimeSnapshots.remove(linkedVehicleId);
      }
      _syncActiveLink();

      if (wasActive) {
        final replacement = fleetSnapshot.activeVehicleId;
        if (replacement != null && replacement != sessionId) {
          _switchToSession(replacement, forced: true);
          message = '已断开链路，已切换到飞机 $replacement';
        } else if (replacement != null) {
          _activeLinkId = _preferredLinkForVehicle(replacement)?.id;
          message = '已断开链路，当前飞机仍有其他链路';
          unawaited(_pollSession());
        } else {
          _sessionGeneration++;
          _stopSessionPolling();
          _calibrationTimer?.cancel();
          sessionId = null;
          _bindLogSession(null);
          _resetRuntimeState();
          final waitingLink = _activeLinkId == null
              ? null
              : fleetSnapshot.link(_activeLinkId!);
          if (waitingLink != null) {
            phase = SerialConnectionPhase.waitingForHeartbeat;
            message = '链路已打开，等待飞控心跳…';
            _startSessionPolling();
          } else {
            phase = SerialConnectionPhase.disconnected;
            message = '飞控连接已关闭';
          }
          _clearTelemetry();
        }
      } else {
        message = '链路已断开';
      }
    } on Object catch (error) {
      message = _friendlyError(error);
    } finally {
      _busy = false;
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    final linkId = _activeLinkId;
    if (linkId != null) {
      await disconnectLink(linkId);
      return;
    }
    final vehicleId = sessionId;
    if (vehicleId != null) await disconnectLink(vehicleId);
  }

  /// Selects a discovered link for the map and command surface. Waiting links
  /// remain visible in the panel but cannot steal vehicle focus until their
  /// heartbeat identifies a vehicle.
  void selectLink(int linkId) {
    if (_busy || linkId == _activeLinkId) return;
    try {
      final link = fleetSnapshot.link(linkId);
      if (link == null) throw StateError('链路不存在');
      final vehicleId = link.vehicleId;
      if (vehicleId == null) {
        throw StateError('链路尚未识别到飞控');
      }
      fleet.selectVehicle(vehicleId);
      _activeLinkId = linkId;
      _notifyConnectionChanges();
      notifyListeners();
      unawaited(_pollSession());
    } on Object catch (error) {
      message = _friendlyError(error);
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  void _pollTelemetry(int id) {
    if (!isConnected || sessionId != id || _disposed) return;
    if (fleet.snapshot.activeVehicleId != id) return;
    final previousError = telemetryError;
    final previousSatellites = satellitesVisible;
    final previousFix = gpsFixType;
    final previousHeading = headingDegrees;
    final previousBatteryPercent = batteryRemainingPercent;
    final previousBatteryVoltage = batteryVoltage;
    try {
      final envelope = telemetryBackend.sessionStatus(id);
      if (envelope['success'] != true) {
        telemetryError = ffiErrorText(envelope, '读取飞控遥测失败');
        _clearTelemetryValues();
        if (previousError != telemetryError ||
            previousSatellites != satellitesVisible ||
            previousFix != gpsFixType ||
            previousHeading != headingDegrees ||
            previousBatteryPercent != batteryRemainingPercent ||
            previousBatteryVoltage != batteryVoltage) {
          _notifyTelemetryChanges();
          notifyListeners();
        }
        return;
      }

      final data = envelope['data'] as Map<String, Object?>?;
      if (data == null || data['session_id'] != id) {
        telemetryError = '飞控遥测会话不匹配';
        _clearTelemetryValues();
        _notifyTelemetryChanges();
        notifyListeners();
        return;
      }

      telemetryError = null;
      final satelliteValue = data['gps_satellites'] as int?;
      satellitesVisible = satelliteValue != null && satelliteValue >= 0
          ? satelliteValue
          : null;
      final fixValue = data['gps_fix_type'] as int?;
      gpsFixType = fixValue != null && fixValue >= 0 ? fixValue : null;
      headingDegrees = (data['heading_deg'] as num?)?.toDouble();
      batteryRemainingPercent = data['battery_remaining_pct'] as int?;
      batteryVoltage = (data['battery_voltage_v'] as num?)?.toDouble();
      var changed =
          previousError != telemetryError ||
          previousSatellites != satellitesVisible ||
          previousFix != gpsFixType ||
          previousHeading != headingDegrees ||
          previousBatteryPercent != batteryRemainingPercent ||
          previousBatteryVoltage != batteryVoltage;

      final now = DateTime.now().millisecondsSinceEpoch;
      if (now - _lastSystemStatusPollMs >= 2000) {
        _lastSystemStatusPollMs = now;
        final profileChanged = _pollSystemStatus();
        if (profileChanged) _notifyProfileChanges();
        changed = profileChanged || changed;
      }
      changed = _pollServoOutputs() || changed;
      changed = _pollRcChannels() || changed;
      changed = _pollVehicleFix() || changed;
      changed = _pollVfrHud() || changed;
      changed = _pollAttitude() || changed;
      changed = _pollFlightStats() || changed;
      if (changed) {
        _notifyTelemetryChanges();
        notifyListeners();
      }
    } on Object catch (error) {
      telemetryError = '读取飞控遥测失败：$error';
      _clearTelemetryValues();
      _notifyTelemetryChanges();
      notifyListeners();
    }
  }

  /// 波形只在页面可见时轮询，不可见就完全停掉，省掉无谓的取数和重绘。
  void setWaveformActive(bool active) {
    if (_waveformActive == active) return;
    _waveformActive = active;
    _restartWaveformTimer();
    _applyWaveformStreamRate();
  }

  /// 选择波形页面显示的实例。取数定时器共用，所以换实例只是换取样区间，
  /// 立刻补一次取数，不用等下一个周期。
  void setWaveformInstance(WaveformInstanceKind kind, int index) {
    if (index < 0) return;
    if (kind == WaveformInstanceKind.mag) {
      if (index == waveformMagInstance) return;
      waveformMagInstance = index;
      waveformMagPointCount = 0;
    } else {
      if (index == waveformImuInstance) return;
      waveformImuInstance = index;
      waveformImuPointCount = 0;
    }
    final id = sessionId;
    if (_waveformActive && id != null && isConnected) {
      _pollWaveform(id);
    } else {
      _notifyWaveformChanges();
    }
  }

  /// 某一类实例的清单，按实例号升序。
  List<WaveformInstance> waveformInstances(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag
      ? waveformMagInstances
      : waveformImuInstances;

  /// 当前选中的实例号。
  int waveformInstanceIndex(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag
      ? waveformMagInstance
      : waveformImuInstance;

  /// 某一类实例最近一次取数写入的点，画图时直接读这里。
  Float32List waveformPoints(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag ? waveformMagPoints : waveformImuPoints;

  List<double> waveformMagLatest(int index) =>
      waveformMagLatestByInstance[index] ??
      const [double.nan, double.nan, double.nan];

  /// 某一类实例最近一次取数的点数。
  int waveformPointCount(WaveformInstanceKind kind) =>
      kind == WaveformInstanceKind.mag
      ? waveformMagPointCount
      : waveformImuPointCount;

  /// 波形页面可见时把 SCALED_IMU/2/3 都提到 10Hz，离开时恢复常规速率。
  /// 复用通用的 mg_stream_set_interval，不新增后端专用接口。
  void _applyWaveformStreamRate() {
    if (initializationError != null || !isConnected) return;
    final intervalUs = _waveformActive
        ? _waveformStreamIntervalUs
        : _waveformIdleStreamIntervalUs;
    for (final messageId in _waveformMessageIds) {
      try {
        _native!.setStreamInterval(messageId, intervalUs);
      } on Object {
        // 速率只是画质优化，失败不影响波形页面本身。
      }
    }
  }

  /// 后端识别到固件时会重新下发常规流速率，识别完成后再补一次高速率。
  void _scheduleWaveformStreamRateRetry() {
    Timer(const Duration(milliseconds: 1500), () {
      if (!_disposed && _waveformActive) _applyWaveformStreamRate();
    });
  }

  void _restartWaveformTimer() {
    _waveformTimer?.cancel();
    _waveformTimer = null;
    if (!_waveformActive || _disposed || initializationError != null) return;
    _waveformTimer = Timer.periodic(_waveformPollInterval, (_) {
      final id = sessionId;
      if (id != null) _pollWaveform(id);
    });
  }

  void _pollWaveform(int id) {
    if (!isConnected || sessionId != id || _disposed) return;
    _readWaveformKind(WaveformInstanceKind.imu);
    _readWaveformKind(WaveformInstanceKind.mag);
    // 磁场强度要同时显示所有罗盘，而整窗取数只覆盖当前选中的实例，
    // 所以给其余实例各补一次"只要最新一点"的取数。
    _refreshMagLatestValues();
    // 实例数量随"最近有没有收到数据"变化，和取数同一个周期刷新，
    // 只有真的变了才通知界面，避免选择器每秒闪一下。
    _refreshWaveformInstances();
    _notifyWaveformChanges();
  }

  /// 刷新所有罗盘实例的最新磁场值，而不只是当前选中的那个。
  ///
  /// 磁场强度卡片是按实例平铺的，只更新选中实例会导致别的罗盘一直空着，
  /// 直到用户手动切过去。
  ///
  /// 这里用**时间窗口**而不是点数来裁剪：`copy_points` 在"可用点数多于
  /// max_points"时会走 min/max 降采样分支，而那个分支要求 `written + 2 <=
  /// max_points`，所以传 `target_points = 1` 会一个点都不返回。改成把窗口压到
  /// 1ms，缓冲里的点数是 1，走的是直通分支，拿到的就是真正的最新一帧。
  void _refreshMagLatestValues() {
    for (final instance in waveformMagInstances) {
      if (instance.index == waveformMagInstance) continue; // 整窗取数已经顺手更新过
      try {
        final written = _native!.readWaveform(
          maxPoints: _waveformMaxPoints,
          floatsPerPoint: _waveformFloatsPerPoint,
          windowMs: 1, // 只保留最新一帧
          instance: instance.index,
        );
        if (written <= 0) continue;
        final base = (written - 1) * _waveformFloatsPerPoint + 7;
        waveformMagLatestByInstance[instance.index] = [
          _native!.waveformValueAt(base),
          _native!.waveformValueAt(base + 1),
          _native!.waveformValueAt(base + 2),
        ];
      } on Object {
        // 单个实例取不到数据不影响其它实例。
      }
    }
  }

  /// 取一类实例的波形。任何一个实例取不到数据都只是把点数清零，
  /// 不影响另一类实例的显示。
  void _readWaveformKind(WaveformInstanceKind kind) {
    final instance = waveformInstanceIndex(kind);
    var count = 0;
    try {
      final written = _native!.readWaveform(
        maxPoints: _waveformMaxPoints,
        floatsPerPoint: _waveformFloatsPerPoint,
        windowMs: waveformWindowMs,
        instance: instance,
      );
      if (written > 0) {
        final view = _native!.waveformView(written * _waveformFloatsPerPoint);
        waveformPoints(kind).setRange(0, view.length, view);
        count = written;
        if (kind == WaveformInstanceKind.mag) {
          final base = (written - 1) * _waveformFloatsPerPoint + 7;
          waveformMagLatestByInstance[instance] = [
            _native!.waveformValueAt(base),
            _native!.waveformValueAt(base + 1),
            _native!.waveformValueAt(base + 2),
          ];
        }
      }
    } on Object {
      count = 0;
    }

    if (kind == WaveformInstanceKind.mag) {
      waveformMagPointCount = count;
    } else {
      waveformImuPointCount = count;
    }
  }

  /// 刷新后端识别到的实例清单，并把选中实例收敛到仍然存在的实例上。
  void _refreshWaveformInstances() {
    try {
      final payload = _native!.waveformInstances() as Map<String, Object?>;
      final imu = _parseWaveformInstances(payload['imu']);
      final mag = _parseWaveformInstances(payload['mag']);
      if (listEquals(imu, waveformImuInstances) &&
          listEquals(mag, waveformMagInstances)) {
        return;
      }
      waveformImuInstances = imu;
      waveformMagInstances = mag;
      waveformImuInstance = _clampInstance(waveformImuInstance, imu);
      waveformMagInstance = _clampInstance(waveformMagInstance, mag);
    } on Object {
      // 实例清单只是选择器数据，取不到就保持上一次的结果。
    }
  }

  static List<WaveformInstance> _parseWaveformInstances(Object? raw) {
    final entries = raw as List<Object?>? ?? const [];
    final instances = <WaveformInstance>[];
    for (final entry in entries) {
      if (entry is! Map<String, Object?>) continue;
      final index = entry['index'] as int?;
      if (index == null || index < 0) continue;
      instances.add(
        WaveformInstance(
          index: index,
          active: entry['active'] == true,
          name: entry['name'] as String? ?? '',
        ),
      );
    }
    if (instances.isEmpty) {
      return const [_waveformDefaultInstance];
    }
    return instances;
  }

  /// 选中的实例消失时退回列表里的第一个，保证选择器始终指向有效实例。
  static int _clampInstance(int current, List<WaveformInstance> instances) =>
      instances.any((instance) => instance.index == current)
      ? current
      : instances.first.index;

  void _pollParameterDownload() {
    if (!_parameterRequestStarted || parametersLoaded || sessionId == null) {
      return;
    }
    try {
      final previousReceived = parameterReceived;
      final previousTotal = parameterTotal;
      final previousLoading = parametersLoading;
      final progress = parameterBackend.progress();
      parameterReceived = progress['received'] as int? ?? 0;
      parameterTotal = progress['total'] as int? ?? 0;
      parametersLoading =
          parameterTotal == 0 || parameterReceived < parameterTotal;
      if (parameterTotal <= 0 || parameterReceived < parameterTotal) {
        if (previousReceived != parameterReceived ||
            previousTotal != parameterTotal ||
            previousLoading != parametersLoading) {
          _notifyParameterChanges();
        }
        return;
      }

      final rawParameters = parameterBackend.all();
      if (rawParameters is! List<Object?>) {
        throw const FormatException('飞控参数列表格式无效');
      }
      parameters
        ..clear()
        ..addEntries(
          rawParameters
              .whereType<Map<String, Object?>>()
              .map((json) {
                final parameter = VehicleParameter.fromTelemetry(json);
                return MapEntry(parameter.name, parameter);
              })
              .where((entry) => entry.key.isNotEmpty),
        );
      parametersLoaded = true;
      parametersLoading = false;
      parameterError = null;
      _parameterMetadataLoaded.clear();
      _parameterMetadataAttempts.clear();
      loadParameterMetadata(_pendingParameterMetadata.toList());
      _notifyProfileChanges();
      _notifyParameterChanges();
    } on Object catch (error) {
      parameterError = '读取飞控参数失败：${error.toString()}';
      parametersLoading = false;
      _notifyParameterChanges();
    }
  }

  bool _pollServoOutputs() {
    final oldValid = servoOutputsValid;
    final oldValues = servoOutputs;
    try {
      final result = telemetryBackend.servoOutputs();
      servoOutputsValid = result['valid'] == true;
      final values = result['servos'] as List<Object?>? ?? const [];
      servoOutputs = values
          .map((value) => value is num ? value.toInt() : 0)
          .toList(growable: false);
    } on Object {
      servoOutputsValid = false;
      servoOutputs = const [];
    }
    return oldValid != servoOutputsValid ||
        !_sameIntList(oldValues, servoOutputs);
  }

  bool _pollRcChannels() {
    final oldValid = rcChannelsValid;
    final oldValues = rcChannels;
    try {
      final result = telemetryBackend.rcChannels();
      rcChannelsValid = result['valid'] == true;
      final values = result['channels'] as List<Object?>? ?? const [];
      rcChannels = values
          .map((value) => value is num ? value.toInt() : 0)
          .toList(growable: false);
    } on Object {
      rcChannelsValid = false;
      rcChannels = const [];
    }
    return oldValid != rcChannelsValid || !_sameIntList(oldValues, rcChannels);
  }

  /// 读取当前位置与 Home 点。
  ///
  /// 后端已经缓存了 GLOBAL_POSITION_INT / HOME_POSITION，这里只做取数和比较，
  /// 位置变化很小也照常通知，地图标记才跟得上。
  bool _pollVehicleFix() {
    final oldPosition = vehiclePosition == null
        ? ''
        : jsonEncode(vehiclePosition!.toJson());
    final oldPositionValid = vehiclePositionValid;
    final oldHome = homePosition == null
        ? ''
        : jsonEncode(homePosition!.toJson());
    final oldHomeValid = homePositionValid;
    final oldRelativeAltitude = relativeAltitudeMeters;
    final oldAbsoluteAltitude = absoluteAltitudeMeters;
    try {
      final envelope = telemetryBackend.globalPosition();
      final data = envelope['data'] as Map<String, Object?>?;
      vehiclePositionValid =
          envelope['success'] == true && data?['valid'] == true;
      vehiclePosition = data == null
          ? null
          : VehicleFix(
              latitude: (data['lat_deg'] as num?)?.toDouble() ?? 0,
              longitude: (data['lon_deg'] as num?)?.toDouble() ?? 0,
              altitudeMeters: (data['alt_msl_m'] as num?)?.toDouble(),
              headingDegrees: (data['heading_deg'] as num?)?.toDouble(),
            );
      relativeAltitudeMeters = (data?['alt_rel_m'] as num?)?.toDouble();
      absoluteAltitudeMeters = (data?['alt_msl_m'] as num?)?.toDouble();
    } on Object {
      vehiclePosition = null;
      vehiclePositionValid = false;
      relativeAltitudeMeters = null;
      absoluteAltitudeMeters = null;
    }
    try {
      final envelope = telemetryBackend.homePosition();
      final data = envelope['data'] as Map<String, Object?>?;
      homePositionValid = envelope['success'] == true && data?['valid'] == true;
      homePosition = data == null
          ? null
          : VehicleFix(
              latitude: (data['lat_deg'] as num?)?.toDouble() ?? 0,
              longitude: (data['lon_deg'] as num?)?.toDouble() ?? 0,
              altitudeMeters: (data['alt_m'] as num?)?.toDouble(),
            );
    } on Object {
      homePosition = null;
      homePositionValid = false;
    }
    return oldPosition !=
            (vehiclePosition == null
                ? ''
                : jsonEncode(vehiclePosition!.toJson())) ||
        oldPositionValid != vehiclePositionValid ||
        oldHome !=
            (homePosition == null ? '' : jsonEncode(homePosition!.toJson())) ||
        oldHomeValid != homePositionValid ||
        oldRelativeAltitude != relativeAltitudeMeters ||
        oldAbsoluteAltitude != absoluteAltitudeMeters;
  }

  /// ATTITUDE → 姿态仪的横滚/俯仰（度）。
  bool _pollAttitude() {
    final oldRoll = rollDegrees;
    final oldPitch = pitchDegrees;
    try {
      final envelope = telemetryBackend.attitude();
      final data = envelope['data'] as Map<String, Object?>? ?? const {};
      rollDegrees = (data['roll_deg'] as num?)?.toDouble();
      pitchDegrees = (data['pitch_deg'] as num?)?.toDouble();
    } on Object {
      // Preserve the previous sample; freshness handles stale data.
    }
    return oldRoll != rollDegrees || oldPitch != pitchDegrees;
  }

  /// VFR_HUD 是空速 / 地速的直接来源。它和位置 / 姿态一样走 20Hz 前端轮询，
  /// 不通过 telemetryChanges 以外的路径更新。
  bool _pollVfrHud() {
    final oldAirspeed = airspeedMetersPerSecond;
    final oldGroundSpeed = groundSpeedMetersPerSecond;
    final oldClimbRate = climbRateMetersPerSecond;
    final oldThrottle = throttlePercent;
    try {
      final envelope = telemetryBackend.vfrHud();
      final data = envelope['data'] as Map<String, Object?>? ?? const {};
      airspeedMetersPerSecond = (data['airspeed_m_s'] as num?)?.toDouble();
      groundSpeedMetersPerSecond = (data['groundspeed_m_s'] as num?)
          ?.toDouble();
      climbRateMetersPerSecond = (data['climb_m_s'] as num?)?.toDouble();
      throttlePercent = (data['throttle_pct'] as num?)?.toDouble();
    } on Object {
      // Preserve the previous sample; freshness handles stale data.
    }
    return oldAirspeed != airspeedMetersPerSecond ||
        oldGroundSpeed != groundSpeedMetersPerSecond ||
        oldClimbRate != climbRateMetersPerSecond ||
        oldThrottle != throttlePercent;
  }

  /// QGC `Vehicle::_flightTimerStart()` / `updateFlightDistance()` 的等价实现。
  ///
  /// 解锁瞬间清零并起表，上锁后停在当前值（QGC 也是停在最后一次统计值，
  /// 重新解锁才清零）。位置没锁定时只走时间，不累距离。
  bool _pollFlightStats() {
    final oldTime = flightTimeSeconds;
    final oldDistance = flightDistanceMeters;
    if (!vehicleArmed) {
      if (_flightStatsArmed) {
        _flightStatsArmed = false;
        _flightStopwatch.stop();
      }
      _flightStopwatchOffsetSeconds = 0;
      flightTimeSeconds = 0;
      flightDistanceMeters = 0;
      _lastFlightLatitude = null;
      _lastFlightLongitude = null;
      return oldTime != flightTimeSeconds ||
          oldDistance != flightDistanceMeters;
    }
    if (!_flightStatsArmed) {
      _flightStatsArmed = true;
      _flightStopwatchOffsetSeconds = 0;
      _flightStopwatch
        ..reset()
        ..start();
      flightTimeSeconds = 0;
      flightDistanceMeters = 0;
      _lastFlightLatitude = null;
      _lastFlightLongitude = null;
      return true;
    }
    flightTimeSeconds =
        _flightStopwatchOffsetSeconds +
        _flightStopwatch.elapsedMilliseconds / 1000.0;
    final fix = vehiclePosition;
    if (vehiclePositionValid && fix != null) {
      final previousLatitude = _lastFlightLatitude;
      final previousLongitude = _lastFlightLongitude;
      if (previousLatitude != null && previousLongitude != null) {
        flightDistanceMeters += wgs84DistanceMeters(
          LatLng(previousLatitude, previousLongitude),
          LatLng(fix.latitude, fix.longitude),
        );
      }
      _lastFlightLatitude = fix.latitude;
      _lastFlightLongitude = fix.longitude;
    }
    return oldTime != flightTimeSeconds || oldDistance != flightDistanceMeters;
  }

  bool _pollSystemStatus() {
    final oldStateFresh = vehicleStateFresh;
    final oldFirmware = jsonEncode(firmwareInfo);
    final oldGuidedTargetValid = guidedTargetValid;
    final oldIdentity = jsonEncode(vehicleIdentity);
    final oldVersion = jsonEncode(firmwareVersionInfo);
    final oldSensors = jsonEncode(sensorHealth);
    final oldArmed = vehicleArmed;
    final oldFlightMode = vehicleFlightMode;
    final oldVehicleType = vehicleType;
    final oldAutopilotType = vehicleAutopilotType;
    final oldLandedState = vehicleLandedState;
    try {
      // 引导操作门控要用到的实时状态（QGC 的 show* 属性）。
      final summaryEnvelope = telemetryBackend.vehicleSummary();
      if (summaryEnvelope['success'] == true) {
        final summaryData =
            summaryEnvelope['data'] as Map<String, Object?>? ?? const {};
        vehicleArmed = summaryData['armed'] == true;
        vehicleFlightMode = summaryData['flight_mode'] as String? ?? '';
        vehicleType = summaryData['vehicle_type'] as int? ?? 0;
        vehicleAutopilotType = summaryData['autopilot_type'] as int? ?? 0;
        _updateGuidedTargetMode();
      }

      final extendedEnvelope = telemetryBackend.extendedSysState();
      if (extendedEnvelope['success'] == true) {
        final extendedData =
            extendedEnvelope['data'] as Map<String, Object?>? ?? const {};
        vehicleLandedState = extendedData['landed_state'] as int? ?? 0;
        vehicleVtolState = extendedData['vtol_state'] as int? ?? 0;
      }

      final firmwareEnvelope = telemetryBackend.firmwareType();
      firmwareInfo = firmwareEnvelope['success'] == true
          ? firmwareEnvelope['data'] as Map<String, Object?>?
          : null;

      vehicleIdentity = telemetryBackend.vehicleFirmware();

      final versionEnvelope = telemetryBackend.firmwareVersion();
      firmwareVersionInfo = versionEnvelope['success'] == true
          ? versionEnvelope['data'] as Map<String, Object?>?
          : null;

      final freshness = telemetryBackend.telemetryFreshness();
      final groups = freshness['groups'] as Map<String, Object?>?;
      final heartbeatFresh = groups?['heartbeat_fresh'] == true;
      // 收到过遥测后若不再新鲜，界面必须标记过期而不是继续显示冻结值。
      _telemetrySnapshotValid = _telemetrySnapshotValid || heartbeatFresh;
      _telemetryGroupsStale = _telemetrySnapshotValid && !heartbeatFresh;
      if (groups?['sys_status_fresh'] == true) {
        sensorHealth = telemetryBackend.sensorHealth();
      } else {
        sensorHealth = null;
      }
    } on Object {
      // Keep the last valid snapshot. A single failed poll must not make a
      // connected vehicle look disarmed or change its profile transiently.
    }
    return oldStateFresh != vehicleStateFresh ||
        oldFirmware != jsonEncode(firmwareInfo) ||
        oldIdentity != jsonEncode(vehicleIdentity) ||
        oldVersion != jsonEncode(firmwareVersionInfo) ||
        oldSensors != jsonEncode(sensorHealth) ||
        oldArmed != vehicleArmed ||
        oldFlightMode != vehicleFlightMode ||
        oldVehicleType != vehicleType ||
        oldAutopilotType != vehicleAutopilotType ||
        oldLandedState != vehicleLandedState ||
        oldGuidedTargetValid != guidedTargetValid;
  }

  /// 让 Go To 显示目标跟随飞控飞行模式生命周期。
  void _updateGuidedTargetMode() {
    if (!guidedTargetValid && guidedTargetPosition == null) {
      _guidedTargetModeObserved = false;
      _guidedTargetModeDeadline = null;
      return;
    }
    if (isGuidedGotoFlightMode) {
      _guidedTargetModeObserved = true;
      _guidedTargetModeDeadline = null;
      return;
    }

    final deadline = _guidedTargetModeDeadline;
    final timedOut = deadline != null && !DateTime.now().isBefore(deadline);
    if (_guidedTargetModeObserved || timedOut) {
      _clearGuidedTarget(notify: false);
    }
  }

  /// 永久清除当前 Go To 显示目标。
  void _clearGuidedTarget({bool notify = true}) {
    final changed =
        guidedTargetValid ||
        guidedTargetPosition != null ||
        _guidedTargetModeObserved ||
        _guidedTargetModeDeadline != null;
    guidedTargetPosition = null;
    guidedTargetValid = false;
    _guidedTargetModeObserved = false;
    _guidedTargetModeDeadline = null;
    if (changed && notify) {
      _notifyTelemetryChanges();
      notifyListeners();
    }
  }

  bool _sameIntList(List<int> left, List<int> right) {
    if (left.length != right.length) return false;
    for (var index = 0; index < left.length; index++) {
      if (left[index] != right[index]) return false;
    }
    return true;
  }

  void _clearTelemetry() {
    _telemetrySnapshotValid = false;
    _telemetryGroupsStale = false;
    _resetRuntimeState();
    _notifyParameterChanges();
    _notifyTelemetryChanges();
    _notifyProfileChanges();
  }

  /// Reset only the per-vehicle state. Session and mission binding are handled
  /// by the caller so a target switch can restore its own snapshot afterward.
  void _resetRuntimeState() {
    telemetryError = null;
    flightMessages = const [];
    _lastMessageSeq = 0;
    _clearTelemetryValues();
    vehicleFlightMode = '';
    vehicleArmed = false;
    vehicleType = 0;
    vehicleAutopilotType = 0;
    flightModesAvailable = false;
    flightModeOptions = const [];
    parameters.clear();
    _parameterMetadataLoaded.clear();
    _parameterMetadataAttempts.clear();
    _pendingParameterMetadata.clear();
    parameterReceived = 0;
    parameterTotal = 0;
    parametersLoaded = false;
    parametersLoading = false;
    parameterError = null;
    _parameterWriteErrors.clear();
    _parameterRequestStarted = false;
    _calibrationTimer?.cancel();
    calibrationStatus = const CalibrationStatus();
    calibrationError = null;
    servoOutputs = const [];
    servoOutputsValid = false;
    rcChannels = const [];
    rcChannelsValid = false;
    vehiclePosition = null;
    vehiclePositionValid = false;
    homePosition = null;
    homePositionValid = false;
    vehicleLandedState = 0;
    vehicleVtolState = 0;
    _clearGuidedTarget(notify: false);
    serialHardwareMappings.clear();
    serialHardwareMappingLoading = false;
    serialHardwareMappingError = null;
    _serialHardwareMappingRequested = false;
    _firmwareVersionRequested = false;
    _loadingParameterMetadata = false;
  }

  void _clearTelemetryValues() {
    satellitesVisible = null;
    gpsFixType = null;
    headingDegrees = null;
    airspeedMetersPerSecond = null;
    groundSpeedMetersPerSecond = null;
    climbRateMetersPerSecond = null;
    throttlePercent = null;
    rollDegrees = null;
    pitchDegrees = null;
    relativeAltitudeMeters = null;
    absoluteAltitudeMeters = null;
    _flightStatsArmed = false;
    _flightStopwatch.stop();
    _flightStopwatchOffsetSeconds = 0;
    flightTimeSeconds = 0;
    flightDistanceMeters = 0;
    _lastFlightLatitude = null;
    _lastFlightLongitude = null;
    batteryRemainingPercent = null;
    batteryVoltage = null;
    firmwareInfo = null;
    vehicleIdentity = null;
    firmwareVersionInfo = null;
    sensorHealth = null;
    waveformImuPointCount = 0;
    waveformMagPointCount = 0;
    waveformMagLatestByInstance.clear();
    waveformImuInstances = const [_waveformDefaultInstance];
    waveformMagInstances = const [_waveformDefaultInstance];
    waveformImuInstance = 0;
    waveformMagInstance = 0;
    servoOutputs = const [];
    servoOutputsValid = false;
    rcChannels = const [];
    rcChannelsValid = false;
    vehiclePosition = null;
    vehiclePositionValid = false;
    homePosition = null;
    homePositionValid = false;
  }

  @override
  void dispose() {
    _disposed = true;
    _flightCommandBackend?.dispose();
    _logController?.dispose();
    _tlogController?.dispose();
    _fileController?.dispose();
    _payloadController?.dispose();
    _fleet?.removeListener(_handleFleetChanged);
    _fleetTimer?.cancel();
    _fleet?.dispose();
    _flightStopwatch.stop();
    _stateTimer?.cancel();
    _telemetryTimer?.cancel();
    _waveformTimer?.cancel();
    _calibrationTimer?.cancel();
    _signing.removeListener(notifyListeners);
    _signing.dispose();
    _native?.dispose();
    parameterChanges.dispose();
    telemetryChanges.dispose();
    profileChanges.dispose();
    connectionChanges.dispose();
    waveformChanges.dispose();
    calibrationChanges.dispose();
    _offlineMissionPlanning?.dispose();
    for (final mission in _missionPlanningBySession.values) {
      mission.dispose();
    }
    _missionPlanningBySession.clear();
    super.dispose();
  }
}
