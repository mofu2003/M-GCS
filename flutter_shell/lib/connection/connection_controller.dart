import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';
import 'package:latlong2/latlong.dart';

import '../mission/mission_document_controller.dart';
import '../mission/mission_geo.dart';
import '../mission/mission_document_ffi_backend.dart';
import '../mission/mission_document_models.dart';
import 'telemetry_ffi_backend.dart';
import 'parameter_ffi_backend.dart';
import 'calibration_ffi_backend.dart';
import 'flight_command_ffi_backend.dart';

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

final class ConnectionController extends ChangeNotifier {
  ConnectionController() {
    _initialize();
  }

  late final DynamicLibrary _library;
  late final Pointer<Void> _core;
  late final Pointer<Void> Function() _create;
  late final void Function(Pointer<Void>) _destroy;
  late final Pointer<Utf8> Function(Pointer<Void>) _listPorts;
  late final int Function(Pointer<Void>, Pointer<Utf8>) _open;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getSessionState;
  late final int Function(Pointer<Void>, Pointer<Float>, int, int, int, int)
  _readWaveform;
  late final Pointer<Utf8> Function(Pointer<Void>) _getWaveformInstances;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _setStreamInterval;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestFirmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _getSerialUartNames;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _selectVehicle;
  late final int Function(Pointer<Void>) _getActiveVehicle;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _cancelOperation;
  late final Pointer<Utf8> Function(Pointer<Void>) _getFlightModes;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getMessages;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _setFlightMode;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _close;
  late final void Function(Pointer<Utf8>) _freeString;

  MissionDocumentController? _missionPlanning;
  MissionDocumentFfiBackend? _missionBackend;
  TelemetryFfiBackend? _telemetryBackend;
  ParameterFfiBackend? _parameterBackend;
  CalibrationFfiBackend? _calibrationBackend;
  FlightCommandFfiBackend? _flightCommandBackend;

  MissionDocumentController get missionPlanning {
    final controller = _missionPlanning ??= MissionDocumentController(
      backend: _missionBackend ??= MissionDocumentFfiBackend(
        _library,
        _core,
        isConnected: () => isConnected,
      ),
    );
    controller.bindSession(sessionId);
    return controller;
  }

  TelemetryFfiBackend get telemetryBackend =>
      _telemetryBackend ??= TelemetryFfiBackend(_library, _core);

  ParameterFfiBackend get parameterBackend =>
      _parameterBackend ??= ParameterFfiBackend(_library, _core);

  CalibrationFfiBackend get calibrationBackend =>
      _calibrationBackend ??= CalibrationFfiBackend(_library, _core);

  FlightCommandFfiBackend get flightCommandBackend =>
      _flightCommandBackend ??= FlightCommandFfiBackend(_library, _core);

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
  /// 与 `docs/CONSTRAINTS.md` 性能约束"波形/曲线 10Hz"一致。
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

  /// QGC `Vehicle::_flightTimerStart()` 的等价统计。
  ///
  /// 飞控不直接提供 FlightTime / FlightDistance，QGC 也是地面站本地累计：
  /// 解锁时清零并开始计时，上锁后停止；每次定位刷新把位移累进总里程。
  double flightTimeSeconds = 0;
  double flightDistanceMeters = 0;
  final Stopwatch _flightStopwatch = Stopwatch();
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
  late final Pointer<Float> _waveformBuffer;
  int _lastSystemStatusPollMs = 0;
  bool _busy = false;
  bool _disposed = false;
  bool _firmwareVersionRequested = false;
  bool _parameterRequestStarted = false;
  bool _loadingParameterMetadata = false;
  VehicleControlProfile? _lastPublishedControlProfile;

  bool get busy => _busy;
  bool get isConnected => phase == SerialConnectionPhase.connected;
  bool get hasSession => sessionId != null;
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

