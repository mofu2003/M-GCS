# 开发检查清单 — 新功能 & 长期维护

**版本**：1.0.0  
**最后更新**：2026-10-13
**用途**：每次新功能、Release、代码审查时参照

---

## 🚀 增添新功能的 5 步流程

### Step 1：识别类型（2 分钟）

```
你要增添的东西是什么？
├─ FFI 后端（与 C++ Core 交互）
│  └─ 需要 ConnectionController 状态吗？
│     ├─ 不需要 → ✅ 加到 VehicleSessionAssembler
│     └─ 需要   → ✅ 直接在 ConnectionController
│
├─ 业务逻辑工作流（应用层）
│  └─ ✅ 新建 Controller（参考 LogController）
│
└─ UI / 工具类
   └─ ✅ 按场景创建，通常不涉及架构
```

**参考**：`docs/QUICK_REFERENCE.md` 的决策树

### Step 2：设计架构（10 分钟）

选择的方案对应的设计：

**如果选 VehicleSessionAssembler**：
```dart
// vehicle_session_assembler.dart 添加：
final XxxFfiBackend? _xxxBackendOverride;
final XxxFfiBackend? _xxxBackend;  // lazy-init 缓存

XxxFfiBackend get xxxBackend => 
  _xxxBackend ??= _xxxBackendOverride ??
  XxxFfiBackend(_nativeCore.library, _nativeCore.handle);

// dispose() 中添加：
_xxxBackend = null;
```

**如果选 ConnectionController**：
```dart
// 直接添加字段、初始化、getter
late final XxxFfiBackend _xxxBackend;

void _initialize() {
  _xxxBackend = XxxFfiBackend(
    _requireNative().library,
    _requireNative().handle,
    // 如有状态回调
    beforeAction: () { _requireFreshVehicleState(); },
  );
}

XxxFfiBackend get xxxBackend => _xxxBackend;
```

**如果选新 Controller**：
```dart
class XxxController extends ChangeNotifier {
  XxxController(this._backend, { this._backendOverride });
  
  final XxxBackend _backend;
  final XxxBackend? _backendOverride;  // 测试注入
  
  XxxBackend get backend => _backendOverride ?? _backend;
  
  void dispose() { /* 清理资源 */ }
}
```

**参考**：`docs/development/adding-features.md` 的常见场景

### Step 3：添加测试注入（5 分钟）

所有新增的后端/控制器都要支持测试注入：

```dart
// 后端：构造参数接受 override
ConnectionController({
  this._xxxBackendOverride,
  // ...
});

// 测试时：
final mockBackend = MockXxxBackend();
controller = ConnectionController(
  xxxBackendOverride: mockBackend,
);
```

**参考**：`docs/development/adding-features.md` 中的"快速修复"

### Step 4：验证架构（5 分钟）

自问这 5 个问题：

- [ ] 后端放对地方了吗？
- [ ] 有测试注入吗？
- [ ] 生命周期清晰吗？
- [ ] 没有循环依赖吗？
- [ ] 文档更新了吗？

**参考**：`docs/development/adding-features.md` 的检查清单

### Step 5：测试与提交（10 分钟）

```bash
flutter analyze              # 0 issues？
flutter test                 # 全通过？
git diff                     # 有新的问题注释吗？
git commit -m "..."
```

---

## 📅 长期维护的时间表

### 每周

```
□ 新增代码有没有硬编码的 if (firmware == .px4)？
  → 有的话记录在 known-deviations.md
□ flutter analyze 有没有新警告？
  → 不要让警告累积
□ 有没有新的 // TODO？
  → 每个 TODO 都要标注期限
```

### 每个小功能完成时

```
□ 新增的公开 API 有文档注释吗？
□ 多文件改动有关键概念注释吗？
□ 新的分支都有测试吗？
□ 涉及 connection_controller.dart 吗？
  → 检查是否违反 vehicle-session-assembler.md 约束
```

### 每个 Release（2-4 周）

```
□ flutter test 全量通过吗？
□ flutter analyze 0 issues 吗？
□ known-deviations.md 有未解决的项吗？
□ 版本号和 CHANGELOG 一致吗？
□ ConnectionController 有变胖吗？（> 4500 行警告）
```

### 每季度回顾

```
□ ConnectionController 行数趋势如何？
□ 有新的 Controller 应该拆分出来吗？
□ 是否遇到新的架构问题？
□ 多固件支持（2.0）的计划有新进展吗？
```

**参考**：`docs/development/maintenance.md` 的完整检查清单

---

## 🚨 关键指标与红线

### 关键数字

| 指标 | 当前 | 警告 | 危险 |
|------|------|------|------|
| ConnectionController 行数 | 3767 | 4500 | 5000 |
| VehicleSessionAssembler 行数 | 88 | 150 | 200 |
| flutter analyze 警告 | 0 | 1+ | - |
| flutter test 通过率 | 258/262 | < 95% | - |

### 红线（不能越过）

🔴 **代码层面**：
- 装配器中不能有业务逻辑判断
- FFI 后端之间不能有依赖
- ConnectionController 不能直接依赖 UI widgets
- 不能在 core/ 的持锁路径中做阻塞 I/O

🔴 **流程层面**：
- flutter analyze 有新警告不能提交
- 新功能没有测试不能合并
- 涉及 connection_controller.dart 的改动没有通过约束检查不能合并

---

## 📚 文档快速导航

### 第一次遇到问题

👉 `docs/QUICK_REFERENCE.md` - 30 秒快速答案

### 要增添新功能

👉 `docs/development/adding-features.md` - 决策树 + 5 个场景

### 做代码审查

👉 `docs/development/maintenance.md` 的"代码审查建议"

