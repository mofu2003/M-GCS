import '../../application/connection_models.dart';
import '../../application/calibration_models.dart';
import '../../application/mission_models.dart';
import '../../application/parameter_models.dart';
import '../../application/flight_log_models.dart';
import '../../application/flight_control_models.dart';
import '../../application/safety_models.dart';

final class MgCoreJsonDecoder {
  SensorTelemetrySnapshot decodeSensorTelemetry(Map<String, Object?> json) {
    SensorVector3 vector(String name) {
      final values = json[name];
      if (values is! Map<Object?, Object?>) return const SensorVector3();
      return SensorVector3(
        x: _double(values['x']) ?? 0,
        y: _double(values['y']) ?? 0,
        z: _double(values['z']) ?? 0,
      );
    }

    return SensorTelemetrySnapshot(
      acceleration: vector('acc_m_s2'),
      gyroscope: vector('gyro_rps'),
      magnetometer: vector('mag_gauss'),
      temperatureCelsius: _double(json['temperature_c']),
      fieldsUpdated: _int(json['fields_updated']) ?? 0,
    );
  }

  ParameterDescriptor decodeParameterDescriptor(Map<String, Object?> json) {
    final name = _string(json['param_name']) ?? '';
    final options = (_asList(json['options']) ?? const [])
        .whereType<Map<Object?, Object?>>()
        .map(
          (item) => ParameterOption(
            value: _int(item['value']) ?? 0,
            label: _string(item['label']) ?? '',
            description: _string(item['description']) ?? '',
          ),
        )
        .where((option) => option.label.isNotEmpty)
        .toList(growable: false);
    return ParameterDescriptor(
      name: name,
      displayName: _string(json['display_name']) ?? name,
      description: _string(json['description']) ?? '',
      group: _string(json['group']) ?? '',
      groupDisplayName: _string(json['group_display_name']) ?? '',
      type: _string(json['type']) ?? '',
      found: _bool(json['found']) ?? false,
      options: options,
      unit: _string(json['unit']) ?? '',
      category: _string(json['category']) ?? '',
      minValue: _double(json['min']),
      maxValue: _double(json['max']),
      increment: _double(json['increment']),
      requiresReboot: _bool(json['requires_reboot']) ?? false,
      readOnly: _bool(json['read_only']) ?? false,
    );
  }

  List<String> decodeParameterGroupNames(Map<String, Object?> json) {
    final items = _asList(json['items']) ?? const [];
    return items
        .whereType<Map<Object?, Object?>>()
        .map((item) => _string(item['name']) ?? '')
        .where((name) => name.isNotEmpty)
        .toList(growable: false);
  }

