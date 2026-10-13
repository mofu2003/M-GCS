# VehicleSessionAssembler 设计约束

**版本**: 1.0  
**生效**: 2026-10-13
**维护者**: Architecture Team

---

## 目的

`VehicleSessionAssembler` 是 ConnectionController 的一个**专用工厂组件**，职责**明确且有界**。本文档定义其职责范围、强制约束和审查清单，防止其变成"god class"或被随意污染。

## 职责范围（MUST）

`VehicleSessionAssembler` 只能管理以下类型的组件：

### ✅ 允许的

1. **无状态依赖的 FFI 后端**
   - `TelemetryFfiBackend`
   - `ParameterFfiBackend`
   - `CalibrationFfiBackend`
   - `FleetFfiBackend`
   
   **特征**：构造参数只需要 `(DynamicLibrary, Pointer<Void>)`，不依赖 ConnectionController 的任何私有状态。

2. **后续可能加入的基础后端**
   - 新增后端时，**必须满足"无状态依赖"条件**，否则不入装配器

### ❌ 禁止的

1. **有状态依赖的后端**
   ```dart
   // ❌ 不允许进装配器
   FlightCommandFfiBackend(
     ...,
     beforeCommand: () {
       _requireFreshVehicleState();  // ← 依赖 ConnectionController 状态
       fleet.refresh();              // ← 依赖 ConnectionController 私有方法
     }
   )
   ```
   
   这些留在 ConnectionController 中，由主控制器直接管理。

2. **业务逻辑控制器**
   ```dart
   // ❌ 不允许进装配器
   LogController, MissionDocumentController, PayloadController 等
   ```
   
   这些有会话绑定、生命周期回调、UI 通知等复杂逻辑，属于 ConnectionController 职责。

3. **任何涉及 ConnectionController 私有方法/字段的初始化**
   ```dart
   // ❌ 不允许
   beforeAccess: () { if (_disposed) ... }
   isConnected: () => isConnected && sessionId == ownerId
   canChangeFocus: _canSwitchActiveVehicle
   ```

4. **UI 相关逻辑**
   ```dart
   // ❌ 不允许
   notifyListeners()
   addListener()
   任何与 ChangeNotifier 相关的操作
   ```

## 设计原则

### 1. 单一职责（MUST）
装配器的**唯一职责**是：管理"基础 FFI 后端"的工厂创建和生命周期。

**违反判断**：如果装配器的代码涉及以下任何一个，说明职责混乱了
- 状态判断（`if`、`switch` on ConnectionController 状态）
- 业务逻辑（超过"创建对象"的范畴）
- UI 交互（listeners、notifications）
- 会话管理（sessionId、vehicleId 判断）

### 2. 无循环依赖（MUST）
```dart
✅ 正确方向：
ConnectionController → VehicleSessionAssembler → FFI Backends

❌ 禁止：
VehicleSessionAssembler → ConnectionController（回调给主控制器除外）
```

装配器可以接收**回调函数作为参数**，但不能持有 ConnectionController 的引用。

### 3. 测试可注入性（SHOULD）
所有后端都应该支持构造时注入，便于单元测试：
```dart
VehicleSessionAssembler({
  required NativeCore nativeCore,
  // 每个后端都有 override 参数
  TelemetryFfiBackend? telemetryBackendOverride,
  ParameterFfiBackend? parameterBackendOverride,
  ...
})
```

### 4. 明确的生命周期（MUST）
装配器必须提供清晰的生命周期钩子：
```dart
// ✅ 允许
void dispose()
void reset()
void captureSnapshot()  // 未来用于多车切换
void restoreSnapshot()  // 未来用于多车切换
```

## 代码审查检查清单

新 PR 修改装配器时，**必须验证以下所有项**：

- [ ] 新代码只涉及 FFI 后端的创建和生命周期
- [ ] 没有引入对 ConnectionController 私有字段的依赖
- [ ] 没有调用 ConnectionController 的私有方法
- [ ] 没有 UI 相关操作（listeners、notifications）
- [ ] 没有业务逻辑（状态判断、条件分支等）
- [ ] 新的后端满足"无状态依赖"条件
- [ ] 新增后端都提供 override 参数用于测试
- [ ] 更新 `dispose()` 方法以清理新增后端
- [ ] 已更新本文档的"允许的"清单（如适用）

## 违反约束时的处理

如果发现装配器代码违反以上约束：

1. **立即 block merge**
2. **提出 refactor 建议**：
   - 如果是状态依赖，移回 ConnectionController
   - 如果是业务逻辑，分离出独立服务
   - 如果是 UI 逻辑，分离出 ViewModel/Controller

3. **更新本文档**（如果是新的约束发现）

## 与其他组件的关系

### ConnectionController ← 主控制器
- 管理后端的使用方
- 管理有状态的控制器（LogController 等）
- 管理生命周期和 UI 通知

### VehicleSessionAssembler ← 装配器 ← 本文档
- 仅创建和管理"基础 FFI 后端"
- 无业务逻辑、无 UI、无状态判断

### Future: BackendFactory (MultiPlatform)
```dart
// 未来支持 PX4/ArduPilot 多固件
abstract class BackendFactory {
  TelemetryFfiBackend createTelemetryBackend();
  ...
}

class ArduPilotBackendFactory implements BackendFactory { ... }
class Px4BackendFactory implements BackendFactory { ... }

// ConnectionController 选择工厂
final factory = firmware == Firmware.px4 
  ? Px4BackendFactory(nativeCore) 
  : ArduPilotBackendFactory(nativeCore);

final assembler = VehicleSessionAssembler(
  nativeCore: _native!,
  backendFactory: factory,
);
```

这样扩展到多固件时，只需修改工厂选择逻辑，不碰装配器。

## 已知约束（KD-XX）

当前版本只管理 4 个基础后端（Telemetry、Parameter、Calibration、Fleet）。其他后端的迁移受以下因素限制：

- **FlightCommandFfiBackend** — 需要 `beforeCommand` 回调（状态依赖）
- **MissionDocumentFfiBackend** — 需要 `isConnected`、`beforeAccess` 回调（会话依赖）
- **LogFfiBackend** — 需要 native 库可用性判断（条件初始化）
- **PayloadFfiBackend**、**TlogFfiBackend** — 尚未评估

这些后端保持在 ConnectionController 中管理。未来如果要改为装配器管理，需要重新设计它们的初始化模式。

## 更新历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0 | 2026-10-13 | 初始版本：定义装配器职责范围和强制约束 |
