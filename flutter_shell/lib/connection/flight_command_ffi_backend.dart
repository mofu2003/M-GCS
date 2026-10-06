import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

/// 飞行命令 FFI 适配层。
///
/// 只负责命令 ABI 绑定和 JSON 响应解包；连接门控、飞行模式确认、解锁确认和
/// QGC/MP 组合流程仍由 ConnectionController 负责。
final class FlightCommandFfiBackend {
  FlightCommandFfiBackend(this._library, this._core) {
    _freeString = _library
        .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>('mg_free_string')
        .asFunction();
    _rtl = _lookup1('mg_command_rtl');
    _land = _lookup1('mg_command_land');
    _takeoff = _lookupTakeoff('mg_command_takeoff');
    _arm = _lookupInt('mg_command_arm');
    _disarm = _lookupInt('mg_command_disarm');
    _setMode = _lookupMode('mg_command_set_mode');
    _missionStart = _lookup1('mg_mission_start');
    _guidedPause = _lookup1('mg_command_guided_pause');
    _changeAltitude = _lookupChangeAltitude('mg_command_change_altitude');
    _setHome = _lookupHome('mg_command_set_home');
    _guidedPoint = _lookupGuidedPoint('mg_command_guided_point');
    _orbit = _lookup2('mg_command_orbit');
    _setRoi = _lookupRoi('mg_command_set_roi');
    _changeHeading = _lookupChangeHeading('mg_command_change_heading');
    _setGlobalOrigin = _lookupGlobalOrigin('mg_command_set_global_origin');
  }

  final DynamicLibrary _library;
  final Pointer<Void> _core;

