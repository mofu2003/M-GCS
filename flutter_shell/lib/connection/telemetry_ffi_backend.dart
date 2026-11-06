import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'native_core.dart';

/// Flutter 到遥测相关 FFI 的适配层。
///
/// 只负责动态符号绑定和 JSON 解包，遥测状态、freshness 和页面通知仍由
/// ConnectionController 管理，避免把状态机和 ABI 绑定继续混在一起。
final class TelemetryFfiBackend {
  TelemetryFfiBackend(this._library, this._core) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _getSessionStatus = _lookupSession('mg_vehicle_get_session_status');
    _getFirmwareType = _lookup1('mg_firmware_get_type');
    _getVehicleFirmware = _lookup1('mg_vehicle_get_firmware');
    _getFirmwareVersion = _lookup1('mg_vehicle_get_firmware_version');
    _getSensorHealth = _lookup1('mg_sensors_get_health');
    _getTelemetryFreshness = _lookup1('mg_telemetry_get_freshness');
    _getServoOutputs = _lookup1('mg_servo_get_outputs');
    _getRcChannels = _lookup1('mg_rc_get_channels');
    _getGlobalPosition = _lookup1('mg_vehicle_get_global_position');
    _getHomePosition = _lookup1('mg_vehicle_get_home_position');
    _getVfrHud = _lookup1('mg_vehicle_get_vfr_hud');
    _getAttitude = _lookup1('mg_vehicle_get_attitude');
    _getVehicleSummary = _lookup1('mg_vehicle_get_summary');
    _getExtendedSysState = _lookup1('mg_vehicle_get_extended_sys_state');
    _getDashboardSnapshot = _lookup1('mg_telemetry_get_dashboard_snapshot');
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _getSessionStatus;
  late final Pointer<Utf8> Function(Pointer<Void>) _getFirmwareType;
  late final Pointer<Utf8> Function(Pointer<Void>) _getVehicleFirmware;
  late final Pointer<Utf8> Function(Pointer<Void>) _getFirmwareVersion;
  late final Pointer<Utf8> Function(Pointer<Void>) _getSensorHealth;
  late final Pointer<Utf8> Function(Pointer<Void>) _getTelemetryFreshness;
  late final Pointer<Utf8> Function(Pointer<Void>) _getServoOutputs;
  late final Pointer<Utf8> Function(Pointer<Void>) _getRcChannels;
  late final Pointer<Utf8> Function(Pointer<Void>) _getGlobalPosition;
  late final Pointer<Utf8> Function(Pointer<Void>) _getHomePosition;
  late final Pointer<Utf8> Function(Pointer<Void>) _getVfrHud;
  late final Pointer<Utf8> Function(Pointer<Void>) _getAttitude;
  late final Pointer<Utf8> Function(Pointer<Void>) _getVehicleSummary;
  late final Pointer<Utf8> Function(Pointer<Void>) _getExtendedSysState;
  late final Pointer<Utf8> Function(Pointer<Void>) _getDashboardSnapshot;

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, int) _lookupSession(String symbol) =>
      _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            symbol,
          )
          .asFunction();

  Map<String, Object?> sessionStatus(int sessionId) =>
      _read(_getSessionStatus(_core, sessionId));

  Map<String, Object?> firmwareType() => _read(_getFirmwareType(_core));

  Map<String, Object?> vehicleFirmware() => _read(_getVehicleFirmware(_core));

  Map<String, Object?> firmwareVersion() => _read(_getFirmwareVersion(_core));

  Map<String, Object?> sensorHealth() => _read(_getSensorHealth(_core));

  Map<String, Object?> telemetryFreshness() =>
      _read(_getTelemetryFreshness(_core));

  Map<String, Object?> servoOutputs() => _read(_getServoOutputs(_core));

  Map<String, Object?> rcChannels() => _read(_getRcChannels(_core));

  Map<String, Object?> globalPosition() => _read(_getGlobalPosition(_core));

  Map<String, Object?> homePosition() => _read(_getHomePosition(_core));

  Map<String, Object?> vfrHud() => _read(_getVfrHud(_core));

  /// 姿态角，单位「度」（后端已把 ATTITUDE 的弧度换算好）。
  Map<String, Object?> attitude() => _read(_getAttitude(_core));

  Map<String, Object?> vehicleSummary() => _read(_getVehicleSummary(_core));

  Map<String, Object?> extendedSysState() => _read(_getExtendedSysState(_core));

  Map<String, Object?> dashboardSnapshot() =>
      _read(_getDashboardSnapshot(_core));

  Map<String, Object?> _read(Pointer<Utf8> pointer) => readJsonMap(
    pointer,
    freeString: _freeString,
    invalidResponseMessage: '遥测响应格式无效',
  );
}