  List<ParameterDescriptor> decodeParameterGroup(Map<String, Object?> json) {
    final groupDisplayName = _string(json['display_name']) ?? '';
    final parameters = _asList(json['parameters']) ?? const [];
    return parameters
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final descriptor = decodeParameterDescriptor(
            Map<String, Object?>.from(item),
          );
          return descriptor.withGroupDisplayName(groupDisplayName);
        })
        .where((descriptor) => descriptor.name.isNotEmpty)
        .toList(growable: false);
  }

  List<FlightControllerParameter> decodeParameters(Map<String, Object?> json) {
    final items = _asList(json['items']) ?? _asList(json['parameters']);
    if (items == null) {
      return const [];
    }
    return items
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final name = _string(item['name']) ?? '';
          return FlightControllerParameter(
            name: name,
            value: _double(item['value']) ?? 0,
            mavlinkType: _int(item['type']) ?? 0,
            index: _int(item['index']) ?? 0,
            count: _int(item['count']) ?? 0,
          );
        })
        .where((parameter) => parameter.name.isNotEmpty)
        .toList(growable: false);
  }

  ParameterChangeSetPreview decodeParameterChangeSetPreview(
    Map<String, Object?> json,
  ) {
    final items = (_asList(json['items']) ?? const [])
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final map = Map<String, Object?>.from(item);
          return ParameterChangeSetItem(
            name: _string(map['name']) ?? '',
            value: _double(map['value']) ?? 0,
            mavlinkType: _int(map['type']) ?? 9,
            disposition: _string(map['disposition']) ?? 'invalid',
            reason: _string(map['reason']) ?? '',
          );
        })
        .where((item) => item.name.isNotEmpty)
        .toList(growable: false);
    return ParameterChangeSetPreview(
      ready: _int(json['ready']) ?? 0,
      unchanged: _int(json['unchanged']) ?? 0,
      unknown: _int(json['unknown']) ?? 0,
      invalid: _int(json['invalid']) ?? 0,
      items: items,
    );
  }

  ParameterWriteSummary decodeParameterChangeSetCommit(
    Map<String, Object?> json,
  ) {
    final succeeded = (_asList(json['succeeded']) ?? const [])
        .whereType<Map<Object?, Object?>>()
        .map((item) => _string(item['name']) ?? '')
        .where((name) => name.isNotEmpty)
        .toList(growable: false);
    final failed = <String, String>{};
    for (final item
        in (_asList(json['failed']) ?? const [])
            .whereType<Map<Object?, Object?>>()) {
      final name = _string(item['name']) ?? '';
      if (name.isNotEmpty) failed[name] = _string(item['reason']) ?? '写入失败';
    }
    return ParameterWriteSummary(succeeded: succeeded, failed: failed);
  }

  ServoOutputSnapshot decodeServoOutputs(Map<String, Object?> json) {
    final values = (_asList(json['servos']) ?? const [])
        .map((value) => value is num ? value.toInt() : 0)
        .take(16)
        .toList(growable: true);
    while (values.length < 16) {
      values.add(0);
    }
    return ServoOutputSnapshot(
      values: List.unmodifiable(values),
      valid: _bool(json['valid']) ?? values.any((value) => value > 0),
    );
  }

  RemoteControlInputSnapshot decodeRemoteControlInputs(
    Map<String, Object?> json,
  ) {
    final values = (_asList(json['channels']) ?? const [])
        .map((value) => value is num ? value.toInt() : 0)
        .take(18)
        .toList(growable: true);
    while (values.length < 18) {
      values.add(0);
    }
    final channelCount =
        (_int(json['chancount']) ?? _int(json['channel_count']) ?? 0).clamp(
          0,
          18,
        );
    return RemoteControlInputSnapshot(
      values: List.unmodifiable(values),
      channelCount: channelCount,
      valid:
          (_bool(json['valid']) ?? channelCount > 0) &&
          values.take(channelCount).any((value) => value > 0),
    );
  }

  List<SerialPortHardwareDescriptor> decodeSerialPortHardware(
    Map<String, Object?> json,
  ) {
    final items = json['items'];
    if (items is! List<Object?>) return const [];
    return items
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final index = item['serial_index'];
          final flow = item['flow_control'];
          return SerialPortHardwareDescriptor(
            serialIndex: index is num ? index.toInt() : -1,
            name: item['name'] as String? ?? '',
            flowControl: flow is num ? flow.toInt() : null,
          );
        })
        .where((item) => item.serialIndex >= 0 && item.name.isNotEmpty)
        .toList(growable: false);
  }

  CalibrationStatusSnapshot decodeCalibrationStatus(Map<String, Object?> json) {
    return CalibrationStatusSnapshot(
      active: json['active'] == true,
      type: json['type'] as String? ?? '',
      stage: json['stage'] as String? ?? 'idle',
      progress: (json['progress'] as num?)?.toInt() ?? 0,
      currentStep: (json['current_step'] as num?)?.toInt() ?? 0,
      totalSteps: (json['total_steps'] as num?)?.toInt() ?? 0,
      message: json['message'] as String? ?? '',
      failureReason: json['failure_reason'] as String? ?? '',
      retryCount: _int(json['retry_count']) ?? 0,
      maxRetries: _int(json['max_retries']) ?? 0,
      accelerometerMode: _string(json['accelerometer_mode']) ?? '',
    );
  }

  List<CalibrationResultSnapshot> decodeCalibrationHistory(
    Map<String, Object?> json,
  ) {
    SensorVector3? vector(Object? value) {
      final map = _asMap(value);
      if (map == null) return null;
      return SensorVector3(
        x: _double(map['x']) ?? 0,
        y: _double(map['y']) ?? 0,
        z: _double(map['z']) ?? 0,
      );
    }

    return (_asList(json['items']) ?? const [])
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final map = Map<String, Object?>.from(item);
          return CalibrationResultSnapshot(
            type: _string(map['type']) ?? '',
            stage: _string(map['stage']) ?? 'idle',
            quality: _string(map['quality']) ?? 'poor',
            timestampMilliseconds: _int(map['timestamp_ms']) ?? 0,
            accelerometerMode: _string(map['accelerometer_mode']) ?? '',
            offsets: vector(map['offsets']) ?? const SensorVector3(),
            previousOffsets:
                vector(map['previous_offsets']) ?? const SensorVector3(),
            scale: vector(map['scale']),
            diagonals: vector(map['diagonals']),
            offDiagonals: vector(map['offdiagonals']),
            fitness: _double(map['fitness']),
            progress: _int(map['progress']) ?? 0,
            currentStep: _int(map['current_step']) ?? 0,
            totalSteps: _int(map['total_steps']) ?? 0,
            failureReason: _string(map['failure_reason']) ?? '',
          );
        })
        .toList(growable: false);
  }

  CalibrationRetrySnapshot decodeCalibrationRetryInfo(
    Map<String, Object?> json,
  ) => CalibrationRetrySnapshot(
    retryCount: _int(json['retry_count']) ?? 0,
    maxRetries: _int(json['max_retries']) ?? 0,
    backoffMilliseconds: _int(json['backoff_ms']) ?? 0,
    lastFailure: _string(json['last_failure']) ?? '',
  );

  List<ConnectionSerialPort> decodePorts(Map<String, Object?> json) {
    final items =
        _asList(json['ports']) ??
        _asList(json['devices']) ??
        _asList(json['items']);
    if (items == null) {
      return const [];
    }
    return items
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final path =
              _string(item['path']) ??
              _string(item['port']) ??
              _string(item['port_name']) ??
              _string(item['name']) ??
              '';
          return ConnectionSerialPort(
            name:
                _string(item['board_name']) ??
                _string(item['name']) ??
                _string(item['description']) ??
                path,
            path: path,
            description: _string(item['description']),
            manufacturer: _string(item['manufacturer']),
          );
        })
        .where((port) => port.path.isNotEmpty)
        .toList(growable: false);
  }

  ConnectionOperationResult decodeOperation(Map<String, Object?> json) {
    final success = _bool(json['success']) ?? _bool(json['ok']) ?? true;
    return ConnectionOperationResult(
      success: success,
      message:
          _string(json['message']) ??
          _string(json['error']) ??
          (success ? '操作成功' : '操作失败'),
      raw: json,
    );
  }

  List<FlightModeOption> decodeFlightModes(List<Object?> json) {
    return json
        .whereType<Map<Object?, Object?>>()
        .map((item) {
          final name = _string(item['name']) ?? _string(item['mode']) ?? '';
          return FlightModeOption(name: name, label: _string(item['label']));
        })
        .where((item) => item.name.isNotEmpty)
        .toList(growable: false);
  }

  FlightModeStatus decodeFlightModeStatus(Map<String, Object?> json) {
    return FlightModeStatus(
      currentMode:
          _string(json['current_mode']) ?? _string(json['mode']) ?? 'UNKNOWN',
      armed: _bool(json['armed']) ?? false,
      inFlight: _bool(json['in_flight']) ?? false,
      baseMode: _int(json['base_mode']) ?? 0,
      customMode: _int(json['custom_mode']) ?? 0,
    );
  }

  PreflightCheckResult decodePreflightCheck(Map<String, Object?> json) {
    final rawItems =
        _asList(json['checks']) ?? _asList(json['items']) ?? const [];
    final items = rawItems
        .whereType<Map<Object?, Object?>>()
        .map(
          (item) => PreflightCheckItem(
            name: _string(item['name']) ?? '检查项',
            ok: _bool(item['ok']) ?? _bool(item['passed']) ?? false,
            message: _string(item['message']) ?? '',
          ),
        )
        .toList(growable: false);
    return PreflightCheckResult(
      ready: _bool(json['ready']) ?? _bool(json['success']) ?? false,
      message: _string(json['ready_message']) ?? _string(json['message']) ?? '',
      items: items,
    );
  }

  MissionPlanSnapshot decodeMissionPlan(Map<String, Object?> json) {
    final rawItems = json['items'];
    final items = rawItems is List<Object?>
        ? rawItems
              .whereType<Map<Object?, Object?>>()
              .map(_decodeMissionItem)
              .toList(growable: false)
        : const <MissionItemSnapshot>[];
    return MissionPlanSnapshot(
      items: items,
      currentIndex: _int(json['current_index']) ?? 0,
      valid: json['valid'] == true,
    );
  }

  MissionItemSnapshot _decodeMissionItem(Map<Object?, Object?> item) =>
      MissionItemSnapshot(
        sequence: _int(item['seq']) ?? 0,
        command: _int(item['command']) ?? 16,
        commandName: _string(item['command_name']) ?? 'NAV_WAYPOINT',
        frame: _int(item['frame']) ?? 3,
        frameName: _string(item['frame_name']) ?? 'GLOBAL_REL_ALT',
        autoContinue: (_int(item['autocontinue']) ?? 0) != 0,
        latitude: _double(item['lat']),
        longitude: _double(item['lon']) ?? _double(item['x']),
        altitudeMeters: _double(item['z']) ?? _double(item['alt']),
        holdTimeSeconds:
            _double(item['param1']) ?? _double(item['hold_time']) ?? 0,
        speedMetersPerSecond:
            _double(item['speed']) ??
            (_int(item['command']) == 178 ? _double(item['param2']) : null),
      );

  MissionProgressSnapshot decodeMissionProgress(Map<String, Object?> json) {
    final state = switch (_string(json['state'])) {
      'downloading' => MissionTransferState.downloading,
      'uploading' => MissionTransferState.uploading,
      'waiting_ack' => MissionTransferState.waitingAck,
      _ => MissionTransferState.idle,
    };
    return MissionProgressSnapshot(
      state: state,
      total: _int(json['total']) ?? 0,
      received: _int(json['received']) ?? 0,
      sent: _int(json['sent']) ?? 0,
      result: _int(json['result']) ?? 0,
      error: _string(json['error']) ?? '',
      progressPercent: (_int(json['progress_pct']) ?? 0).clamp(0, 100),
    );
  }

  MissionGenerationResult decodeMissionGeneration(Map<String, Object?> json) {
    final rawItems = json['waypoints'];
    final items = rawItems is List
        ? rawItems
              .whereType<Map<Object?, Object?>>()
              .map(_decodeMissionItem)
              .toList(growable: false)
        : const <MissionItemSnapshot>[];
    return MissionGenerationResult(
      valid: json['valid'] == true,
      items: items,
      waypointCount: _int(json['waypoint_count']) ?? items.length,
      flightLineCount: _int(json['flight_line_count']) ?? 0,
      totalDistanceMeters: _double(json['total_distance_m']) ?? 0,
      estimatedTimeSeconds: _double(json['estimated_time_s']) ?? 0,
      areaSquareMeters: _double(json['area_m2']) ?? 0,
      photoCount: _int(json['photo_count']) ?? 0,
      error: _string(json['error']) ?? '',
    );
  }

  GeofenceConfigSnapshot decodeGeofenceConfig(Map<String, Object?> json) =>
      GeofenceConfigSnapshot(
        enabled: _bool(json['enabled']) ?? false,
        maxAltitudeMeters: _double(json['max_altitude']) ?? 120,
        minAltitudeMeters: _double(json['min_altitude']) ?? 0,
        action: _int(json['action']) ?? 0,
        returnAtMinimum: _bool(json['return_at_min']) ?? false,
        breachDistanceMeters: _double(json['breach_distance']) ?? 10,
      );

  List<FencePointSnapshot> decodeGeofencePoints(List<Object?> json) => [
    for (final value in json.whereType<Map<Object?, Object?>>())
      FencePointSnapshot(
        latitude: _double(value['lat']) ?? 0,
        longitude: _double(value['lon']) ?? 0,
        altitudeMeters: _double(value['alt']) ?? 0,
      ),
  ];

  GeofenceValidationSnapshot decodeGeofenceValidation(
    Map<String, Object?> json,
  ) => GeofenceValidationSnapshot(
    valid: _bool(json['valid']) ?? false,
    error: _string(json['error']) ?? '',
    warnings: (_asList(json['warnings']) ?? const [])
        .whereType<Map<Object?, Object?>>()
        .map((item) => _string(item['msg']) ?? '')
        .where((value) => value.isNotEmpty)
        .toList(growable: false),
    areaSquareMeters: _double(json['area_m2']) ?? 0,
    perimeterMeters: _double(json['perimeter_m']) ?? 0,
  );

  AppConnectionSummary decodeConnection(Map<String, Object?> json) {
    final vehicle = _asMap(json['vehicle']) ?? const {};
    final heartbeat = _asMap(json['heartbeat']) ?? const {};
    final connected =
        _bool(json['connected']) ?? _string(json['state']) == 'connected';
    return AppConnectionSummary(
      status: connected ? ConnectionStatus.connected : ConnectionStatus.idle,
      label:
          _string(vehicle['vehicle_name']) ??
          _string(vehicle['board_name']) ??
          _string(json['vehicle_name']) ??
          _string(json['board_name']) ??
          (connected ? '已连接飞控' : '未连接'),
      detail: _string(json['state']) ?? _string(json['detail']) ?? '等待连接',
      activeVehicleId:
          _int(vehicle['active_vehicle_id']) ??
          _int(vehicle['vehicle_id']) ??
          _int(json['active_vehicle_id']) ??
          _int(json['vehicle_id']),
      heartbeatAgeMs:
          _int(heartbeat['age_ms']) ?? _int(json['heartbeat_age_ms']),
    );
  }

  FlightDashboardData decodeDashboard(Map<String, Object?> json) {
    final telemetry = _asMap(json['telemetry']) ?? json;
    final summary = _asMap(telemetry['summary']) ?? telemetry;
    final battery = _asMap(telemetry['battery']) ?? telemetry;
    final position =
        _asMap(telemetry['global_position']) ??
        _asMap(telemetry['position']) ??
        telemetry;
    final gps = _asMap(telemetry['gps']) ?? const {};
    final attitude = _asMap(telemetry['attitude']) ?? const {};
    final vfrHud = _asMap(telemetry['vfr_hud']) ?? telemetry;
    final validity = _asMap(json['validity']) ?? const {};
    return FlightDashboardData(
      valid:
          _bool(validity['attitude']) ??
          _bool(telemetry['valid']) ??
          _bool(json['valid']) ??
          false,
      mode:
          _string(summary['flight_mode']) ??
          _string(telemetry['mode']) ??
          _string(json['mode']) ??
          'UNKNOWN',
      armed:
          _bool(summary['armed']) ??
          _bool(telemetry['armed']) ??
          _bool(json['armed']) ??
          false,
      altitudeMeters:
          _double(position['alt_rel_m']) ??
          _double(position['relative_alt_m']) ??
          _double(position['altitude_m']) ??
          _double(position['relative_altitude_m']),
      groundSpeedMetersPerSecond:
          _double(vfrHud['groundspeed_m_s']) ??
          _double(telemetry['ground_speed_m_s']) ??
          _double(telemetry['groundspeed']),
      batteryVoltage:
          _double(battery['voltage_v']) ?? _double(battery['voltage']),
      batteryRemainingPercent:
          _int(battery['remaining_pct']) ??
          _int(battery['remaining_percent']) ??
          _int(battery['battery_remaining']),
      latitude:
          _double(position['lat_deg']) ??
          _double(position['lat']) ??
          _double(position['latitude']),
      longitude:
          _double(position['lon_deg']) ??
          _double(position['lon']) ??
          _double(position['longitude']),
      satellitesVisible:
          _int(gps['satellites']) ??
          _int(gps['satellites_visible']) ??
          _int(telemetry['satellites']),
      pitchDegrees: _double(attitude['pitch_deg']),
      rollDegrees: _double(attitude['roll_deg']),
      headingDegrees:
          _double(vfrHud['heading_deg']) ?? _double(attitude['yaw_deg']),
      airspeedMetersPerSecond: _double(vfrHud['airspeed_m_s']),
      climbMetersPerSecond: _double(vfrHud['climb_m_s']),
    );
  }

  ConnectionSessionSnapshot decodeSession({
    required Map<String, Object?> diagnostics,
    required Map<String, Object?> dashboard,
    ConnectionBootstrapSnapshot bootstrap = const ConnectionBootstrapSnapshot(),
  }) {
    return ConnectionSessionSnapshot(
      connection: decodeConnection(diagnostics),
      dashboard: decodeDashboard(dashboard),
      bootstrap: bootstrap,
      rawDiagnostics: diagnostics,
    );
  }

  ConnectionBootstrapSnapshot decodeBootstrap({
    required Map<String, Object?> diagnostics,
    required Map<String, Object?> parameterProgress,
    required Map<String, Object?> remoteControl,
    required Map<String, Object?> firmwareType,
    required Map<String, Object?> firmwareVersion,
  }) {
    final vehicle = _asMap(diagnostics['vehicle']) ?? const {};
    final link = _asMap(diagnostics['link']) ?? const {};
    final firmware = _asMap(firmwareType['data']) ?? firmwareType;
    final version = _asMap(firmwareVersion['data']) ?? firmwareVersion;
    final received = _int(parameterProgress['received']) ?? 0;
    final total = _int(parameterProgress['total']) ?? 0;
    final rawRssi = _int(remoteControl['rssi']);
    final rssiAvailable = rawRssi != null && rawRssi > 0 && rawRssi < 255;
    final versionReceived = _bool(version['received']) ?? false;
    final measuring = _bool(link['measuring']) ?? true;
    final airframe = _airframeLabel(
      firmware: _string(firmware['firmware']),
      vehicleType: _string(firmware['vehicle_type']),
    );

    return ConnectionBootstrapSnapshot(
      parameterProgress: ParameterSyncProgress(
        received: received,
        total: total,
      ),
      identity: FlightControllerIdentity(
        name:
            _string(vehicle['vehicle_name']) ??
            _string(vehicle['board_name']) ??
            '已连接飞控',
        airframe: airframe,
        firmwareType: _string(firmware['firmware']) ?? '固件待确认',
        firmwareMajor: versionReceived ? _int(version['major']) : null,
        firmwareMinor: versionReceived ? _int(version['minor']) : null,
        firmwarePatch: versionReceived ? _int(version['patch']) : null,
      ),
      linkQuality: LinkQuality(
        measuring: measuring,
        received: _int(link['received']) ?? 0,
        lost: _int(link['lost']) ?? 0,
        percent: measuring ? null : _int(link['quality_pct']),
      ),
      remoteControlSignal: RemoteControlSignal(
        percent: rssiAvailable && rawRssi <= 100 ? rawRssi : null,
        rawRssi: rssiAvailable ? rawRssi : null,
      ),
    );
  }

  FlightControllerMessage decodeFlightControllerMessage(
    Map<String, Object?> json,
  ) {
    return FlightControllerMessage(
      text: _string(json['text']) ?? '飞控未提供文本',
      severity: _string(json['severity_label']) ?? 'info',
      receivedAt: DateTime.now(),
    );
  }

  List<FlightLogEntry> decodeFlightLogs(List<Object?> items) {
    return items
        .map(_asMap)
        .whereType<Map<String, Object?>>()
        .map(
          (item) => FlightLogEntry(
            id: _int(item['id']) ?? 0,
            sizeBytes: _int(item['size_bytes']) ?? _int(item['size']) ?? 0,
            timestamp: flightLogTimestampFromUtcSeconds(
              _int(item['time_utc']) ?? 0,
            ),
          ),
        )
        .where((item) => item.id > 0)
        .toList(growable: false);
  }

  FlightLogDownloadProgress decodeFlightLogProgress(Map<String, Object?> json) {
    final state = switch (_string(json['state'])) {
      'downloading' => FlightLogDownloadState.downloading,
      'completed' => FlightLogDownloadState.completed,
      'cancelled' => FlightLogDownloadState.cancelled,
      'failed' => FlightLogDownloadState.failed,
      _ => FlightLogDownloadState.idle,
    };
    return FlightLogDownloadProgress(
      logId: _int(json['log_id']) ?? 0,
      receivedBytes: _int(json['received_bytes']) ?? 0,
      totalBytes: _int(json['total_bytes']) ?? 0,
      state: state,
      destinationPath: _string(json['path']),
      message: _string(json['message']) ?? '',
    );
  }
}

