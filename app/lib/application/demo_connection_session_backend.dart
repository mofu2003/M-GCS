import 'connection_models.dart';
import 'connection_session_backend.dart';
import 'mission_models.dart';
import 'parameter_models.dart';

final class DemoConnectionSessionBackend
    implements
        ConnectionSessionBackend,
        ParameterSessionBackend,
        ParameterFileSessionBackend,
        ParameterRefreshSessionBackend,
        ServoOutputSessionBackend,
        MissionSessionBackend {
  bool _connected = false;
  bool _parameterSyncRequested = false;
  int _parameterProgressReads = 0;
  List<FlightControllerParameter> _parameters = const [
    FlightControllerParameter(
      name: 'BATT_MONITOR',
      value: 4,
      mavlinkType: 9,
      index: 0,
      count: 4,
    ),
    FlightControllerParameter(
      name: 'BATT_ENABLE',
      value: 1,
      mavlinkType: 9,
      index: 1,
      count: 4,
    ),
    FlightControllerParameter(
      name: 'INS_GYR_ID',
      value: 1,
      mavlinkType: 6,
      index: 2,
      count: 4,
    ),
    FlightControllerParameter(
      name: 'ATC_RAT_RLL_P',
      value: 0.135,
      mavlinkType: 9,
      index: 3,
      count: 4,
    ),
  ];
  List<MissionItemSnapshot> _mission = const [
    MissionItemSnapshot(
      sequence: 0,
      command: 16,
      commandName: 'NAV_WAYPOINT',
      frame: 3,
      frameName: 'GLOBAL_RELATIVE_ALT',
      autoContinue: true,
      latitude: 31.2304,
      longitude: 121.4737,
      altitudeMeters: 60,
    ),
  ];

  @override
  Future<List<ConnectionSerialPort>> listSerialPorts() async {
    return const [
      ConnectionSerialPort(
        name: 'Demo serial',
        path: '/dev/tty.demo',
        description: '离线演示链路',
        manufacturer: 'M-GCS',
      ),
    ];
  }

  @override
  Future<ConnectionOperationResult> connect(ConnectionRequest request) async {
    _connected = true;
    return const ConnectionOperationResult(success: true, message: '已进入离线演示连接');
  }

  @override
  Future<ConnectionOperationResult> disconnect() async {
    _connected = false;
    _parameterSyncRequested = false;
    _parameterProgressReads = 0;
    return const ConnectionOperationResult(success: true, message: '已断开');
  }

  @override
  Future<ConnectionOperationResult> startParameterSync() async {
    if (!_connected) {
      return const ConnectionOperationResult(success: false, message: '未连接飞控');
    }
    _parameterSyncRequested = true;
    _parameterProgressReads = 0;
    return const ConnectionOperationResult(success: true, message: '已请求参数列表');
  }

  @override
  Future<List<FlightControllerParameter>> loadParameters() async => _parameters;

  @override
  Future<List<FlightControllerParameter>> refreshParameters() async =>
      _parameters;

  @override
  Future<ServoOutputSnapshot> loadServoOutputs() async => ServoOutputSnapshot(
    valid: _connected,
    values: _connected
        ? List<int>.generate(16, (index) => 1460 + index * 5)
        : List<int>.filled(16, 0),
  );

  @override
  Future<MissionPlanSnapshot> downloadMission() async {
    if (!_connected) throw StateError('未连接飞控');
    return MissionPlanSnapshot(items: _mission, valid: _mission.isNotEmpty);
  }

  @override
  Future<ConnectionOperationResult> uploadMission(
    List<MissionItemSnapshot> items,
  ) async {
    if (!_connected) {
      return const ConnectionOperationResult(success: false, message: '未连接飞控');
    }
    _mission = items
        .asMap()
        .entries
        .map((entry) => entry.value.copyWith(sequence: entry.key))
        .toList(growable: false);
    return const ConnectionOperationResult(success: true, message: '演示任务已写入');
  }

  @override
  Future<List<ParameterFileEntry>> parseParameterFile(String content) async {
    const skipped = {
      'SYSID_SW_MREV',
      'WP_TOTAL',
      'CMD_TOTAL',
      'FENCE_TOTAL',
      'SYS_NUM_RESETS',
      'ARSPD_OFFSET',
      'GND_ABS_PRESS',
      'GND_TEMP',
      'BARO1_GND_PRESS',
      'BARO2_GND_PRESS',
      'BARO3_GND_PRESS',
      'BARO_GND_TEMP',
      'CMD_INDEX',
      'LOG_LASTFILE',
      'FORMAT_VERSION',
    };
    final entries = <ParameterFileEntry>[];
    for (final rawLine in content.split(RegExp(r'\r?\n'))) {
      final line = rawLine.trim();
      if (line.isEmpty || line.startsWith('#')) continue;
      final fields = line.split(RegExp(r'[,\s]+'));
      if (fields.length < 2 || skipped.contains(fields.first)) continue;
      final value = double.tryParse(fields[1]);
      if (value == null || !value.isFinite) continue;
      entries.add(ParameterFileEntry(name: fields.first, value: value));
    }
    if (entries.isEmpty) {
      throw const FormatException('文件中没有可读取的参数');
    }
    return entries;
  }

  @override
  Future<String> createParameterFile(
    List<FlightControllerParameter> parameters,
  ) async {
    final sorted = [...parameters]..sort((a, b) => a.name.compareTo(b.name));
    return '${sorted.map((item) => '${item.name},${item.value}').join('\n')}\n';
  }

  @override
  Future<ParameterDescriptor> loadParameterDescriptor(String name) async {
    return switch (name) {
      'BATT_MONITOR' => const ParameterDescriptor(
        name: 'BATT_MONITOR',
        displayName: '电池监测器',
        description: '选择主电池监测器的类型。',
        group: '电池',
        groupDisplayName: '电池',
        type: 'enum',
        found: true,
        options: [
          ParameterOption(value: 0, label: '禁用'),
          ParameterOption(value: 3, label: '模拟电压'),
          ParameterOption(value: 4, label: '模拟电压和电流'),
        ],
      ),
      'BATT_ENABLE' => const ParameterDescriptor(
        name: 'BATT_ENABLE',
        displayName: '电池监测启用',
        description: '控制第二路电池监测是否启用。',
        group: '电池',
        groupDisplayName: '电池',
        type: 'bool',
        found: true,
      ),
      _ => ParameterDescriptor(
        name: name,
        displayName: name,
        description: '',
        group: '',
        groupDisplayName: '',
        type: '',
        found: false,
      ),
    };
  }

  @override
  Future<List<ParameterDescriptor>> loadParameterDescriptors() async => [
    await loadParameterDescriptor('BATT_MONITOR'),
    await loadParameterDescriptor('BATT_ENABLE'),
  ];

  @override
  Future<ConnectionOperationResult> writeParameter(
    FlightControllerParameter parameter,
    double value,
  ) async {
    _parameters = _parameters
        .map(
          (item) =>
              item.name == parameter.name ? item.copyWith(value: value) : item,
        )
        .toList(growable: false);
    return const ConnectionOperationResult(success: true, message: '参数已写入');
  }

  @override
  Future<ConnectionSessionSnapshot> loadSessionSnapshot(
    ConnectionRequest request,
  ) async {
    return ConnectionSessionSnapshot(
      connection: AppConnectionSummary(
        status: _connected ? ConnectionStatus.connected : ConnectionStatus.idle,
        label: _connected ? '演示飞控' : '未连接',
        detail: _connected ? '等待接入 mgcore native library' : '离线',
        activeVehicleId: _connected ? 1 : null,
        heartbeatAgeMs: _connected ? 120 : null,
      ),
      dashboard: await loadTelemetrySnapshot(),
    );
  }

  @override
  Future<FlightDashboardData> loadTelemetrySnapshot() async {
    return FlightDashboardData(
      valid: _connected,
      mode: _connected ? 'STABILIZE' : 'UNKNOWN',
      armed: false,
      altitudeMeters: _connected ? 12.4 : null,
      groundSpeedMetersPerSecond: _connected ? 0.8 : null,
      batteryVoltage: _connected ? 12.1 : null,
      batteryRemainingPercent: _connected ? 87 : null,
      latitude: _connected ? 31.2304 : null,
      longitude: _connected ? 121.4737 : null,
      satellitesVisible: _connected ? 14 : null,
      airspeedMetersPerSecond: _connected ? 12.6 : null,
      climbMetersPerSecond: _connected ? -0.2 : null,
    );
  }

  @override
  Future<ConnectionBootstrapSnapshot> loadBootstrapSnapshot() async {
    if (!_connected) {
      return const ConnectionBootstrapSnapshot();
    }
    if (_parameterSyncRequested) {
      _parameterProgressReads += 1;
    }
    final received = _parameterSyncRequested
        ? (_parameterProgressReads * 180).clamp(0, 1046)
        : 0;
    return ConnectionBootstrapSnapshot(
      parameterProgress: ParameterSyncProgress(received: received, total: 1046),
      identity: const FlightControllerIdentity(
        name: '演示飞控',
        airframe: 'Copter',
        firmwareType: 'ArduPilot',
        firmwareMajor: 4,
        firmwareMinor: 5,
        firmwarePatch: 7,
      ),
      linkQuality: const LinkQuality(
        measuring: false,
        received: 96,
        lost: 4,
        percent: 96,
      ),
      remoteControlSignal: const RemoteControlSignal(percent: 72, rawRssi: 72),
    );
  }

  @override
  void dispose() {}
}
