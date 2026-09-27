import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:flutter/foundation.dart';

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

enum CalibrationType { gyroscope, accelerometer, magnetometer, barometer }

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

  bool get isAccelerometer => type == CalibrationType.accelerometer;
  bool get isWaitingOrientation => stage == 'waiting_orientation';
  bool get isSuccess => stage == 'success';
  bool get isFailed => stage == 'failed' || stage == 'cancelled';

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

enum ConnectionTransport { serial, udp, tcp }

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
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getSessionStatus;
  late final Pointer<Utf8> Function(Pointer<Void>) _getFirmwareType;
  late final Pointer<Utf8> Function(Pointer<Void>) _getVehicleFirmware;
  late final Pointer<Utf8> Function(Pointer<Void>) _getFirmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _getSensorHealth;
  late final int Function(Pointer<Void>, Pointer<Float>, int, int, int, int)
  _readWaveform;
  late final Pointer<Utf8> Function(Pointer<Void>) _getWaveformInstances;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _setStreamInterval;
  late final Pointer<Utf8> Function(Pointer<Void>) _getTelemetryFreshness;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestFirmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _requestAllParameters;
  late final Pointer<Utf8> Function(Pointer<Void>) _getAllParameters;
  late final Pointer<Utf8> Function(Pointer<Void>) _getParameterProgress;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _getParameterByName = _library
      .lookup<
        NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
      >('mg_params_get_by_name')
      .asFunction();
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)
  _readParameterDescriptor;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double)
  _writeParameter;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _calibrationStart;
  late final Pointer<Utf8> Function(Pointer<Void>) _calibrationCancel;
  late final Pointer<Utf8> Function(Pointer<Void>) _calibrationGetStatus;
  late final Pointer<Utf8> Function(Pointer<Void>, int)
  _calibrationAccelConfirm;
  late final Pointer<Utf8> Function(Pointer<Void>) _getServoOutputs;
  late final Pointer<Utf8> Function(Pointer<Void>) _getSerialUartNames;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _selectVehicle;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _close;
  late final void Function(Pointer<Utf8>) _freeString;

  final List<SerialPortInfo> ports = [];
  final List<int> baudRates = const [57600, 115200, 230400, 460800, 921600];

  /// 波形缓冲区上限。50s 窗口 @10Hz 约 500 帧，取 600 留余量，
  /// 正常就是逐帧原始点，抽稀只在采样率异常高时才兜底。
  /// 点数直接决定每帧的绘制和分配开销，这个值不要为了"看起来更细"随意调大。
  static const int _waveformMaxPoints = 600;

  /// 每个采样点：相对毫秒 + 加速度 3 轴 + 角速度 3 轴 + 磁力 3 轴。
  static const int _waveformFloatsPerPoint = 10;

  /// 波形页面默认显示的时间窗口（毫秒）。
  static const int _waveformDefaultWindowMs = 50000;

  /// 波形页面取数间隔。传感器校准看的是趋势，1Hz 足够；
  /// 后端仍按 10Hz 采样，所以窗口里的曲线细节不会丢。
  static const Duration _waveformPollInterval = Duration(seconds: 1);

  /// 波形页面可见时请求的采样间隔（10Hz）。SCALED_IMU/2/3 各请求一次，
  /// 按显示需要取数，把串口带宽和缓冲写入量压到常规链路水平。
  static const int _waveformStreamIntervalUs = 100000;
  static const int _waveformIdleStreamIntervalUs = 500000;

  /// 波形页面按采样间隔请求的消息 ID：SCALED_IMU / SCALED_IMU2 / SCALED_IMU3。
  static const List<int> _waveformMessageIds = [26, 116, 129];

  /// 常规遥测轮询间隔。顶栏数据是给人看的，2Hz 足够。
  static const Duration _telemetryPollInterval = Duration(milliseconds: 500);

  /// 舵机输出（电机输出页面用）单独限流，避免每次遥测轮询都解析一遍。
  static const Duration _servoPollInterval = Duration(milliseconds: 1000);
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
  int? batteryRemainingPercent;
  double? batteryVoltage;
  Map<String, Object?>? firmwareInfo;
  Map<String, Object?>? vehicleIdentity;
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
  int _lastServoPollMs = 0;
  bool _busy = false;
  bool _disposed = false;
  bool _firmwareVersionRequested = false;
  bool _parameterRequestStarted = false;
  bool _loadingParameterMetadata = false;
  VehicleControlProfile? _lastPublishedControlProfile;

  bool get busy => _busy;
  bool get isConnected => phase == SerialConnectionPhase.connected;
  bool get hasSession => sessionId != null;
  bool get hasParameters => parametersLoaded && parameters.isNotEmpty;
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

  int get pendingParameterWriteCount => _parameterWriteJobs.length;
  bool get hasParameterWritesInProgress => _parameterWriteJobs.isNotEmpty;
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

  bool get isCalibrationActive => calibrationStatus.active;

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
      final result = _readJson(_calibrationStart(_core, 1, nativeMode));
      if (result is! Map<String, Object?> || result['success'] != true) {
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
      };
      final result = _readJson(_calibrationStart(_core, nativeType, 1));
      if (result is! Map<String, Object?> || result['success'] != true) {
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
      final result = _readJson(_calibrationAccelConfirm(_core, position));
      if (result is! Map<String, Object?> || result['success'] != true) {
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
    try {
      final result = _readJson(_calibrationCancel(_core));
      if (result is! Map<String, Object?> || result['success'] != true) {
        throw StateError(_nativeError(result, '取消校准失败'));
      }
      await _pollCalibrationStatus();
    } on Object catch (error) {
      calibrationError = _friendlyError(error);
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
      final raw = _readJson(_calibrationGetStatus(_core));
      if (raw is! Map<String, Object?>) throw const FormatException('校准状态格式无效');
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
      final result =
          _readJson(_requestAllParameters(_core)) as Map<String, Object?>;
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
      parameterError = error.toString().replaceFirst('Bad state: ', '');
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
            final nativeName = name.toNativeUtf8();
            late final Map<String, Object?> descriptor;
            try {
              descriptor =
                  _readJson(_readParameterDescriptor(_core, nativeName))
                      as Map<String, Object?>;
            } finally {
              malloc.free(nativeName);
            }
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

    try {
      if (previous != null) await previous;
      if (_disposed) return false;
      _ensureWriteSession(requestedSessionId, name);
      _parameterWriteErrors.remove(name);
      _notifyParameterChanges();
      notifyListeners();

      // A value queued behind an in-flight write may already be current by
      // the time its turn arrives.
      final latest = parameters[name];
      if (latest == null || latest.readOnly) {
        throw StateError('参数会话已变化，未能写入 $name');
      }
      if (latest.value == value) return true;

      for (var attempt = 0; attempt < 3; attempt++) {
        _ensureWriteSession(requestedSessionId, name);
        final nativeName = name.toNativeUtf8();
        try {
          final result =
              _readJson(_writeParameter(_core, nativeName, value))
                  as Map<String, Object?>;
          if (result['success'] != true) {
            final error = result['error'];
            throw StateError(
              error is Map<String, Object?>
                  ? error['message'] as String? ?? '参数写入失败'
                  : error as String? ?? '参数写入失败',
            );
          }
        } finally {
          malloc.free(nativeName);
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
      _recordParameterWriteError(
        name,
        error.toString().replaceFirst('Bad state: ', ''),
      );
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
    final nativeName = name.toNativeUtf8();
    try {
      final result =
          _readJson(_getParameterByName(_core, nativeName))
              as Map<String, Object?>;
      if (result['found'] != true) return null;
      return (result['value'] as num?)?.toDouble();
    } finally {
      malloc.free(nativeName);
    }
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
      _getSessionStatus = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            'mg_vehicle_get_session_status',
          )
          .asFunction();
      _getFirmwareType = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_firmware_get_type',
          )
          .asFunction();
      _getVehicleFirmware = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_vehicle_get_firmware',
          )
          .asFunction();
      _getFirmwareVersion = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_vehicle_get_firmware_version',
          )
          .asFunction();
      _getSensorHealth = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_sensors_get_health',
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
      _getTelemetryFreshness = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_telemetry_get_freshness',
          )
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
      _requestAllParameters = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_params_request_all',
          )
          .asFunction();
      _getAllParameters = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_params_get_all',
          )
          .asFunction();
      _getParameterProgress = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_params_get_progress',
          )
          .asFunction();
      _readParameterDescriptor = _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
          >('mg_config_read_param')
          .asFunction();
      _writeParameter = _library
          .lookup<
            NativeFunction<
              Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, Float)
            >
          >('mg_config_write_param')
          .asFunction<
            Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>, double)
          >();
      _calibrationStart = _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32, Int32)>
          >('mg_calibration_start')
          .asFunction();
      _calibrationCancel = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_calibration_cancel',
          )
          .asFunction();
      _calibrationGetStatus = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_calibration_get_status',
          )
          .asFunction();
      _calibrationAccelConfirm = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            'mg_calibration_accel_confirm',
          )
          .asFunction();
      _getServoOutputs = _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(
            'mg_servo_get_outputs',
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
      message = '$selectedTransportLabel 链路已打开，正在等待飞控心跳…';
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
      // The connected session and its timers now own the opened backend handle.
      openedSessionId = null;
      _notifyParameterChanges();
      await _pollSession();
    } on Object catch (error) {
      message = error.toString().replaceFirst('Bad state: ', '');
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

  Future<void> _pollSession() async {
    final id = sessionId;
    if (id == null || _disposed) return;
    final previousPhase = phase;
    final previousMessage = message;
    try {
      final state =
          _readJson(_getSessionState(_core, id)) as Map<String, Object?>;
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

  Future<void> disconnect() async {
    final id = sessionId;
    if (id == null || _busy) return;
    _busy = true;
    _notifyConnectionChanges();
    notifyListeners();
    try {
      final result = _readJson(_close(_core, id)) as Map<String, Object?>;
      if (result['success'] != true) {
        throw StateError(result['error'] as String? ?? '断开连接失败');
      }
      message = '已断开飞控连接';
      sessionId = null;
      phase = SerialConnectionPhase.disconnected;
      _firmwareVersionRequested = false;
      _parameterRequestStarted = false;
      _stateTimer?.cancel();
      _telemetryTimer?.cancel();
      _waveformTimer?.cancel();
      _calibrationTimer?.cancel();
      _clearTelemetry();
    } on Object catch (error) {
      message = error.toString().replaceFirst('Bad state: ', '');
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
      final envelope =
          _readJson(_getSessionStatus(_core, id)) as Map<String, Object?>;
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
      if (now - _lastServoPollMs >= _servoPollInterval.inMilliseconds) {
        _lastServoPollMs = now;
        changed = _pollServoOutputs() || changed;
      }
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
    // 实例数量随"最近有没有收到数据"变化，和取数同一个周期刷新，
    // 只有真的变了才通知界面，避免选择器每秒闪一下。
    _refreshWaveformInstances();
    _notifyWaveformChanges();
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
      final progress =
          _readJson(_getParameterProgress(_core)) as Map<String, Object?>;
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

      final rawParameters = _readJson(_getAllParameters(_core));
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
      final result = _readJson(_getServoOutputs(_core)) as Map<String, Object?>;
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

  bool _pollSystemStatus() {
    final oldFirmware = jsonEncode(firmwareInfo);
    final oldIdentity = jsonEncode(vehicleIdentity);
    final oldVersion = jsonEncode(firmwareVersionInfo);
    final oldSensors = jsonEncode(sensorHealth);
    try {
      final firmwareEnvelope =
          _readJson(_getFirmwareType(_core)) as Map<String, Object?>;
      firmwareInfo = firmwareEnvelope['success'] == true
          ? firmwareEnvelope['data'] as Map<String, Object?>?
          : null;

      vehicleIdentity =
          _readJson(_getVehicleFirmware(_core)) as Map<String, Object?>;

      final versionEnvelope =
          _readJson(_getFirmwareVersion(_core)) as Map<String, Object?>;
      firmwareVersionInfo = versionEnvelope['success'] == true
          ? versionEnvelope['data'] as Map<String, Object?>?
          : null;

      final freshness =
          _readJson(_getTelemetryFreshness(_core)) as Map<String, Object?>;
      final groups = freshness['groups'] as Map<String, Object?>?;
      if (groups?['sys_status_fresh'] == true) {
        sensorHealth =
            _readJson(_getSensorHealth(_core)) as Map<String, Object?>;
      } else {
        sensorHealth = null;
      }
    } on Object {
      firmwareInfo = null;
      vehicleIdentity = null;
      firmwareVersionInfo = null;
      sensorHealth = null;
    }
    return oldFirmware != jsonEncode(firmwareInfo) ||
        oldIdentity != jsonEncode(vehicleIdentity) ||
        oldVersion != jsonEncode(firmwareVersionInfo) ||
        oldSensors != jsonEncode(sensorHealth);
  }

  bool _sameIntList(List<int> left, List<int> right) {
    if (left.length != right.length) return false;
    for (var index = 0; index < left.length; index++) {
      if (left[index] != right[index]) return false;
    }
    return true;
  }

  void _clearTelemetry() {
    telemetryError = null;
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
    super.dispose();
  }
}
