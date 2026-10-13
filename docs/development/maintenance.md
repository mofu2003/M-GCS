# 长期维护指南

**面向**：项目维护者、新加入的开发者  
**频率**：新功能前阅读、每个 Release 前回顾  
**目标**：保持架构清晰，避免衰化

---

## 一句话原则

> **无状态用装配器，有状态用控制器，业务逻辑用新 Controller。**

---

## 日常维护清单（每周）

- [ ] `flutter analyze` 无警告
- [ ] 新增代码没有 `if (firmware == .px4)` 的硬编码
- [ ] 新增后端/控制器都有测试注入支持
- [ ] 代码审查时问：这个东西放对地方了吗？

---

## 按时间段的检查

### 每次 commit 前

```bash
flutter analyze          # 0 issues？
flutter test             # 全通过？
git diff                 # 有新的 // TODO 注释吗？(应该关键词大写)
```

### 每个小功能（1-2 天）完成时

```
□ 新增的公开 API 有文档注释吗？
□ 涉及多个文件的改动有关键概念注释吗？
□ 测试覆盖新的分支吗？
```

### 每个 Release（2-4 周）

```
□ 运行全量测试：flutter test
□ 检查 known-deviations.md 有未解决的项吗？
□ 有可以预防的技术债吗？（如重复代码）
□ 版本号和 CHANGELOG 同步了吗？
□ 是否需要更新多固件计划（针对 2.0）？
```

### 每个季度回顾

```
□ ConnectionController 行数是否又开始增长？
□ 有新的 Controller 应该拆分出来吗？
□ 是否遇到了新的架构问题，需要更新约束文档？
□ 对多固件支持（工厂模式）有新的设计洞察吗？
```

---

## 常见衰化迹象

### 迹象 1：ConnectionController 又变胖了

```
当前：3767 行
临界点：4500 行（超过则开始衰化）

检查：
□ 是否有新的大型工作流（> 200 行）应该拆分？
  → 创建新 Controller（如 MissionPlanning 已做）
□ 是否有新的后端初始化逻辑？
  → 考虑加到 VehicleSessionAssembler（如无状态）
□ 是否代码有重复？
  → 抽取为私有方法或独立类
```

### 迹象 2：新后端互相依赖

```
症状：创建后端 A 时需要后端 B 的结果

危险：
  A 依赖 B → B 依赖 C → C 依赖 A（循环）
  后端之间应该无感知

正确做法：
  通过 ConnectionController 调度
  ✅ A.do() → 回调 ConnectionController
     → ConnectionController.useBForA()
     → B.do() → 结果传给 A
```

### 迹象 3：测试变得很难写

```
症状：
  新增一个功能，却要 mock 5 个其他东西
  ConnectionController 无法单独测试

原因：
  可能违反了单一职责原则
  或者缺少依赖注入

修正：
  □ 抽取为独立 Controller
  □ 添加 override 参数支持
  □ 与 ConnectionController 的耦合拆松
```

### 迹象 4：出现了 "黑科技"

```
危险信号：
  // TODO: 这是个 workaround，未来需要重构
  if (sessionId == _cachedSessionId && isConnected) { ... }
  
  原因很可能是架构问题，而非简单的 bug

处理：
  1. 记录在 known-deviations.md
  2. 写明"为什么现在这样"和"什么时候解决"
  3. 后续 Review 时提醒
  4. 定期（每季度）重新评估优先级
```

---

## 关键指标监控

### 代码行数

```
Main modules:
  connection_controller.dart
    ├─ 当前：3767 行
    ├─ 警告：> 4500 行（开始衰化）
    └─ 危险：> 5000 行（必须拆分）

  vehicle_session_assembler.dart
    ├─ 当前：88 行
    ├─ 目标：保持 < 150 行
    └─ 如果 > 200：违反了无状态原则
```

### 测试覆盖

```
当前：258/262（99%）

目标：> 95%（允许 skip 的特殊场景）

如果新增功能导致覆盖率下降：
  □ 是否有测试无法 mock？
  □ 是否该功能不值得测？
  □ 是否需要添加集成测试？
```

### 静态分析

```
目标：0 issues

允许例外：
  - 必要的 ignore 注释（标注原因）
  - 针对未来版本的 deprecated（标注版本）
  
不允许：
  - ignore 但没有理由
  - 累积的警告（> 3 个）
```

---

## 架构债清单（known-deviations.md）

这个文件是项目健康度的晴雨表。

**每个偏差的记录格式**：