String _airframeLabel({String? firmware, String? vehicleType}) {
  return switch (vehicleType) {
    'FixedWing' => 'Plane',
    'Copter' => 'Copter',
    'Rover' => 'Rover',
    'Submarine' => 'Sub',
    'AntennaTracker' => 'Tracker',
    'VTOL' when firmware == 'ArduPilot' => 'QPlane',
    'VTOL' => 'VTOL',
    _ => '机架待确认',
  };
}

List<Object?>? _asList(Object? value) => value is List<Object?> ? value : null;

Map<String, Object?>? _asMap(Object? value) {
  if (value is Map<String, Object?>) {
    return value;
  }
  if (value is Map<Object?, Object?>) {
    return value.map((key, value) => MapEntry('$key', value));
  }
  return null;
}

String? _string(Object? value) => value == null ? null : '$value';

bool? _bool(Object? value) {
  if (value is bool) {
    return value;
  }
  if (value is String) {
    return value == 'true' || value == 'connected';
  }
  return null;
}

int? _int(Object? value) {
  if (value is int) {
    return value;
  }
  if (value is num) {
    return value.round();
  }
  if (value is String) {
    return int.tryParse(value);
  }
  return null;
}

double? _double(Object? value) {
  if (value is double) {
    return value;
  }
  if (value is num) {
    return value.toDouble();
  }
  if (value is String) {
    return double.tryParse(value);
  }
  return null;
}