  late final void Function(Pointer<Utf8>) _freeString;
  late final Pointer<Utf8> Function(Pointer<Void>) _rtl;
  late final Pointer<Utf8> Function(Pointer<Void>) _land;
  late final Pointer<Utf8> Function(Pointer<Void>, double) _takeoff;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _arm;
  late final Pointer<Utf8> Function(Pointer<Void>, int) _disarm;
  late final Pointer<Utf8> Function(Pointer<Void>, int, int) _setMode;
  late final Pointer<Utf8> Function(Pointer<Void>) _missionStart;
  late final Pointer<Utf8> Function(Pointer<Void>) _guidedPause;
  late final Pointer<Utf8> Function(Pointer<Void>, double) _changeAltitude;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double, double)
  _setHome;
  late final Pointer<Utf8> Function(
    Pointer<Void>,
    double,
    double,
    double,
    double,
    double,
  )
  _guidedPoint;
  late final Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _orbit;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double, double)
  _setRoi;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double, int)
  _changeHeading;
  late final Pointer<Utf8> Function(Pointer<Void>, double, double, double)
  _setGlobalOrigin;

  Pointer<Utf8> Function(Pointer<Void>) _lookup1(String symbol) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>)>>(symbol)
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>) _lookup2(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>)>
      >(symbol)
      .asFunction();

  /// C 侧是 `float altitude_m`，必须绑 `Float`。
  ///
  /// 用 `Double` 会把 64 位双精度按位塞进 32 位 float 槽位：45.1 会被读成
  /// -1.07e8，ArduPilot 直接判坐标非法（MAV_RESULT_DENIED）；15/30/100 这种
  /// 整数会被读成 0，起飞高度静默失效。float 参数一律用 `Float`。
  Pointer<Utf8> Function(Pointer<Void>, double) _lookupTakeoff(String symbol) =>
      _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Float)>>(
            symbol,
          )
          .asFunction();

  /// C 侧是 `float altitude_change_m`，必须绑 `Float`。
  Pointer<Utf8> Function(Pointer<Void>, double) _lookupChangeAltitude(
    String symbol,
  ) => _library
      .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Float)>>(
        symbol,
      )
      .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, int) _lookupInt(String symbol) =>
      _library
          .lookup<NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32)>>(
            symbol,
          )
          .asFunction();

  Pointer<Utf8> Function(Pointer<Void>, int, int) _lookupMode(String symbol) =>
      _library
          .lookup<
            NativeFunction<Pointer<Utf8> Function(Pointer<Void>, Int32, Int32)>
          >(symbol)
          .asFunction();

  /// C 侧是 `(double lat, double lon, float alt_m)`：只有高度是 float。
  Pointer<Utf8> Function(Pointer<Void>, double, double, double) _lookupHome(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Double, Double, Float)
        >
      >(symbol)
      .asFunction();

  /// C 侧是 `(double lat, double lon, float alt, float yaw, float speed)`：
  /// 经纬度是 double，高度/航向/速度都是 float。
  Pointer<Utf8> Function(Pointer<Void>, double, double, double, double, double)
  _lookupGuidedPoint(String symbol) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(
            Pointer<Void>,
            Double,
            Double,
            Float,
            Float,
            Float,
          )
        >
      >(symbol)
      .asFunction();

  Map<String, Object?> rtl() => _read(_rtl(_core));

  Map<String, Object?> land() => _read(_land(_core));

  Map<String, Object?> takeoff(double altitudeMeters) =>
      _read(_takeoff(_core, altitudeMeters));

  Map<String, Object?> arm(int force) => _read(_arm(_core, force));

  Map<String, Object?> disarm(int force) => _read(_disarm(_core, force));

  Map<String, Object?> setMode(int baseMode, int customMode) =>
      _read(_setMode(_core, baseMode, customMode));

  Map<String, Object?> missionStart() => _read(_missionStart(_core));

  Map<String, Object?> guidedPause() => _read(_guidedPause(_core));

  /// `altitudeChangeMeters` 为正表示上升（QGC 同）。
  Map<String, Object?> changeAltitude(double altitudeChangeMeters) =>
      _read(_changeAltitude(_core, altitudeChangeMeters));

  Map<String, Object?> setHome(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _read(_setHome(_core, latitude, longitude, altitudeMeters));

  Map<String, Object?> guidedPoint(
    double latitude,
    double longitude,
    double altitudeMeters,
    double yawDegrees,
    double speedMetersPerSecond,
  ) => _read(
    _guidedPoint(
      _core,
      latitude,
      longitude,
      altitudeMeters,
      yawDegrees,
      speedMetersPerSecond,
    ),
  );

  Pointer<Utf8> Function(Pointer<Void>, double, double, double) _lookupRoi(
    String symbol,
  ) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Double, Double, Float)
        >
      >(symbol)
      .asFunction();

  /// C 侧 `(float heading_deg, float yaw_rate_dps, int direction)`。
  Pointer<Utf8> Function(Pointer<Void>, double, double, int)
  _lookupChangeHeading(String symbol) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Float, Float, Int32)
        >
      >(symbol)
      .asFunction();

  /// C 侧 `(double lat, double lon, float alt_m)`。
  Pointer<Utf8> Function(Pointer<Void>, double, double, double)
  _lookupGlobalOrigin(String symbol) => _library
      .lookup<
        NativeFunction<
          Pointer<Utf8> Function(Pointer<Void>, Double, Double, Float)
        >
      >(symbol)
      .asFunction();

  Map<String, Object?> setRoi(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _read(_setRoi(_core, latitude, longitude, altitudeMeters));

  Map<String, Object?> changeHeading(
    double headingDegrees,
    double yawRateDegreesPerSecond,
    int direction,
  ) => _read(
    _changeHeading(_core, headingDegrees, yawRateDegreesPerSecond, direction),
  );

  Map<String, Object?> setGlobalOrigin(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _read(_setGlobalOrigin(_core, latitude, longitude, altitudeMeters));

  Map<String, Object?> orbit({
    required double latitude,
    required double longitude,
    required double radiusMeters,
    required double velocityMetersPerSecond,
    required double altitudeMeters,
  }) {
    final config = jsonEncode({
      'lat': latitude,
      'lon': longitude,
      'radius': radiusMeters,
      'velocity': velocityMetersPerSecond,
      'altitude': altitudeMeters,
    }).toNativeUtf8();
    try {
      return _read(_orbit(_core, config));
    } finally {
      malloc.free(config);
    }
  }

  Map<String, Object?> _read(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      final raw = jsonDecode(pointer.toDartString());
      if (raw is! Map) throw const FormatException('飞行命令响应格式无效');
      return Map<String, Object?>.from(raw);
    } finally {
      _freeString(pointer);
    }
  }
}
