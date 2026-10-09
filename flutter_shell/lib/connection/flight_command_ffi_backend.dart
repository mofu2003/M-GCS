import 'dart:convert';
import 'dart:async';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

/// 飞行命令 FFI 适配层。
///
/// 所有命令统一经过非阻塞的 submit/poll FFI；native 侧使用显式 vehicle_id 和线程局部
/// 目标覆盖，不修改 UI active focus。现有旧命令 ABI 仍保留兼容。
final class FlightCommandFfiBackend {
  FlightCommandFfiBackend(
    DynamicLibrary library,
    this._core, {
    this.beforeCommand,
    this.targetVehicleId,
    this.executeForVehicleOverride,
    this.pollForTicketOverride,
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
      _poll = pollForTicketOverride;
    } else {
      final submit = library
          .lookupFunction<
            Pointer<Utf8> Function(Pointer<Void>, Int32, Pointer<Utf8>),
            Pointer<Utf8> Function(Pointer<Void>, int, Pointer<Utf8>)
          >('mg_command_submit_for_vehicle');
      final poll = library
          .lookupFunction<
            Pointer<Utf8> Function(Pointer<Void>, Uint64),
            Pointer<Utf8> Function(Pointer<Void>, int)
          >('mg_command_poll');
      _executeForVehicle = (vehicleId, commandJson) =>
          submit(_core, vehicleId, commandJson);
      _poll = (ticket) => poll(_core, ticket);
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
  final Pointer<Utf8> Function(int)? pollForTicketOverride;

  late final void Function(Pointer<Utf8>) _nativeFreeString;
  void Function(Pointer<Utf8>)? _freeStringOverride;
  late final Pointer<Utf8> Function(int, Pointer<Utf8>) _executeForVehicle;
  Pointer<Utf8> Function(int)? _poll;
  bool _disposed = false;

  void dispose() => _disposed = true;

  Future<Map<String, Object?>> startMission({bool armIfNeeded = true}) =>
      _dispatch('start_mission', {'arm_if_needed': armIfNeeded ? 1 : 0});

  Future<Map<String, Object?>> rtl() => _dispatch('rtl');

  Future<Map<String, Object?>> land() => _dispatch('land');

  Future<Map<String, Object?>> takeoff(double altitudeMeters) =>
      _dispatch('takeoff', {'altitude_m': altitudeMeters});

  Future<Map<String, Object?>> arm(int force) =>
      _dispatch('arm', {'force': force});

  Future<Map<String, Object?>> disarm(int force) =>
      _dispatch('disarm', {'force': force});

  Future<Map<String, Object?>> setMode(int baseMode, int customMode) =>
      _dispatch('set_mode', {'base_mode': baseMode, 'custom_mode': customMode});

  Future<Map<String, Object?>> missionStart() => _dispatch('mission_start');

  Future<Map<String, Object?>> guidedPause() => _dispatch('guided_pause');

  /// `altitudeChangeMeters` 为正表示上升（QGC 同）。
  Future<Map<String, Object?>> changeAltitude(double altitudeChangeMeters) =>
      _dispatch('change_altitude', {'altitude_change_m': altitudeChangeMeters});

  Future<Map<String, Object?>> setHome(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _dispatch('set_home', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
  });

  Future<Map<String, Object?>> guidedPoint(
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

  Future<Map<String, Object?>> setRoi(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _dispatch('set_roi', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
  });

  Future<Map<String, Object?>> changeHeading(
    double headingDegrees,
    double yawRateDegreesPerSecond,
    int direction,
  ) => _dispatch('change_heading', {
    'heading_deg': headingDegrees,
    'yaw_rate_dps': yawRateDegreesPerSecond,
    'direction': direction,
  });

  Future<Map<String, Object?>> setGlobalOrigin(
    double latitude,
    double longitude,
    double altitudeMeters,
  ) => _dispatch('set_global_origin', {
    'lat': latitude,
    'lon': longitude,
    'alt': altitudeMeters,
  });

  Future<Map<String, Object?>> orbit({
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

  /// 显式目标的分派入口，专用批量动作。这里刻意绕过 [beforeCommand] 和
  /// [targetVehicleId]：批量指令不能依赖 active focus，也不需要先切换飞机。
  Future<Map<String, Object?>> executeRawForVehicle(
    int vehicleId,
    String command, [
    Map<String, Object?> args = const {},
  ]) async {
    if (vehicleId <= 0) {
      throw StateError('批量指令目标飞机无效');
    }
    if (command.isEmpty) throw StateError('批量指令名称为空');
    return _invoke(vehicleId, command, args);
  }

  Future<Map<String, Object?>> _dispatch(
    String command, [
    Map<String, Object?> args = const {},
  ]) async {
    beforeCommand?.call();
    final vehicleId = targetVehicleId?.call();
    if (vehicleId == null || vehicleId <= 0) {
      throw StateError('当前没有明确的目标飞机，拒绝发送指令');
    }
    return _invoke(vehicleId, command, args);
  }

  Future<Map<String, Object?>> _invoke(
    int vehicleId,
    String command,
    Map<String, Object?> args,
  ) async {
    if (_disposed) throw StateError('飞行命令后端已关闭');
    final commandJson = jsonEncode({'command': command, 'args': args});
    final native = commandJson.toNativeUtf8();
    final Map<String, Object?> submission;
    try {
      submission = _read(_executeForVehicle(vehicleId, native));
    } finally {
      malloc.free(native);
    }
    // Test seam retains the direct result. Production only calls nonblocking
    // submit/poll; no synchronous ACK wait runs on Flutter's UI isolate.
    if (_poll == null || submission['success'] != true) return submission;
    final ticket = (submission['ticket'] as num?)?.toInt();
    if (ticket == null || ticket <= 0) throw const FormatException('无效指令凭证');
    final elapsed = Stopwatch()..start();
    while (elapsed.elapsed < const Duration(seconds: 45)) {
      if (_disposed) throw StateError('飞行命令后端已关闭');
      final status = _read(_poll!(ticket));
      if (status['success'] != true) return status;
      if (status['state'] == 'complete') {
        final result = status['result'];
        if (result is! Map) throw const FormatException('无效指令结果');
        return Map<String, Object?>.from(result);
      }
      await Future<void>.delayed(const Duration(milliseconds: 25));
    }
    throw TimeoutException('等待飞行指令结果超时');
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