```markdown
### [分类] 短描述

**位置**：文件路径和行号  
**问题**：发生了什么不符合架构  
**原因**：为什么现在无法修正（时间/依赖/设计困境）  
**计划解决**：什么时候、怎么解决  
**优先级**：High / Medium / Low  
**链接**：相关 Issue / ADR / 讨论
```

**定期清理**：
- 每季度回顾一遍
- 如果已解决，删除
- 如果优先级变化，更新
- 如果无解，升级为设计决策（写入 ADR）

---

## 多固件支持的长期规划

### 1.0（当前）
- 仅 ArduPilot
- 如有 PX4 差异，用 if 语句 + known-deviations.md 记录

### 2.0（计划）
- 实现 BackendFactory 接口
- 创建 ArduPilotBackendFactory / Px4BackendFactory
- VehicleSessionAssembler 接收工厂，无需改动 ConnectionController

### 现在要做的

```
□ 定期审查 if (firmware == ...) 的出现次数
□ 积累多固件的设计经验
□ 规划 2.0 的工厂模式界面
□ 不要提前过度设计（1.0 保持简洁）
```

---

## 代码审查的建议问题

### For New Features

- [ ] 这个功能属于哪一层（FFI 后端/业务逻辑/UI）？
- [ ] 能否单独测试？
- [ ] 是否有测试注入支持？
- [ ] 涉及多个文件，有足够的注释吗？
- [ ] 是否有循环依赖的风险？

### For Bug Fixes

- [ ] 为什么会有这个 bug？架构问题还是实现问题？
- [ ] 这个修正会不会隐藏更深层的问题？
- [ ] 是否需要记录在 known-deviations.md？

### For Refactoring

- [ ] 改动后代码行数减少了吗？
- [ ] 测试仍然全通过吗？
- [ ] 是否改善了可测试性？
- [ ] 是否有新的技术债产生？

---

## 快速故障排查

| 问题 | 可能原因 | 检查点 |
|------|---------|--------|
| 新功能难以测试 | 没做依赖注入 | 构造器有 override 参数吗？ |
| 修改后 flutter analyze 报警 | 可能引入了循环依赖 | 导入顺序对吗？ |
| 两个后端同时被初始化 | 可能都在装配器里 | 检查是否应该拆分 |
| ConnectionController 又变大了 | 新功能没有拆分出 Controller | 是否有 > 200 行的工作流？ |
| 多车切换时数据混乱 | 会话缓存逻辑有问题 | 检查 sessionId 的判断 |

---

## 文档同步

每当架构发生变化时，更新：

```
优先级 1（必须同步）：
  □ docs/constraints/vehicle-session-assembler.md
    → 如有新的约束发现
  □ docs/constraints/known-deviations.md
    → 如有新的技术债

优先级 2（应该同步）：
  □ docs/adr/000X-*.md
    → 如有重要架构决策
  □ docs/development/adding-features.md
    → 如有新的常见场景

优先级 3（可选）：
  □ REFACTORING_FINAL_REPORT.md
    → 仅在重大改组时更新
```

---

## 新人入门清单

新开发者加入时：

1. **了解架构**（1 小时）
   - 读 `REFACTORING_FINAL_REPORT.md` 的摘要
   - 读 `docs/adr/0009-vehicle-session-assembler.md`

2. **理解约束**（30 分钟）
   - 读 `docs/constraints/vehicle-session-assembler.md`
   - 看决策树（本文档）

3. **实操**（2-4 小时）
   - 在 adding-features.md 找一个相似的场景
   - 跟着步骤添加一个小功能
   - 提交 PR，让老手 review

4. **持续学习**
   - 每次修改前问"这是无状态还是有状态"
   - 每次 review 时看别人怎么决策
   - 6 个月后可以独立做架构判断

---

## 一页纸维护看板

```
项目健康度检查表

[ ] flutter analyze: 0 issues (最后检查时间: ___)
[ ] flutter test: 100% pass (最后运行: ___)
[ ] known-deviations.md 更新了 (最后更新: ___)
[ ] ConnectionController < 4500 行 (当前: 3767)
[ ] VehicleSessionAssembler < 150 行 (当前: 88)
[ ] 没有 TODO 注释超过 1 周 (搜索结果: __)
[ ] 近 1 月没有新增 if (firmware == ...) (新增: __)
[ ] 最新 Release 的 CHANGELOG 完整 (版本: 1.0.0)

如果某项为 ❌，立即处理或排队到下个迭代。
```

---

**记住**：好的架构是**立法者的工具**，而非限制。当约束无法适应新需求时，不是绕过约束，而是更新约束。

所有架构偏差都要记录。不记录的代码债，才是最危险的。