### 了解整个设计

👉 `docs/adr/0009-vehicle-session-assembler.md` - 为什么这样做

### 了解约束

👉 `docs/constraints/vehicle-session-assembler.md` - 职责范围 + 代码审查清单

### 查看技术债

👉 `docs/constraints/known-deviations.md` - 临时偏差记录

---

## 🎯 常见场景速查

### 场景 1：增加一个简单的无状态后端（如 HealthCheckFfiBackend）

```
✅ 流程：
  1. 在 vehicle_session_assembler.dart 中添加：
     - override 参数
     - 缓存字段
     - getter（lazy-init 模式）
  2. 在 connection_controller.dart 的 _initialize() 中传参
  3. 添加 getter：get healthCheckBackend => _assembler.healthCheckBackend
  4. 测试时注入 mock
  
📖 参考：docs/development/adding-features.md 的"常见场景 1"
```

### 场景 2：增加一个有状态的后端（需要访问 fleet、sessionId 等）

```
✅ 流程：
  1. 直接在 ConnectionController 中添加字段
  2. 在 _initialize() 中创建，传入 beforeCommand/callback 等
  3. 添加 getter（可选择 lazy-init）
  4. 不需要加到装配器

📖 参考：docs/development/adding-features.md 的"常见场景 2"
   以及 FlightCommandFfiBackend 的实现
```

### 场景 3：增加一个新的业务逻辑（如视频录制）

```
✅ 流程：
  1. 新建 VideoRecordingController (extends ChangeNotifier)
  2. 在 ConnectionController 中添加字段和 getter
  3. 在 _initialize() 中创建
  4. 依赖注入 override 参数用于测试

📖 参考：docs/development/adding-features.md 的"常见场景 3"
   以及 MissionDocumentController 的实现
```

### 场景 4：两个后端互相依赖

```
❌ 不要直接依赖
✅ 做法：
  - A 后端在需要 B 的结果时调用 callback
  - ConnectionController 捕获回调 → 调用 B → 传结果给 A
  
📖 参考：docs/development/adding-features.md 的"快速修复：后端互相依赖"
```

### 场景 5：多固件支持（PX4 vs ArduPilot）

```
1.0（现在）做法：
  □ 用 if (firmware == .px4) { ... }
  □ 记录在 known-deviations.md
  □ 标注"2.0 用工厂模式重构"

2.0（规划）做法：
  □ 创建 BackendFactory 接口
  □ ArduPilotBackendFactory / Px4BackendFactory
  □ VehicleSessionAssembler 接收工厂
  □ ConnectionController 完全不感知固件差异

📖 参考：docs/development/adding-features.md 的"多固件支持"
```

---

## 📋 代码审查的关键问题

### 审查新功能时必问

```
□ 这个功能属于哪一层（FFI 后端/业务逻辑/UI）？
□ 按决策树，这个东西应该放在哪里？
□ 是否有测试注入支持？
□ 是否涉及 connection_controller.dart？
  → 如果是，是否违反了 vehicle-session-assembler.md？
□ 是否有循环依赖风险？
□ 代码是否有注释？（关键概念要说明）
□ 测试通过吗？
```

### 审查 bug 修复时

```
□ 这个 bug 的根本原因是什么？
□ 是架构问题还是实现问题？
□ 这个修复会不会隐藏更深层的问题？
□ 需要记录在 known-deviations.md 吗？
```

### 审查重构时

```
□ 改动后代码行数减少了吗？
□ 测试仍然全通过吗？
□ 是否改善了可测试性？
□ 是否有新的技术债产生？
□ 是否与 ADR-0009 的初心一致？
```

---

## ✅ 完整的开发流程（从想法到合并）

```
1. 有想法
   ↓
2. 查 QUICK_REFERENCE.md 的决策树
   ↓
3. 读对应的 adding-features.md 场景
   ↓
4. 设计（5 分钟）+ 编码 + 测试
   ↓
5. 自检 5 个架构问题
   ↓
6. flutter analyze / flutter test
   ↓
7. 提交：git commit -m "功能简述"
   ↓
8. 提 PR
   ↓
9. Reviewer 按"代码审查关键问题"检查
   ↓
10. 合并
    ↓
11. 月末 Release 时，检查关键指标
    ↓
12. 完成！
```

---

## 🔄 如何保持项目健康

### 每个 PR 的承诺

```
你认同以下承诺，PR 才能合并：

1. 代码符合架构约束（vehicle-session-assembler.md）
2. flutter analyze 0 issues
3. flutter test 全通过
4. 新功能有测试
5. 如有架构偏差，记录在 known-deviations.md
6. 不引入循环依赖
7. 没有访问 ConnectionController 私有方法（除非必要）
```

### 长期维护的承诺

```
1. 每季度回顾一次技术债
2. ConnectionController < 5000 行时拆分
3. 定期确认多固件（2.0）的计划
4. 新人入门时用本清单指导
5. 架构有变化时更新文档
```

---

## 📞 遇到问题怎么办

| 问题 | 第一步 | 第二步 |
|------|--------|--------|
| 不知道新功能放哪里 | 看 QUICK_REFERENCE 决策树 | 问维护者 |
| 测试很难写 | 检查是否缺少 override 参数 | 考虑是否需要拆分 |
| flutter analyze 报警 | 查是否有循环依赖 | 修复或添加 ignore 注释 |
| 代码审查反馈多 | 从 QUICK_REFERENCE 开始自检 | 按检查清单改 |
| 不确定架构方案 | 看 adding-features 的相似场景 | 询问 Review 者 |

---

**记住**：好的架构是为了让代码更容易维护，而不是限制。有问题就改文档，不要绕过约束。
