/// 车辆会话装配器：管理基础 FFI 后端的生命周期。
///
/// **职责范围**（see docs/constraints/vehicle-session-assembler.md）
/// - 仅管理"无状态依赖的基础 FFI 后端"（TelemetryFfiBackend、ParameterFfiBackend 等）
/// - 提供 lazy-init 和测试注入能力
/// - 统一后端的生命周期管理（dispose）
///
/// **禁止做的**
/// - ❌ 不管理有状态依赖的后端（FlightCommandFfiBackend 等）
/// - ❌ 不管理业务逻辑控制器（LogController、MissionDocumentController 等）
/// - ❌ 不实现业务逻辑、状态判断、UI 通知
/// - ❌ 不调用 ConnectionController 的私有方法
///
/// **设计动机**
/// 为多固件支持（PX4 + ArduPilot）做准备。未来可通过工厂模式在此处切换固件实现，
/// 而不需改动 ConnectionController。
library;

import 'native_core.dart';
import 'telemetry_ffi_backend.dart';
import 'parameter_ffi_backend.dart';
import 'calibration_ffi_backend.dart';
import '../fleet/fleet_ffi_backend.dart';

/// 测试注入：只在测试时使用
final class BackendOverrides {
  const BackendOverrides({
    this.telemetry,
    this.parameter,
    this.calibration,
    this.fleet,
  });

  final TelemetryFfiBackend? telemetry;
  final ParameterFfiBackend? parameter;
  final CalibrationFfiBackend? calibration;
  final FleetFfiBackend? fleet;
}

/// 车辆会话装配器
///
/// 仅创建和管理"无状态依赖的基础 FFI 后端"。
///
/// 添加新后端前，必须：
/// 1. 检查是否满足"无状态依赖"条件（只需 NativeCore，不依赖 ConnectionController 状态）
/// 2. 如果有状态依赖，保持在 ConnectionController 中
/// 3. 参考 docs/constraints/vehicle-session-assembler.md
final class VehicleSessionAssembler {
  VehicleSessionAssembler({
    required NativeCore nativeCore,
    BackendOverrides? overrides,
  })  : _nativeCore = nativeCore,
        _overrides = overrides ?? const BackendOverrides();

  final NativeCore _nativeCore;
  final BackendOverrides _overrides;

  // Lazy-init 缓存
  TelemetryFfiBackend? _telemetryBackend;
  ParameterFfiBackend? _parameterBackend;
  CalibrationFfiBackend? _calibrationBackend;
  FleetFfiBackend? _fleetBackend;

  // ================================================================
  // 后端 Getter（lazy-init 或注入）
  // ================================================================

  TelemetryFfiBackend get telemetryBackend =>
      _telemetryBackend ??= _overrides.telemetry ??
          TelemetryFfiBackend(_nativeCore.library, _nativeCore.handle);

  ParameterFfiBackend get parameterBackend =>
      _parameterBackend ??= _overrides.parameter ??
          ParameterFfiBackend(_nativeCore.library, _nativeCore.handle);

  CalibrationFfiBackend get calibrationBackend =>
      _calibrationBackend ??= _overrides.calibration ??
          CalibrationFfiBackend(_nativeCore.library, _nativeCore.handle);

  FleetFfiBackend get fleetBackend =>
      _fleetBackend ??= _overrides.fleet ??
          FleetFfiBackend(
            _nativeCore.library,
            _nativeCore.handle,
          );

  /// 清理所有资源
  void dispose() {
    _telemetryBackend = null;
    _parameterBackend = null;
    _calibrationBackend = null;
    _fleetBackend = null;
  }
}
