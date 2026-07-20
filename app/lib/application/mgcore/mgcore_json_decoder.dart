import '../connection_models.dart';
import '../parameter_models.dart';

final class MgCoreJsonDecoder {
  ParameterDescriptor decodeParameterDescriptor(Map<String, Object?> json) {
    final name = _string(json['param_name']) ?? '';
    return ParameterDescriptor(
      name: name,
      displayName: _string(json['display_name']) ?? name,
      description: _string(json['description']) ?? '',
      group: _string(json['group']) ?? '',
      groupDisplayName: _string(json['group_display_name']) ?? '',
      type: _string(json['type']) ?? '',
      found: _bool(json['found']) ?? false,
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
          return ParameterDescriptor(
            name: descriptor.name,
            displayName: descriptor.displayName,
            description: descriptor.description,
            group: descriptor.group,
            groupDisplayName: groupDisplayName,
            type: descriptor.type,
            found: descriptor.found,
          );
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
