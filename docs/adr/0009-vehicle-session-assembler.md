# ADR-0009: VehicleSessionAssembler 用于多固件适配

> 历史记录：本文记录特定时间点的架构决策。当前行为以代码、测试和根目录 `AGENTS.md` 为准。

**状态**: Accepted  
**日期**: 2026-10-13
**作者**: Architecture Team

## 问题

ConnectionController 即将支持多固件（PX4 + ArduPilot）。目前的问题：

1. 后端初始化逻辑散落在 ConnectionController 的 20+ 个 getter 中
2. 每个固件可能需要不同的后端实现（e.g., Px4TelemetryBackend vs ArduPilotTelemetryBackend）
3. 无法在工厂层面切换固件，必须在 ConnectionController 中加条件判断
4. 单元测试时无法独立测试"后端组装逻辑"，必须构造完整的 ConnectionController

## 方案选项

### A. 全量迁移（所有 10+ 后端/控制器都进装配器）
- ✅ 最彻底的分离
- ❌ 装配器会变成另一个"god class"
- ❌ 需要处理有状态依赖的后端，引入复杂的回调机制
- ❌ 代码行数反而增加（闭包 + 委托）

### B. 只迁移"纯"后端（无状态依赖的）
- ✅ 职责清晰、易维护
- ✅ 风险低、改动集中
- ✅ 为多固件做好基础
- ✅ 不会变成屎山
- △ 不能彻底清理 ConnectionController

## 决策

**选择方案 B**。

装配器管理以下"无状态依赖"的后端：
- TelemetryFfiBackend
- ParameterFfiBackend
- CalibrationFfiBackend
- FleetFfiBackend

其他后端（FlightCommandFfiBackend、MissionDocumentFfiBackend 等）保持在 ConnectionController 中，因为它们需要：
- 捕获 ConnectionController 的私有状态（_disposed、sessionId）
- 访问 ConnectionController 的私有方法（_requireFreshVehicleState）
- 会话绑定逻辑（一对多的会话缓存）

## 实施

### Phase 1: 创建装配器框架
- 创建 VehicleSessionAssembler
- 支持构造时注入（用于测试）
- 4 个基础后端都迁移进来

### Phase 2: 多固件支持（未来）
```dart
// 工厂模式
abstract class BackendFactory {
  TelemetryFfiBackend createTelemetryBackend();
  ParameterFfiBackend createParameterBackend();
  CalibrationFfiBackend createCalibrationBackend();
  FleetFfiBackend createFleetBackend();
}

class ArduPilotBackendFactory implements BackendFactory { ... }
class Px4BackendFactory implements BackendFactory { ... }

// ConnectionController 中
final factory = firmware == Firmware.px4 
  ? Px4BackendFactory(nativeCore)
  : ArduPilotBackendFactory(nativeCore);
final assembler = VehicleSessionAssembler(backendFactory: factory);
```

这样多固件支持只需新增 Factory 类，不改 ConnectionController 和 VehicleSessionAssembler。

## 约束

装配器管理以下"无状态依赖"的后端，职责边界明确。

## 收益

1. **后端初始化逻辑集中** — 所有基础后端的创建在一个地方
2. **为多固件做基础** — 未来可通过工厂模式快速支持 PX4
3. **测试友好** — 可独立测试装配器，注入 mock NativeCore
4. **代码组织清晰** — "工厂职责"从业务逻辑中分离出来

## 成本

1. **多一层抽象** — ConnectionController 需要理解装配器的存在
2. **半成品状态** — 短期内代码是混合的（一部分后端在装配器，一部分在主控制器）

## 风险缓解

- ✅ 建立强约束文档，明确职责边界
- ✅ 代码审查检查清单，防止职责混乱
- ✅ 单元测试验证，确保逻辑正确
- ✅ ADR 记录，防止后人误用

## 参考

- 相关代码审查: PR#XXX（待合并）
