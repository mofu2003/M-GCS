import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

/// 飞行命令 FFI 适配层。
///
/// 所有命令统一经过 [mg_command_execute_for_vehicle] 的显式 vehicle_id 分派；
/// native 侧使用线程局部目标覆盖，不修改 UI active focus。现有旧命令 ABI 仍保留
/// 兼容，但 Flutter 不再依赖它。
final class FlightCommandFfiBackend {
  FlightCommandFfiBackend(
    DynamicLibrary library,
    this._core, {
    this.beforeCommand,
    this.targetVehicleId,
    this.executeForVehicleOverride,
    this.freeStringOverride,
  }) {
    if (freeStringOverride != null) {
      _freeStringOverride = freeStringOverride;
    } else {
      _nativeFreeString = library
          .lookup<NativeFunction<Void Function(Pointer<Utf8>)>>(
            'mg_free_string',
          )
          .asFunction();
    }
    if (executeForVehicleOverride != null) {
      _executeForVehicle = executeForVehicleOverride!;
    } else {
      final execute = library
          .lookupFunction<
            Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>),
            Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
          >('mg_command_execute_for_vehicle');
      _executeForVehicle = (vehicleId, commandJson) =>
          execute(_core, vehicleId, commandJson);
    }
  }

  final Pointer<Void> _core;

  /// Runs before native invocation, not after response decoding.
  final void Function()? beforeCommand;

  /// Returns the session ID selected for the next command.
  final int? Function()? targetVehicleId;

  /// Test seam; production uses the native symbol directly.
  final Pointer<Utf8> Function(int, Pointer<Utf8>)? executeForVehicleOverride;
  final void Function(Pointer<Utf8>)? freeStringOverride;

  late final void Function(Pointer<Utf8>) _nativeFreeString;
  void Function(Pointer<Utf8>)? _freeStringOverride;
  late final Pointer<Utf8> Function(int, Pointer<Utf8>) _executeForVehicle;

  Map<String, Object?> rtl() => _dispatch('rtl');

  Map<String, Object?> land() => _dispatch('land');

  Map<String, Object?> takeoff(double altitudeMeters) =>
      _dispatch('takeoff', {'altitude_m': altitudeMeters});

  Map<String, Object?> arm(int force) => _dispatch('arm', {'force': force});

  Map<String, Object?> disarm(int force) =>
      _dispatch('disarm', {'force': force});

  Map<String, Object?> setMode(int baseMode, int customMode) =>
      _dispatch('set_mode', {'base_mode': baseMode, 'custom_mode': customMode});

  Map<String, Object?> missionStart() => _dispatch('mission_start');

  Map<String, Object?> guidedPause() => _dispatch('guided_pause');

  /// `altitudeChangeMeters` 为正表示上升（QGC 同）。
  Map<String, Object?> changeAltitude(double altitudeChangeMeters) =>
      _dispatch('change_altitude', {'altitude_change_m': altitudeChangeMeters});

  Map<String, Object?> setHome(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _dispatch('set_home', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
  });

  Map<String, Object?> guidedPoint(
    double latitude,
    double longitude,
    double altitudeMeters,
    double yawDegrees,
    double speedMetersPerSecond,
  ) => _dispatch('guided_point', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
    'yaw': yawDegrees,
    'speed': speedMetersPerSecond,
  });

  Map<String, Object?> setRoi(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _dispatch('set_roi', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
  });

  Map<String, Object?> changeHeading(
    double headingDegrees,
    double yawRateDegreesPerSecond,
    int direction,
  ) => _dispatch('change_heading', {
    'heading_deg': headingDegrees,
    'yaw_rate_dps': yawRateDegreesPerSecond,
    'direction': direction,
  });

  Map<String, Object?> setGlobalOrigin(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _dispatch('set_global_origin', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
  });

  Map<String, Object?> orbit({
    required double latitude,
    required double longitude,
    required double radiusMeters,
    required double velocityMetersPerSecond,
    required double altitudeMeters,
  }) => _dispatch('orbit', {
    'lat': latitude,
    'lon': longitude,
    'radius': radiusMeters,
    'velocity': velocityMetersPerSecond,
    'altitude': altitudeMeters,
  });

  Map<String, Object?> _dispatch(
    String command, [
    Map<String, Object?> args = const {},
  ]) {
    beforeCommand?.call();
    final vehicleId = targetVehicleId?.call();
    if (vehicleId == null || vehicleId <= 0) {
      throw StateError('当前没有明确的目标飞机，拒绝发送指令');
    }
    final commandJson = jsonEncode({'command': command, 'args': args});
    final native = commandJson.toNativeUtf8();
    try {
      return _read(_executeForVehicle(vehicleId, native));
    } finally {
      malloc.free(native);
    }
  }

  Map<String, Object?> _read(Pointer<Utf8> pointer) {
    if (pointer == nullptr) throw StateError('后端返回空结果');
    try {
      final raw = jsonDecode(pointer.toDartString());
      if (raw is! Map) throw const FormatException('飞行命令响应格式无效');
      return Map<String, Object?>.from(raw);
    } finally {
      (_freeStringOverride ?? _nativeFreeString)(pointer);
    }
  }
}