  /// QGC `APMFirmwarePlugin::_guidedModeTakeoff` only enables takeoff for
  /// multirotor and VTOL vehicles. Fixed-wing aircraft need a runway takeoff
  /// and are handled by the mission/AUTO flow instead.
  bool get supportsGuidedTakeoff => const {
    2, 3, 4, 13, 14, 15, // MAV_TYPE multirotor family
    19, 20, 21, 22, 23, 24, // MAV_TYPE VTOL family
  }.contains(vehicleType);

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
  bool get supportsChangeHeading => const {
    2, 3, 4, 13, 14, 15, // MAV_TYPE multirotor family
    12, // MAV_TYPE_SUBMARINE
  }.contains(vehicleType);

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
        throw StateError(_nativeError(result, '启动加速度计校准失败'));
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
        throw StateError(_nativeError(result, '启动校准失败'));
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
        throw StateError(_nativeError(result, '确认姿态失败'));
      }
      await _pollCalibrationStatus();
    } on Object catch (error) {
      calibrationError = _friendlyError(error);
      _notifyCalibrationChanges();
      notifyListeners();
    }
  }

  Future<void> cancelCalibration() async {
    if (!calibrationStatus.active) return;
    final requestedType = calibrationStatus.type;
    _calibrationTimer?.cancel();
    calibrationStatus = CalibrationStatus(
      active: false,
      type: requestedType,
      stage: 'cancelled',
      progress: calibrationStatus.progress,
      message: '已取消校准',
      compassProgress: calibrationStatus.compassProgress,
    );
    calibrationError = null;
    _notifyCalibrationChanges();
    notifyListeners();

    Object? cancellationError;
    try {
      final result = calibrationBackend.cancel();
      if (result['success'] != true) {
        final message = _nativeError(result, '取消校准失败');
        // A stale UI snapshot can outlive the native calibration state. In
        // that case the local clear above is already the correct end state.
        if (!message.contains('no calibration in progress')) {
          throw StateError(message);
        }
      }
    } on Object catch (error) {
      cancellationError = error;
    }
    // Always refresh after a cancellation attempt. The native service stops
    // its local state even when the MAVLink cancel frame could not be sent;
    // leaving the UI stuck in "calibrating" is worse than showing the send
    // error after the state has already returned to idle/cancelled.
    await _pollCalibrationStatus();
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
    if (_disposed || sessionId == null) return;
    try {
      final raw = calibrationBackend.status();
      final next = CalibrationStatus(
        active: raw['active'] == true,
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

  String _nativeError(Object? result, String fallback) {
    if (result is Map<String, Object?>) {
      final error = result['error'];
      if (error is String && error.isNotEmpty) return error;
      if (error is Map<String, Object?>) {
        final message = error['message'];
        if (message is String && message.isNotEmpty) return message;
      }
    }
    return fallback;
  }

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
    if (_busy || hasSession || initializationError != null) return false;
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

  String get selectedTransportLabel => switch (transport) {
    ConnectionTransport.serial => '串口',
    ConnectionTransport.udp => 'UDP',
    ConnectionTransport.tcp => 'TCP',
  };

  void selectTransport(ConnectionTransport value) {
    if (hasSession || _busy || transport == value) return;
    transport = value;
    message = null;
    _notifyConnectionChanges();
    notifyListeners();
    if (value == ConnectionTransport.serial && ports.isEmpty) {
      unawaited(refreshPorts());
    }
  }

  void updateNetworkEndpoint({required String host, required String port}) {
    if (hasSession) return;
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
        final error = result['error'];
        throw StateError(
          error is Map<String, Object?>
              ? error['message'] as String? ?? '无法请求飞控参数'
              : '无法请求飞控参数',
        );
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
    _serialHardwareMappingRequested = true;
    serialHardwareMappingLoading = true;
    serialHardwareMappingError = null;
    _notifyParameterChanges();
    notifyListeners();
    await Future<void>.delayed(Duration.zero);
    try {
      final result =
          _readJson(_getSerialUartNames(_core)) as Map<String, Object?>;
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
        serialHardwareMappingError = result['error'] as String?;
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
    _loadingParameterMetadata = true;
    try {
      while (_pendingParameterMetadata.isNotEmpty && !_disposed) {
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
      _loadingParameterMetadata = false;
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
        final result = parameterBackend.write(name, value);
        if (result['success'] != true) {
          final error = result['error'];
          throw StateError(
            error is Map<String, Object?>
                ? error['message'] as String? ?? '参数写入失败'
                : error as String? ?? '参数写入失败',
          );
        }

        final acknowledgementTimer = Stopwatch()..start();
        while (acknowledgementTimer.elapsed < const Duration(seconds: 1)) {
          await Future<void>.delayed(const Duration(milliseconds: 100));
          _ensureWriteSession(requestedSessionId, name);
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
      _library = _openPlatformLibrary();
      _create = _library
          .lookup<NativeFunction<Pointer<Void> Function()>>('mg_create')
          .asFunction();
      _destroy = _library
          .lookup<NativeFunction<Void Function(Pointer<Void>)>>('mg_destroy')
          .asFunction();
      _freeString = _library
          .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>(
            'mg_free_string',
          )
          .asFunction();
      _listPorts = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_connection_list_ports',
          )
          .asFunction();
      _open = _library
          .lookup<NativeFunction<Int32 Function(Pointer<Void>, Pointer<Utf8>)>>(
            'mg_connection_open',
          )
          .asFunction();
      _getSessionState = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            'mg_connection_get_session_state',
          )
          .asFunction();
      _readWaveform = _library
          .lookup<
            NativeFunction<
              Int32 Function(
                Pointer<Void>,
                Pointer<Float>,
                Int32,
                Int32,
                Int32,
                Int32,
              )
            >
          >('mg_waveform_poll')
          .asFunction();
      _getWaveformInstances = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_waveform_get_instances',
          )
          .asFunction();
      _setStreamInterval = _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32, Int32)>
          >('mg_stream_set_interval')
          .asFunction();
      _requestFirmwareVersion = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_vehicle_request_firmware_version',
          )
          .asFunction();
      _close = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            'mg_connection_close',
          )
          .asFunction();
      _getSerialUartNames = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_serial_get_uart_names',
          )
          .asFunction();
      _selectVehicle = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            'mg_vehicle_select',
          )
          .asFunction();
      _getActiveVehicle = _library
          .lookup<NativeFunction<Int32 Function(Pointer<Void>)>>(
            'mg_vehicle_get_active',
          )
          .asFunction();
      _cancelOperation = _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
          >('mg_operations_cancel')
          .asFunction();
      _getFlightModes = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_flight_get_modes',
          )
          .asFunction();
      _getMessages = _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Uint64)>
          >('mg_telemetry_get_messages')
          .asFunction();
      _setFlightMode = _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
          >('mg_flight_set_mode')
          .asFunction();
      _core = _create();
      _waveformBuffer = calloc<Float>(
        _waveformMaxPoints * _waveformFloatsPerPoint,
      );
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

  // ================================================================
  // 引导操作与任务读取（对标 QGC GuidedActionsController / PlanView）
  // ================================================================

  void _requireConnected(String action) {
    if (!isConnected) throw StateError('未连接飞控，无法$action');
  }

  /// 增量拉取飞控消息（只取 seq 比上次大的），追加到本地列表。
  ///
  /// 后端保留最近 200 条，这里同样限长，避免长时间连接后无限增长。
  Future<void> refreshFlightMessages() async {
    if (!isConnected) return;
    try {
      final response =
          _readJson(_getMessages(_core, _lastMessageSeq))
              as Map<String, Object?>;
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
    try {
      final response =
          _readJson(_getFlightModes(_core)) as Map<String, Object?>;
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
      if (current.isNotEmpty) vehicleFlightMode = current;
      notifyListeners();
    } on Object {
      // 轮询失败保持上一次结果，不打断其它遥测。
    }
  }

  /// 按名字切换飞行模式（MP 的 setMode(name) / QGC 的 flightMode 属性写法）。
  Future<void> setFlightMode(String name) async {
    _requireConnected('切换飞行模式');
    final nativeName = name.toNativeUtf8();
    try {
      _requireCommandSuccess(
        _readJson(_setFlightMode(_core, nativeName)),
        '切换到 $name',
      );
    } finally {
      malloc.free(nativeName);
    }
    await refreshFlightModes();
  }

  void _requireCommandSuccess(Object? raw, String action) {
    if (raw is! JsonMap) throw const FormatException('指令响应格式无效');
    if (raw['success'] == true) return;
    final reason = raw['error'] ?? raw['result_text'] ?? _commandAckReason(raw);
    throw StateError('$action失败：$reason');
  }

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
    _requireConnected('移动到此处');
    // QGC `APMFirmwarePlugin::guidedModeGotoLocation` 在高度未知时直接返回、不发指令。
    // DO_REPOSITION 走 MAV_FRAME_GLOBAL，高度必须是 AMSL 绝对值；这里如果拿到 0
    // 或非有限值就拒发——否则等于命令飞机飞向地面高度。
    if (!altitudeMeters.isFinite || altitudeMeters == 0) {
      throw StateError('移动到此处失败：没有有效的海拔高度');
    }
    _requireCommandSuccess(
      flightCommandBackend.guidedPoint(
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
    _notifyTelemetryChanges();
    notifyListeners();
  }

  Future<void> guidedReturnToLaunch() async {
    _requireConnected('返航');
    _requireCommandSuccess(flightCommandBackend.rtl(), '返航');
  }

  Future<void> guidedLand() async {
    _requireConnected('降落');
    _requireCommandSuccess(flightCommandBackend.land(), '降落');
  }

  Future<void> guidedTakeoff(double altitudeMeters) async {
    _requireConnected('起飞');
    if (vehicleFlying) {
      throw StateError('起飞失败：飞机已经在空中');
    }
    if (!supportsGuidedTakeoff) {
      throw StateError('起飞失败：当前机型不支持引导起飞');
    }
    // QGC `_guidedModeTakeoff` 在 `altitudeAMSL` 为 NaN 时直接拒绝。
    if (absoluteAltitudeMeters == null) {
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
        flightCommandBackend.takeoff(takeoffAltitude),
        '起飞',
      );
      if (!vehicleArmed) await _armVehicleAndValidate();
      return;
    }

    await _setFlightModeAndValidate(_guidedCustomMode, 'Guided');
    if (!vehicleArmed) await _armVehicleAndValidate();
    _requireCommandSuccess(flightCommandBackend.takeoff(takeoffAltitude), '起飞');
  }

  /// 按 QGC 的 APM/PX4 Start Mission 流程解锁并启动当前飞控任务。
  ///
  /// ArduPilot 固定翼需要先进入 AUTO 再解锁，避免 VTOL/固定翼在错误姿态下上锁；
  /// 多旋翼先进入 GUIDED、解锁后再发 MISSION_START。PX4 统一进入 AUTO_MISSION。
  Future<void> startMission() async {
    _requireConnected('启动任务');
    // QGC uses Vehicle::fixedWing() here. ArduPilot VTOL reports the VTOL MAV
    // type, so it is *not* in QGC's fixed-wing class and must go through the
    // GUIDED + MISSION_START path instead of being treated like a runway plane.
    final qgcFixedWing = controlProfile == VehicleControlProfile.fixedWing;
    final planeFirmware =
        qgcFixedWing || controlProfile == VehicleControlProfile.planeVtol;

    if (vehicleAutopilotType == 12) {
      const px4AutoMissionMode = 0x04040000;
      await _setFlightModeAndValidate(px4AutoMissionMode, 'Auto Mission');
      await _armVehicleAndValidate();
      return;
    }

    final autoMode = planeFirmware ? 10 : 3;
    final guidedMode = planeFirmware ? 15 : 4;
    if (vehicleFlying) {
      await _setFlightModeAndValidate(autoMode, 'Auto');
      return;
    }

    if (qgcFixedWing) {
      await _setFlightModeAndValidate(autoMode, 'Auto');
      await _armVehicleAndValidate();
      return;
    }

    if (!vehicleArmed) {
      await _setFlightModeAndValidate(guidedMode, 'Guided');
      await _armVehicleAndValidate();
    }
    if (!qgcFixedWing) {
      _requireCommandSuccess(flightCommandBackend.missionStart(), '启动任务');
    }
  }

  Future<void> _setFlightModeAndValidate(
    int customMode,
    String expectedMode,
  ) async {
    if (_flightModeMatches(expectedMode)) return;

    for (var attempt = 0; attempt < 3; attempt++) {
      _requireCommandSuccess(
        flightCommandBackend.setMode(_mavModeFlagCustomModeEnabled, customMode),
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
  Future<void> armVehicle() async {
    _requireConnected('解锁');
    await _armVehicleAndValidate();
  }

  /// 上锁。飞行中上锁会掉飞机，界面必须确认。
  Future<void> disarmVehicle() async {
    _requireConnected('上锁');
    await _disarmVehicleAndValidate();
  }

  Future<void> _armVehicleAndValidate() async {
    if (vehicleArmed) return;
    _requireCommandSuccess(flightCommandBackend.arm(0), '解锁');
    if (await _waitForVehicleState(
      armed: true,
      timeout: _armValidationWindow,
    )) {
      return;
    }
    throw StateError('解锁失败：飞控未报告 armed=true');
  }

  Future<void> _disarmVehicleAndValidate() async {
    if (!vehicleArmed) return;
    _requireCommandSuccess(flightCommandBackend.disarm(0), '上锁');
    if (await _waitForVehicleState(
      armed: false,
      timeout: _armValidationWindow,
    )) {
      return;
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
    final deadline = DateTime.now().add(timeout);
    while (!_disposed && DateTime.now().isBefore(deadline)) {
      if (_readVehicleSummaryForCommand()) {
        final modeMatches = mode == null || _flightModeMatches(mode);
        final armedMatches = armed == null || vehicleArmed == armed;
        if (modeMatches && armedMatches) return true;
      }
      await Future<void>.delayed(_commandStatePollInterval);
    }
    if (!_readVehicleSummaryForCommand()) return false;
    return (mode == null || _flightModeMatches(mode)) &&
        (armed == null || vehicleArmed == armed);
  }

  bool _readVehicleSummaryForCommand() {
    try {
      final envelope = telemetryBackend.vehicleSummary();
      final data = envelope['data'] as Map<String, Object?>? ?? const {};
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
    _requireConnected('改变高度');
    final current = relativeAltitudeMeters;
    if (!vehiclePositionValid || current == null) {
      throw StateError('改变高度失败：飞机高度未知');
    }
    final delta = targetAltitudeAboveHome - current;
    if (delta.abs() < 0.01) return;
    await _setFlightModeAndValidate(_guidedCustomMode, 'Guided');
    _requireCommandSuccess(flightCommandBackend.changeAltitude(delta), '改变高度');
  }

  Future<void> guidedPause() async {
    _requireConnected('暂停');
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
    _requireCommandSuccess(flightCommandBackend.guidedPause(), '暂停');
  }

  Future<void> guidedOrbit({
    required double latitude,
    required double longitude,
    double radiusMeters = 50,
    double velocityMetersPerSecond = 5,
    double altitudeMeters = 50,
  }) async {
    _requireConnected('环绕飞行');
    final config = jsonEncode({
      'lat': latitude,
      'lon': longitude,
      'radius': radiusMeters,
      'velocity': velocityMetersPerSecond,
      'altitude': altitudeMeters,
    }).toNativeUtf8();
    try {
      _requireCommandSuccess(
        flightCommandBackend.orbit(
          latitude: latitude,
          longitude: longitude,
          radiusMeters: radiusMeters,
          velocityMetersPerSecond: velocityMetersPerSecond,
          altitudeMeters: altitudeMeters,
        ),
        '环绕飞行',
      );
    } finally {
      malloc.free(config);
    }
  }

  /// 设置飞控 home。会改变返航基准，调用方必须先向用户确认。
  Future<void> setVehicleHome(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) async {
    _requireConnected('设置 Home');
    _requireCommandSuccess(
      flightCommandBackend.setHome(latitude, longitude, altitudeMeters),
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
      flightCommandBackend.setRoi(latitude, longitude, altitudeMeters),
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
      flightCommandBackend.changeHeading(
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
      flightCommandBackend.setGlobalOrigin(latitude, longitude, altitudeMeters),
      '设置估计原点',
    );
  }

  /// 打开当前平台对应的 MgCore FFI 共享库。
  ///
  /// 桌面平台的构建脚本会把共享库放到可执行文件旁边；macOS 仍使用
  /// app bundle 的 Frameworks 目录。Android 由系统 linker 按 JNI 库名查找，
  /// 但真正的 USB Host 串口发现仍需要 Android 原生层提供设备路径和权限。
  DynamicLibrary _openPlatformLibrary() {
    if (Platform.isAndroid) {
      return DynamicLibrary.open('libmgcore.so');
    }

    final executable = File(Platform.resolvedExecutable);
    final candidates = <String>[];
    if (Platform.isMacOS) {
      candidates.add(
        '${executable.parent.parent.path}/Frameworks/libmgcore.dylib',
      );
    } else if (Platform.isWindows) {
      candidates.add('${executable.parent.path}/mgcore.dll');
    } else if (Platform.isLinux) {
      candidates.add('${executable.parent.path}/libmgcore.so');
      candidates.add('${executable.parent.path}/lib/libmgcore.so');
    }

    for (final path in candidates) {
      final file = File(path);
      if (file.existsSync()) return DynamicLibrary.open(file.path);
    }

    // Linux distributions may resolve a library installed in the process
    // search path even when it is not beside the executable.
    if (Platform.isLinux) return DynamicLibrary.open('libmgcore.so');

    final expected = candidates.isEmpty
        ? '当前平台的 mgcore 共享库'
        : candidates.join(' 或 ');
    throw FileSystemException('找不到 $expected');
  }

  Future<void> refreshPorts() async {
    if (initializationError != null || _busy || hasSession) return;
    _busy = true;
    message = null;
    _notifyConnectionChanges();
    notifyListeners();
    try {
      final result = _readJson(_listPorts(_core));
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
    if (hasSession || path == null) return;
    selectedPort = path;
    _notifyConnectionChanges();
    notifyListeners();
  }

  void selectBaudRate(int? value) {
    if (hasSession || value == null) return;
    baudRate = value;
    _notifyConnectionChanges();
    notifyListeners();
  }

  Future<void> connect() async {
    if (!canConnect) return;
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
    int? openedSessionId;
    try {
      final nativeUri = uri.toNativeUtf8();
      late final int result;
      try {
        result = _open(_core, nativeUri);
      } finally {
        malloc.free(nativeUri);
      }
      if (result < 0) {
        throw StateError(_openFailureMessage(result, uri));
      }
      openedSessionId = result;
      await _attachSession(result, transportLabel: selectedTransportLabel);
      openedSessionId = null;
    } on Object catch (error) {
      message = _friendlyError(error);
      final openedId = openedSessionId;
      if (openedId != null) {
        try {
          final response = _close(_core, openedId);
          if (response != nullptr) _freeString(response);
        } on Object {
          // Keep the original connect error; backend disposal is best effort.
        }
      }
      if (sessionId == openedId) sessionId = null;
      phase = SerialConnectionPhase.disconnected;
      _firmwareVersionRequested = false;
      _parameterRequestStarted = false;
      _stateTimer?.cancel();
      _telemetryTimer?.cancel();
      _waveformTimer?.cancel();
      _waveformTimer = null;
      _notifyConnectionChanges();
    } finally {
      _busy = false;
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  /// Attach the controller to a native session returned by a connection API.
  Future<void> _attachSession(
    int result, {
    required String transportLabel,
  }) async {
    final selection =
        _readJson(_selectVehicle(_core, result)) as Map<String, Object?>;
    if (selection['success'] != true) {
      final error = selection['error'];
      throw StateError(
        error is Map<String, Object?>
            ? error['message'] as String? ?? '无法选择当前飞控'
            : '无法选择当前飞控',
      );
    }
    sessionId = result;
    _missionPlanning?.bindSession(result);
    _ensureActiveSession(result);
    _firmwareVersionRequested = false;
    _parameterRequestStarted = false;
    parametersLoaded = false;
    parametersLoading = false;
    parameterReceived = 0;
    parameterTotal = 0;
    parameterError = null;
    parameters.clear();
    _parameterMetadataLoaded.clear();
    _parameterMetadataAttempts.clear();
    phase = SerialConnectionPhase.waitingForHeartbeat;
    message = '$transportLabel 链路已打开，正在等待飞控心跳…';
    _notifyConnectionChanges();
    _lastSystemStatusPollMs = 0;
    _stateTimer?.cancel();
    _stateTimer = Timer.periodic(
      const Duration(milliseconds: 750),
      (_) => unawaited(_pollSession()),
    );
    _telemetryTimer?.cancel();
    _telemetryTimer = Timer.periodic(
      _telemetryPollInterval,
      (_) => _pollTelemetry(result),
    );
    _restartWaveformTimer();
    _notifyParameterChanges();
    await _pollSession();
  }

  Future<void> _pollSession() async {
    final id = sessionId;
    if (id == null || _disposed) return;
    final previousPhase = phase;
    final previousMessage = message;
    try {
      final state =
          _readJson(_getSessionState(_core, id)) as Map<String, Object?>;
      _ensureActiveSession(id);
      if (state['success'] != true) {
        _stateTimer?.cancel();
        _telemetryTimer?.cancel();
        _waveformTimer?.cancel();
        _calibrationTimer?.cancel();
        sessionId = null;
        phase = SerialConnectionPhase.disconnected;
        message = previousPhase == SerialConnectionPhase.waitingForHeartbeat
            ? '未收到飞控心跳，连接已关闭。请检查所选设备、波特率或数据链路。'
            : '飞控连接已关闭，请检查线缆或数据链路。';
        _firmwareVersionRequested = false;
        _parameterRequestStarted = false;
        _clearTelemetry();
      } else if (state['heartbeat_alive'] == true) {
        phase = SerialConnectionPhase.connected;
        message = '飞控已连接';
        if (previousPhase != SerialConnectionPhase.connected &&
            _waveformActive) {
          _applyWaveformStreamRate();
          _scheduleWaveformStreamRateRetry();
        }
        if (!_firmwareVersionRequested) {
          _firmwareVersionRequested = true;
          try {
            _readJson(_requestFirmwareVersion(_core));
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
      message = '读取连接状态失败：$error';
      if (message != previousMessage) notifyListeners();
    }
  }

  void _ensureActiveSession(int id) {
    try {
      if (_getActiveVehicle(_core) == id) return;
      final response = _selectVehicle(_core, id);
      if (response != nullptr) _freeString(response);
    } on Object {
      // The session poll will surface a missing session on the next pass.
    }
  }

  void _cancelActiveOperations() {
    for (final operation in const ['params', 'mission', 'calibration', 'log']) {
      final name = operation.toNativeUtf8();
      try {
        final response = _cancelOperation(_core, name);
        if (response != nullptr) _freeString(response);
      } on Object {
        // Best effort: closing the link still tears down the transport.
      } finally {
        malloc.free(name);
      }
    }
  }

  Future<void> disconnect() async {
    final id = sessionId;
    if (id == null || _busy) return;
    _busy = true;
    _notifyConnectionChanges();
    notifyListeners();
    try {
      _cancelActiveOperations();
      final result = _readJson(_close(_core, id)) as Map<String, Object?>;
      if (result['success'] != true) {
        throw StateError(result['error'] as String? ?? '断开连接失败');
      }
      message = '已断开飞控连接';
      sessionId = null;
      _missionPlanning?.bindSession(null);
      phase = SerialConnectionPhase.disconnected;
      _firmwareVersionRequested = false;
      _parameterRequestStarted = false;
      _stateTimer?.cancel();
      _telemetryTimer?.cancel();
      _waveformTimer?.cancel();
      _calibrationTimer?.cancel();
      _clearTelemetry();
    } on Object catch (error) {
      message = _friendlyError(error);
    } finally {
      _busy = false;
      _notifyConnectionChanges();
      notifyListeners();
    }
  }

  Object? _readJson(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      return jsonDecode(pointer.toDartString());
    } finally {
      _freeString(pointer);
    }
  }

  void _pollTelemetry(int id) {
    if (!isConnected || sessionId != id || _disposed) return;
    final previousError = telemetryError;
    final previousSatellites = satellitesVisible;
    final previousFix = gpsFixType;
    final previousHeading = headingDegrees;
    final previousBatteryPercent = batteryRemainingPercent;
    final previousBatteryVoltage = batteryVoltage;
    try {
      final envelope = telemetryBackend.sessionStatus(id);
      if (envelope['success'] != true) {
        final error = envelope['error'];
        telemetryError = error is Map<String, Object?>
            ? error['message'] as String?
            : '读取飞控遥测失败';
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
        final response = _setStreamInterval(_core, messageId, intervalUs);
        if (response != nullptr) _freeString(response);
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
        final written = _readWaveform(
          _core,
          _waveformBuffer,
          _waveformMaxPoints,
          _waveformMaxPoints,
          1, // window_ms = 1：只保留最新一帧
          instance.index,
        );
        if (written <= 0) continue;
        final base = (written - 1) * _waveformFloatsPerPoint + 7;
        waveformMagLatestByInstance[instance.index] = [
          _waveformBuffer[base].toDouble(),
          _waveformBuffer[base + 1].toDouble(),
          _waveformBuffer[base + 2].toDouble(),
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
      final written = _readWaveform(
        _core,
        _waveformBuffer,
        _waveformMaxPoints,
        _waveformMaxPoints,
        waveformWindowMs,
        instance,
      );
      if (written > 0) {
        final view = _waveformBuffer.asTypedList(
          written * _waveformFloatsPerPoint,
        );
        waveformPoints(kind).setRange(0, view.length, view);
        count = written;
        if (kind == WaveformInstanceKind.mag) {
          final base = (written - 1) * _waveformFloatsPerPoint + 7;
          waveformMagLatestByInstance[instance] = [
            _waveformBuffer[base].toDouble(),
            _waveformBuffer[base + 1].toDouble(),
            _waveformBuffer[base + 2].toDouble(),
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
      final payload =
          _readJson(_getWaveformInstances(_core)) as Map<String, Object?>;
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
      flightTimeSeconds = 0;
      flightDistanceMeters = 0;
      _lastFlightLatitude = null;
      _lastFlightLongitude = null;
      return oldTime != flightTimeSeconds ||
          oldDistance != flightDistanceMeters;
    }
    if (!_flightStatsArmed) {
      _flightStatsArmed = true;
      _flightStopwatch
        ..reset()
        ..start();
      flightTimeSeconds = 0;
      flightDistanceMeters = 0;
      _lastFlightLatitude = null;
      _lastFlightLongitude = null;
      return true;
    }
    flightTimeSeconds = _flightStopwatch.elapsedMilliseconds / 1000.0;
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
    final oldFirmware = jsonEncode(firmwareInfo);
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
      if (groups?['sys_status_fresh'] == true) {
        sensorHealth = telemetryBackend.sensorHealth();
      } else {
        sensorHealth = null;
      }
    } on Object {
      // Keep the last valid snapshot. A single failed poll must not make a
      // connected vehicle look disarmed or change its profile transiently.
    }
    return oldFirmware != jsonEncode(firmwareInfo) ||
        oldIdentity != jsonEncode(vehicleIdentity) ||
        oldVersion != jsonEncode(firmwareVersionInfo) ||
        oldSensors != jsonEncode(sensorHealth) ||
        oldArmed != vehicleArmed ||
        oldFlightMode != vehicleFlightMode ||
        oldVehicleType != vehicleType ||
        oldAutopilotType != vehicleAutopilotType ||
        oldLandedState != vehicleLandedState;
  }

  bool _sameIntList(List<int> left, List<int> right) {
    if (left.length != right.length) return false;
    for (var index = 0; index < left.length; index++) {
      if (left[index] != right[index]) return false;
    }
    return true;
  }

  void _clearTelemetry() {
    _missionPlanning?.bindSession(null);
    telemetryError = null;
    flightMessages = const [];
    _lastMessageSeq = 0;
    _clearTelemetryValues();
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
    guidedTargetPosition = null;
    guidedTargetValid = false;
    serialHardwareMappings.clear();
    serialHardwareMappingLoading = false;
    serialHardwareMappingError = null;
    _serialHardwareMappingRequested = false;
    _notifyParameterChanges();
    _notifyTelemetryChanges();
    _notifyProfileChanges();
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

  String _openFailureMessage(int code, String endpoint) => switch (code) {
    -1 => '连接失败：后端核心尚未初始化。',
    -3 => '无法打开链路 $endpoint。请检查设备、网络地址和端口是否可用。',
    -4 => '连接参数无效，请检查端口、地址和波特率。',
    -5 => '当前连接状态不允许执行此操作。',
    -7 => '连接资源正忙，请稍后重试。',
    -14 => '连接会话不存在或已关闭。',
    _ => '连接失败（后端错误码 $code）。',
  };

  @override
  void dispose() {
    _disposed = true;
    _flightStopwatch.stop();
    _stateTimer?.cancel();
    _telemetryTimer?.cancel();
    _waveformTimer?.cancel();
    _calibrationTimer?.cancel();
    if (initializationError == null) {
      if (sessionId != null) {
        final response = _close(_core, sessionId!);
        if (response != nullptr) _freeString(response);
      }
      calloc.free(_waveformBuffer);
      _destroy(_core);
    }
    parameterChanges.dispose();
    telemetryChanges.dispose();
    profileChanges.dispose();
    connectionChanges.dispose();
    waveformChanges.dispose();
    calibrationChanges.dispose();
    _missionPlanning?.dispose();
    super.dispose();
  }
}
