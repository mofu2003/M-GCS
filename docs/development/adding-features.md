# 增添新功能 — 快速指南

**目的**：帮助开发者在增添新功能时做出正确的架构决策，避免破坏既有设计。  
**长度**：~200 行，快速查阅用。

---

## 决策树：新功能应该放哪里

```
📝 新功能来了
   │
   ├─ 是 FFI 后端吗？(与 C++ Core 交互)
   │  ├─ Yes → 需要 ConnectionController 的状态/回调吗？
   │  │  ├─ No (无状态)  → ✅ 加到 VehicleSessionAssembler
   │  │  └─ Yes (有状态) → ✅ 直接在 ConnectionController 中
   │  │
   │  └─ No → 是业务逻辑吗？(应用层)
   │     ├─ Yes → 新建 Controller（如 LogController）
   │     └─ No  → 看场景决定
   │
   └─ 是 UI 界面吗？
      └─ Yes → 新建 UI 文件，用 ConnectionController 的 getter 获取后端/控制器
```

---

## 常见场景

### 1. 新增一个简单的 FFI 后端（如 HealthCheckFfiBackend）

```
问题：需要访问 ConnectionController 的状态吗？
│
├─ No (自洽的数据获取/命令)
│  → 加到 VehicleSessionAssembler
│    ✅ 在 assembler.dart 中添加：
│       - 构造参数：healthCheckBackendOverride
│       - 缓存字段：_healthCheckBackend
│       - getter：get healthCheckBackend
│    ✅ 在 connection_controller.dart 的 _initialize() 中传参
│    ✅ 测试：在 ConnectionController 构造时注入 mock
│
└─ Yes (需要 fleet.refresh()、sessionId 等)
   → 保持在 ConnectionController 中
     ✅ 添加字段、getter、初始化
     ✅ 遵循 FlightCommandFfiBackend 的模式
```

### 2. 新增一个业务逻辑功能（如视频录制）

```
✅ 新建 VideoRecordingController（extends ChangeNotifier）
│  ├─ 依赖：FileFfiBackend、LogController
│  ├─ 生命周期：ConnectionController 管理
│  └─ UI 通知：addListener() → notifyListeners()

✅ 在 ConnectionController 中：
   ├─ 添加字段：late final VideoRecordingController _videoRecording
   ├─ 初始化：_videoRecording = VideoRecordingController(...)
   └─ getter：VideoRecordingController get videoRecording => _videoRecording

✅ 验证：
   ├─ 不访问装配器
   ├─ 依赖注入用于测试
   └─ 生命周期明确
```

### 3. 多固件支持（ArduPilot vs PX4）

```
⚠️  不要在已有的后端中加 if (firmware == PX4) {...}

✅ 未来做法（推迟到 2.0）：
   ├─ 创建 BackendFactory 接口
   ├─ ArduPilotBackendFactory implements BackendFactory
   ├─ Px4BackendFactory implements BackendFactory
   └─ VehicleSessionAssembler 接收 factory 作为参数
      → 从此处切换固件实现，ConnectionController 无感知

✅ 现在的做法（1.0 保持）：
   └─ 如果有固件差异，暂时用 if 语句
      （但记录在 known-deviations.md，标记为"待工厂模式重构"）
```

---

## 架构检查清单（新功能）

加功能前，自问这 5 个问题：

- [ ] **后端放对地方了吗？**
  - 无状态依赖 → VehicleSessionAssembler
  - 有状态 → ConnectionController
  
- [ ] **有测试注入吗？**
  - FFI 后端：构造参数接受 override
  - 控制器：ConnectionController 构造时注入
  
- [ ] **生命周期清晰吗？**
  - 初始化在哪里？谁负责 dispose？
  - 与车辆焦点切换的关系？
  
- [ ] **没有循环依赖吗？**
  - 依赖方向单向：UI → 控制器 → 后端 → Core
  
- [ ] **文档更新了吗？**
  - 新的公共 getter 在 ConnectionController 注释中说明用途
  - 如果破坏了约束，更新 `docs/constraints/known-deviations.md`

---

## 快速修复：常见违规模式

