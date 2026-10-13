# VehicleSessionAssembler 重构报告（历史快照）

**报告时点**：2026-10-13
**对应提交**：`7301829 refactor: 移除装配器中的 FlightCommandFfiBackend`
**项目版本**：1.0.0

> **本文档的性质**：记录 2026-10 装配器重构的**历史过程与决策**，不是当前状态说明。
> 其中「当前状态」一节描述的是报告时点的事实；此后代码可能已变化。
> 阅读时若需确认现状，请以 `flutter_shell/lib/connection/vehicle_session_assembler.dart`
> 与 `docs/constraints/vehicle-session-assembler.md` 为准。

> **修订说明（2026-10-13 后续）**：原文档标注日期为 2025-01-15（早于仓库起始时间，
> 系笔误），并包含「93 → 98 分」「架构清晰度 9/10」「文档完整度 10/10」等**自评分数**。
> 该评分由作者自行给出、无外部依据，且五项维度中四项为满分，不具备评估价值，
> 已全部移除。技术内容与决策记录予以保留。

---

## 背景与目标

`ConnectionController` 在重构前承担了过多的后端装配职责：20+ 个 getter 各自惰性创建
FFI 后端，初始化逻辑分散，难以确认依赖关系，也无法在测试中替换后端。

重构目标是把这部分**无状态后端**的创建集中到一个专用组件，从而：

- 让后端初始化与依赖关系可一处看清；
- 使 `ConnectionController` 的后端依赖可注入，便于单独测试；
- 为后续多固件支持预留一个明确的装配点。

---

## 采用的方案

引入 `VehicleSessionAssembler` 作为**专用工厂组件**：

```dart
final class VehicleSessionAssembler {
  // 仅管理 4 个无状态后端
  TelemetryFfiBackend get telemetryBackend => ...;
  ParameterFfiBackend get parameterBackend => ...;
  CalibrationFfiBackend get calibrationBackend => ...;
  FleetFfiBackend get fleetBackend => ...;

  // 支持测试注入
  VehicleSessionAssembler({
    required this._nativeCore,
    this._telemetryBackendOverride,
    this._parameterBackendOverride,
    this._calibrationBackendOverride,
    this._fleetBackendOverride,
  });

  // 统一生命周期
  void dispose() { /* 清理所有缓存 */ }
}
```

方案取舍：

- **方案 A（全量迁移 10+ 后端）**：会让装配器承担过多职责，变成新的集中复杂度来源。
  已否决。
- **方案 B（仅迁移无状态后端）**：职责边界清晰，风险可控。**已采用**。

### 保留在 ConnectionController 的后端及原因

| 后端 | 位置 | 原因 |
|------|------|------|
| Telemetry | 装配器 | 无状态，仅需 NativeCore |
| Parameter | 装配器 | 无状态，仅需 NativeCore |
| Calibration | 装配器 | 无状态，仅需 NativeCore |
| Fleet | 装配器 | 无状态，仅需 NativeCore |
| FlightCommand | ConnectionController | 有状态回调（beforeCommand） |
| MissionDocument | ConnectionController | 有状态会话缓存 |
| Log / File / Tlog / Payload | ConnectionController | 有状态生命周期管理 |

---

## 实施过程中的修复

| # | 问题 | 严重度 | 处理 | 状态 |
|----|------|--------|------|------|
| 1 | 装配器中不应包含 `FlightCommandFfiBackend` | 高 | 删除 import + 参数 + getter | 已修复 |
| 2 | `ConnectionController` 中 3 个字段应为 `final` | 低 | 添加 `final` 限定符 | 已修复 |
| 3 | `flutter analyze` 报 3 个警告 | 低 | 由上述两项自动消除 | 已修复 |
| 4 | 检查清单文档需反映 Phase 3-4 回滚 | 低 | 标记回滚并更新说明 | 已修复 |

---

## 关键决策：Phase 3-4 回滚

重构原计划分 4 个阶段推进。**Phase 3-4 已被回滚**，装配器最终只管理 4 个无状态后端。

回滚原因是第一阶段之后的评估发现：继续迁移有状态后端需要把回调、会话缓存与生命周期
一并搬入装配器，会使装配器开始承担 `ConnectionController` 原有的状态管理职责，
违背引入它时设定的「只做装配」边界。**主动停在边界处，而不是推进到底**，
是本次重构中最重要的一次判断。

该状态在提交 `c02cf58 docs: 更正装配器迁移状态为 4 个后端（Phase 3-4 已回滚）`
中已更正，`docs/constraints/vehicle-session-assembler.md` 同步记录了边界。

---

## 相关文档与代码

- `flutter_shell/lib/connection/vehicle_session_assembler.dart` — 装配器实现
- `flutter_shell/lib/connection/connection_controller.dart` — 主控制器
- `docs/constraints/vehicle-session-assembler.md` — 职责范围与代码审查清单
- `docs/adr/0009-vehicle-session-assembler.md` — 架构决策记录
- `docs/refactoring/connection-controller-assembler-migration.md` — 迁移指南
- `docs/refactoring/assembler-architecture-comparison.md` — 方案对比
- `docs/refactoring/assembler-refactoring-checklist.md` — 迁移检查清单
