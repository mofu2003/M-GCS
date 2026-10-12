/// 车辆会话装配器：管理后端与控制器的生命周期。
///
/// 从 ConnectionController 抽取装配逻辑，避免主控制器变成一个"万能工厂"。
/// 职责：
/// - 持有所有后端（Backend）和控制器（Controller）实例
/// - 提供统一的 lazy-init 和工厂方法
/// - 隔离不同子系统的初始化复杂度
library;

import 'native_core.dart';
import 'telemetry_ffi_backend.dart';
import 'parameter_ffi_backend.dart';
import 'calibration_ffi_backend.dart';
import 'flight_command_ffi_backend.dart';
import '../fleet/fleet_ffi_backend.dart';

/// 车辆会话装配器
final class VehicleSessionAssembler {
  VehicleSessionAssembler({
    required this._nativeCore,
    // 用于测试注入
    this._telemetryBackendOverride,
    this._parameterBackendOverride,
    this._calibrationBackendOverride,
    this._flightCommandBackendOverride,
    this._fleetBackendOverride,
  });

  final NativeCore _nativeCore;

  // 测试注入
  final TelemetryFfiBackend? _telemetryBackendOverride;
  final ParameterFfiBackend? _parameterBackendOverride;
  final CalibrationFfiBackend? _calibrationBackendOverride;
  final FlightCommandFfiBackend? _flightCommandBackendOverride;
  final FleetFfiBackend? _fleetBackendOverride;

  // Lazy-init 缓存
  TelemetryFfiBackend? _telemetryBackend;
  ParameterFfiBackend? _parameterBackend;
  CalibrationFfiBackend? _calibrationBackend;
  FlightCommandFfiBackend? _flightCommandBackend;
  FleetFfiBackend? _fleetBackend;

  // ================================================================
  // 后端 Getter（lazy-init 或注入）
  // ================================================================

  TelemetryFfiBackend get telemetryBackend => _telemetryBackend ??=
      _telemetryBackendOverride ??
      TelemetryFfiBackend(_nativeCore.library, _nativeCore.handle);

  ParameterFfiBackend get parameterBackend => _parameterBackend ??=
      _parameterBackendOverride ??
      ParameterFfiBackend(_nativeCore.library, _nativeCore.handle);

  CalibrationFfiBackend get calibrationBackend => _calibrationBackend ??=
      _calibrationBackendOverride ??
      CalibrationFfiBackend(_nativeCore.library, _nativeCore.handle);

  FlightCommandFfiBackend get flightCommandBackend =>
      _flightCommandBackend ??= _flightCommandBackendOverride ??
      FlightCommandFfiBackend(
        _nativeCore.library,
        _nativeCore.handle,
      );

  FleetFfiBackend get fleetBackend =>
      _fleetBackend ??= _fleetBackendOverride ??
      FleetFfiBackend(
        _nativeCore.library,
        _nativeCore.handle,
      );

  /// 清理所有资源
  void dispose() {
    _telemetryBackend = null;
    _parameterBackend = null;
    _calibrationBackend = null;
    _flightCommandBackend = null;
    _fleetBackend = null;
  }
}
