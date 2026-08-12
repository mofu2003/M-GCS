import 'package:flutter_test/flutter_test.dart';
import 'package:mgcs/application/connection_models.dart';
import 'package:mgcs/application/mgcore/mgcore_json_decoder.dart';

void main() {
  test('decodes serial ports from the real mgcore FFI list', () {
    final decoder = MgCoreJsonDecoder();

    final ports = decoder.decodePorts({
      'items': [
        {'port_name': '/dev/cu.usbserial-1', 'manufacturer': 'FTDI'},
      ],
    });

    expect(ports, hasLength(1));
    expect(ports.single.path, '/dev/cu.usbserial-1');
    expect(ports.single.manufacturer, 'FTDI');
  });

  test('decodes the parameter list returned by mgcore', () {
    final decoder = MgCoreJsonDecoder();

    final parameters = decoder.decodeParameters({
      'items': [
        {
          'name': 'BATT_MONITOR',
          'value': 4.0,
          'type': 9,
          'index': 2,
          'count': 3,
        },
      ],
    });

    expect(parameters, hasLength(1));
    expect(parameters.single.name, 'BATT_MONITOR');
    expect(parameters.single.value, 4.0);
    expect(parameters.single.mavlinkType, 9);
    expect(parameters.single.index, 2);
    expect(parameters.single.count, 3);
  });

  test('decodes parameter editor metadata and enum options', () {
    final descriptor = MgCoreJsonDecoder().decodeParameterDescriptor({
      'param_name': 'SERIAL1_PROTOCOL',
      'display_name': '串口协议',
      'description': '串口使用的协议',
      'group': 'SERIAL',
      'type': 'enum',
      'found': true,
      'requires_reboot': true,
      'read_only': false,
      'unit': '',
      'category': 'Standard',
      'min': -1,
      'max': 50,
      'increment': 1,
      'options': [
        {'value': -1, 'label': '无'},
        {'value': 1, 'label': 'MAVLink1'},
        {'value': 2, 'label': 'MAVLink2', 'description': '推荐'},
      ],
    });

    expect(descriptor.type, 'enum');
    expect(descriptor.requiresReboot, isTrue);
    expect(descriptor.minValue, -1);
    expect(descriptor.maxValue, 50);
    expect(descriptor.increment, 1);
    expect(descriptor.options, hasLength(3));
    expect(descriptor.options.last.value, 2);
    expect(descriptor.options.last.label, 'MAVLink2');
    expect(descriptor.options.last.description, '推荐');
  });

  test('prefers the detected board name for a serial port label', () {
    final decoder = MgCoreJsonDecoder();

    final ports = decoder.decodePorts({
      'items': [
        {
          'port_name': '/dev/cu.usbmodem1101',
          'description': 'SULILGH7-P1-P2',
          'board_name': 'SULILGH7-P1-P2',
        },
      ],
    });

    expect(ports.single.name, 'SULILGH7-P1-P2');
    expect(ports.single.path, '/dev/cu.usbmodem1101');
  });

  test('decodes the real diagnostics and dashboard snapshot contracts', () {
    final decoder = MgCoreJsonDecoder();

    final snapshot = decoder.decodeSession(
      diagnostics: {
        'connected': true,
        'state': 'connected',
        'heartbeat': {'age_ms': 180},
        'target': {'flight_mode': 'LOITER', 'armed': false},
        'vehicle': {'active_vehicle_id': 7, 'board_name': 'CubeOrange'},
      },
      dashboard: {
        'telemetry': {
          'summary': {'flight_mode': 'LOITER', 'armed': false},
          'attitude': {'roll_deg': 1.2, 'pitch_deg': -2.3, 'yaw_deg': 86.0},
          'gps': {'satellites': 14},
          'global_position': {
            'lat': 31.2,
            'lon': 121.4,
            'relative_alt_m': 42.5,
          },
          'vfr_hud': {
            'airspeed_m_s': 10.5,
            'groundspeed_m_s': 1.25,
            'climb_m_s': -0.4,
            'heading_deg': 86.0,
          },
          'battery': {'voltage_v': 12.3, 'remaining_percent': 88},
        },
        'validity': {
          'heartbeat': true,
          'attitude': true,
          'gps': true,
          'global_position': true,
          'battery': true,
          'vfr_hud': true,
        },
      },
    );

    expect(snapshot.connection.status, ConnectionStatus.connected);
    expect(snapshot.connection.label, 'CubeOrange');
    expect(snapshot.connection.activeVehicleId, 7);
    expect(snapshot.connection.heartbeatAgeMs, 180);
    expect(snapshot.dashboard.mode, 'LOITER');
    expect(snapshot.dashboard.batteryRemainingPercent, 88);
    expect(snapshot.dashboard.altitudeMeters, 42.5);
    expect(snapshot.dashboard.satellitesVisible, 14);
    expect(snapshot.dashboard.airspeedMetersPerSecond, 10.5);
    expect(snapshot.dashboard.groundSpeedMetersPerSecond, 1.25);
    expect(snapshot.dashboard.climbMetersPerSecond, -0.4);
  });

  test('decodes parameter progress and radio connection bootstrap data', () {
    final decoder = MgCoreJsonDecoder();

    final bootstrap = decoder.decodeBootstrap(
      diagnostics: {
        'vehicle': {'vehicle_name': 'M-GCS Plane'},
        'link': {
          'measuring': false,
          'received': 96,
          'lost': 4,
          'quality_pct': 96,
        },
      },
      parameterProgress: {'received': 523, 'total': 1046},
      remoteControl: {'rssi': 72},
      firmwareType: {
        'data': {'firmware': 'ArduPilot', 'vehicle_type': 'Copter'},
      },
      firmwareVersion: {
        'data': {'received': true, 'major': 4, 'minor': 5, 'patch': 7},
      },
    );

    expect(bootstrap.parameterProgress.received, 523);
    expect(bootstrap.parameterProgress.total, 1046);
    expect(bootstrap.identity.name, 'M-GCS Plane');
    expect(bootstrap.identity.airframe, 'Copter');
    expect(bootstrap.identity.firmwareType, 'ArduPilot');
    expect(bootstrap.identity.versionLabel, '4.5.7');
    expect(bootstrap.linkQuality.percent, 96);
    expect(bootstrap.linkQuality.measuring, isFalse);
    expect(bootstrap.remoteControlSignal.percent, 72);

    final unavailable = decoder.decodeBootstrap(
      diagnostics: const {},
      parameterProgress: const {},
      remoteControl: {'rssi': 255},
      firmwareType: const {},
      firmwareVersion: const {},
    );
    expect(unavailable.remoteControlSignal.percent, isNull);
  });

  test('decodes all remote control input channels', () {
    final snapshot = MgCoreJsonDecoder().decodeRemoteControlInputs({
      'chancount': 16,
      'channels': List<int>.generate(18, (index) => 1400 + index),
    });

    expect(snapshot.valid, isTrue);
    expect(snapshot.channelCount, 16);
    expect(snapshot.values, hasLength(18));
    expect(snapshot.values.first, 1400);
    expect(snapshot.values[15], 1415);
  });

  test('honors explicit invalid remote control snapshots', () {
    final snapshot = MgCoreJsonDecoder().decodeRemoteControlInputs({
      'valid': false,
      'chancount': 16,
      'channels': List<int>.filled(18, 1500),
    });

    expect(snapshot.valid, isFalse);
    expect(snapshot.channelCount, 16);
    expect(snapshot.values.first, 1500);
  });

  test('decodes calibration history, quality, and retry state', () {
    final decoder = MgCoreJsonDecoder();
    final history = decoder.decodeCalibrationHistory({
      'items': [
        {
          'type': 'magnetometer',
          'stage': 'success',
          'quality': 'good',
          'timestamp_ms': 1730000000000,
          'accelerometer_mode': 'level',
          'offsets': {'x': 12.5, 'y': -8.25, 'z': 4},
          'previous_offsets': {'x': 1, 'y': 2, 'z': 3},
          'fitness': 18.5,
          'diagonals': {'x': 1.1, 'y': 1.2, 'z': 1.3},
          'offdiagonals': {'x': 0.1, 'y': 0.2, 'z': 0.3},
          'progress': 100,
          'current_step': 0,
          'total_steps': 0,
          'failure_reason': '',
        },
      ],
    });
    final retry = decoder.decodeCalibrationRetryInfo({
      'retry_count': 1,
      'max_retries': 3,
      'backoff_ms': 1000,
      'last_failure': 'sample coverage too low',
    });

    expect(history, hasLength(1));
    expect(history.single.type, 'magnetometer');
    expect(history.single.succeeded, isTrue);
    expect(history.single.quality, 'good');
    expect(history.single.accelerometerMode, 'level');
    expect(history.single.offsets.x, 12.5);
    expect(history.single.previousOffsets.z, 3);
    expect(history.single.fitness, 18.5);
    expect(history.single.diagonals?.y, 1.2);
    expect(history.single.offDiagonals?.z, 0.3);
    expect(retry.retryCount, 1);
    expect(retry.maxRetries, 3);
    expect(retry.lastFailure, 'sample coverage too low');
  });
}