| 症状 | 违规模式 | 修正 |
|------|---------|------|
| 装配器变得很大 | 塞入了业务逻辑/控制器 | 分离出独立 Controller |
| 后端互相依赖 | 后端之间有引用 | 通过 ConnectionController 调度 |
| 测试很难写 | 无法 mock 后端 | 添加 override 参数，支持注入 |
| 多处初始化逻辑 | 后端创建散落各地 | 集中到一个工厂/builder |
| ConnectionController 又变胖了 | 没在早期做好分离 | 定期回顾，拆分新的 Controller |

---

## 长期维护建议

### 每月/每个 Release 的检查

```
□ 新增代码行数 > 200？
  → 看看能否拆分为新的 Controller/backend
  
□ 有新的 if (条件判断) 涉及业务逻辑？
  → 考虑提升为参数/工厂模式
  
□ flutter analyze 警告 > 0？
  → 不要让警告累积，及时修
  
□ 测试失败或被跳过？
  → 每次 skip 都要标注原因和解决期限
  
□ 修改了 ConnectionController？
  → 确认没有违反 vehicle-session-assembler.md 的约束
```

### 债务追踪

维护 `docs/constraints/known-deviations.md`：
- 记录所有暂时的违规（如 if 判断、黑科技）
- 标注原因（为什么现在不能改）
- 标注计划解决时间（e.g., "2.0 工厂模式完成后")

```markdown
### 临时偏差：固件判断逻辑

**位置**：connection_controller.dart:2300  
**描述**：使用 if (firmware == .px4) 而非工厂模式  
**原因**：1.0 版本只支持 ArduPilot，暂时用 if 占位  
**计划解决**：2.0 启动时，实现 BackendFactory 并重构  
**优先级**：High（多固件支持的关键）
```

### 代码审查要点

Pull Request 时必问：

1. **这改动涉及后端吗？** → 验证后端没进 ConnectionController（有状态的除外）
2. **新增了控制器吗？** → 检查它的生命周期和测试注入
3. **改了 ConnectionController 的公开 API？** → 确保向后兼容
4. **有新的条件判断吗？** → 考虑是否需要参数化或工厂模式

---

## 多固件支持的准备（PX4）

### 现在（1.0 ArduPilot only）

```dart
// 直接使用 ArduPilot 的后端
TelemetryFfiBackend get telemetryBackend => ...
```

### 未来（2.0+）

```dart
// 通过工厂模式切换
abstract class BackendFactory {
  TelemetryFfiBackend createTelemetryBackend();
  ParameterFfiBackend createParameterBackend();
  // ...
}

// 装配器依注入接收工厂
final class VehicleSessionAssembler {
  final BackendFactory _factory;
  TelemetryFfiBackend get telemetryBackend =>
    _factory.createTelemetryBackend();
}

// ConnectionController 无需改动
// 只是在初始化装配器时换工厂：
_assembler = VehicleSessionAssembler(
  nativeCore: _native!,
  factory: isPx4 ? Px4BackendFactory() : ArduPilotBackendFactory(),
);
```

**现在要做什么**：
- 如果发现 ArduPilot 和 PX4 的后端实现差异，暂时用 if 语句
- 记录在 `known-deviations.md` 的"多固件差异"部分
- 2.0 规划时统一重构为工厂模式

---

## 参考资源

| 文档 | 用途 |
|------|------|
| `docs/constraints/vehicle-session-assembler.md` | 装配器约束（必读） |
| `docs/adr/0009-vehicle-session-assembler.md` | 为什么这样设计 |
| `REFACTORING_FINAL_REPORT.md` | 重构的历史和决策 |
| `docs/constraints/known-deviations.md` | 当前的技术债 |

---

## 快速决策卡

**新增一个 X，应该放在 VehicleSessionAssembler 吗？**

| 特征 | VehicleSessionAssembler | ConnectionController | 新 Controller |
|------|--------|---------|---------|
| 无状态依赖 | ✅ | ❌ | ❌ |
| 有状态回调 | ❌ | ✅ | ❌ |
| 业务逻辑 | ❌ | ❌ | ✅ |
| 需要 UI 通知 | ❌ | ❌ | ✅ |
| 需要会话缓存 | ❌ | ✅ | ❌ |
| 可单独测试 | ✅ | ⚠️ | ✅ |

---

**最后一条规则**：不确定时，问自己："这个新东西是要创建一个资源（后端）、还是管理一个工作流（控制器）？"
- 创建资源 → 装配器（或 ConnectionController）
- 管理工作流 → 新 Controller
