# 项目文档总览

**项目版本**：1.0.0  
**最后更新**：2026-10-13

---

## 📚 核心文档体系（阅读优先级）

### 🚀 开始使用（新人必读）

| 文档 | 长度 | 用途 | 阅读时间 |
|------|------|------|---------|
| **[DEVELOPMENT_CHECKLIST.md](DEVELOPMENT_CHECKLIST.md)** | 400 行 | 完整的开发流程，从想法到合并 | 15 min |
| **[docs/QUICK_REFERENCE.md](docs/QUICK_REFERENCE.md)** | 150 行 | 30 秒快速查阅（决策树、FAQ、红线） | 5 min |
| **[docs/development/adding-features.md](docs/development/adding-features.md)** | 200 行 | 新功能怎么加不破坏架构 | 10 min |

### 🏗️ 架构与约束（代码审查必读）

| 文档 | 长度 | 用途 | 深度 |
|------|------|------|------|
| **[docs/constraints/vehicle-session-assembler.md](docs/constraints/vehicle-session-assembler.md)** | 190 行 | 装配器约束 + 代码审查清单 | ★★★ |
| **[docs/adr/0009-vehicle-session-assembler.md](docs/adr/0009-vehicle-session-assembler.md)** | 108 行 | 为什么这样设计（问题→方案→决策） | ★★☆ |

### 📖 长期维护（Release & 技术债管理）

| 文档 | 长度 | 用途 | 频率 |
|------|------|------|------|
| **[docs/development/maintenance.md](docs/development/maintenance.md)** | 300 行 | 长期维护计划、衰化迹象、监控指标 | 每 Release |
| **[docs/constraints/known-deviations.md](docs/constraints/known-deviations.md)** | ? | 当前的技术债记录 | 持续更新 |

### 📋 重构历史与总结

| 文档 | 长度 | 用途 | 何时读 |
|------|------|------|--------|
| **[2026-10-13-assembler-refactoring-report.md](docs/reports/2026-10-13-assembler-refactoring-report.md)** | 见左栏路径 | 装配器重构的过程与决策记录 | 了解背景 |
| **[docs/refactoring/connection-controller-assembler-migration.md](docs/refactoring/connection-controller-assembler-migration.md)** | 600 行 | 迁移指南（已完成的阶段 1-2） | 参考实现 |

### 🛠️ 开发环境与测试

| 文档 | 位置 | 用途 |
|------|------|------|
| **开发指南** | [docs/development.md](docs/development.md) | 构建、测试、门禁 |
| **发布流程** | docs/constraints/release-and-hardware.md | 版本发布 |
| **测试约束** | docs/constraints/testing.md | 编写测试 |

---

## 🎯 场景导航（我想...）

### 我要增添一个新功能

```
1️⃣  阅读 QUICK_REFERENCE.md 的决策树 (5 min)
   → 确定功能是 FFI 后端、业务逻辑还是 UI

2️⃣  查看 docs/development/adding-features.md 的对应场景 (10 min)
   → 学习该类功能的标准做法

3️⃣  参照 DEVELOPMENT_CHECKLIST.md 的 5 步流程开发 (20 min)
   → 设计 → 编码 → 测试 → 自检

4️⃣  提交前检查 docs/constraints/vehicle-session-assembler.md (5 min)
   → 确保没违反约束
```

**总时间**：40 分钟（从想法到可提交的代码）

---

### 我要审查一个 PR

```
1️⃣  用 QUICK_REFERENCE.md 快速判断代码位置对不对 (2 min)

2️⃣  按 docs/development/maintenance.md 的代码审查清单检查 (10 min)
   ├─ 新功能必问的 5 个问题
   ├─ Bug 修复必问的 3 个问题
   └─ 重构必问的 4 个问题

3️⃣  查看 docs/constraints/vehicle-session-assembler.md 的红线 (3 min)
   → 是否违反了不可逾越的约束

4️⃣  看 DEVELOPMENT_CHECKLIST.md 的"完整开发流程" (2 min)
   → 检查是否按流程走
```

**总时间**：15-20 分钟

---

### 我要做 Release / 季度回顾

```
1️⃣  阅读 docs/development/maintenance.md 的对应时间表

2️⃣  按检查清单逐项确认：
   ├─ flutter test 全通过？
   ├─ flutter analyze 0 issues？
   ├─ 版本号同步了吗？
   └─ 技术债有新增吗？

3️⃣  查看 docs/constraints/known-deviations.md
   → 是否有该关闭的技术债

4️⃣  更新 CHANGELOG 和版本号
```

**总时间**：30 分钟

---

### 我想了解整个架构设计

```
✓ 快速版（20 分钟）：
  1. QUICK_REFERENCE.md - 一页纸总结
  2. docs/adr/0009-vehicle-session-assembler.md - 为什么这样

✓ 完整版（60 分钟）：
  1. docs/reports/2026-10-13-assembler-refactoring-report.md - 背景与历史
  2. docs/adr/0009-* - 决策过程
  3. docs/constraints/vehicle-session-assembler.md - 约束详解
  4. docs/development/adding-features.md - 实践指南
```

---

## 📊 文档统计

### 按类型

| 类型 | 文档数 | 总行数 | 用途 |
|------|--------|--------|------|
| 快速查阅 | 2 | ~200 | 决策树、FAQ、红线 |
| 详细指南 | 3 | ~900 | 开发、维护、检查清单 |
| 架构文档 | 3 | ~400 | ADR、约束、重构报告 |
| 参考资料 | 4 | ~1200 | 迁移、测试、发布、已知债 |
| **总计** | **12+** | **~2700** | |

### 阅读路径

```
新人入门：10 min (QUICK_REFERENCE + DEVELOPMENT_CHECKLIST)
   ↓
第一个功能：25 min (adding-features + 实践)
   ↓
成为 Reviewer：45 min (全部约束文档 + maintenance)
   ↓
深入理解：120 min (完整架构史话)
```

---

## 🔐 约束体系

### Hard Rules（必须遵守，违反即 block merge）

```
✅ 必须满足
- 装配器只管 4 个无状态后端 + lazy-init + 测试注入
- FFI 后端之间不能有依赖
- ConnectionController < 5000 行
- flutter analyze 0 issues
- flutter test 全通过

❌ 不允许
- 装配器中有业务逻辑判断
- 后端互相依赖
- ConnectionController 直接依赖 UI widgets
- 在 core/ 的持锁路径中做阻塞 I/O
```

### Soft Rules（应该遵守，但可讨论）

```
⚠️  优先级高
- 无状态后端优先放装配器
- 代码行数 > 4500 要考虑拆分
- 新的 if (条件) 要记录原因
- 每个 TODO 都要标注期限

⚠️  优先级中等
- 代码应该有清晰的注释（关键概念）
- 新功能应该有测试
- 多文件改动要有关键注释
```

---

## 📅 维护日历

### 周维护（每周一次检查）

```
□ flutter analyze 无新警告
□ 没有硬编码 if (firmware == .px4)
□ 每个 TODO 都要标注期限
```

### Release 维护（每 2-4 周）

```
□ 全量测试通过
□ 版本号和 CHANGELOG 同步
□ known-deviations.md 已清理
□ 技术债预算有没有超支
```

### 季度维护（每 3 个月）

```
□ ConnectionController 行数趋势如何
□ 是否有新的衰化迹象
□ 多固件支持（2.0）有新进展吗
□ 文档是否需要更新
```

---

## 🗂️ 文件导航

### 根目录

```
/
├── DEVELOPMENT_CHECKLIST.md          👈 开发检查清单（必读）
├── QUICK_REFERENCE.md                👈 速查表（必读）
├── docs/reports/2026-10-13-assembler-refactoring-report.md       📖 重构报告
├── DOCUMENTATION_OVERVIEW.md          📖 本文件（文档总览）
└── AGENTS.md                          📖 协作规则
```

### docs/development/

```
docs/development/
├── development.md                     🛠️  构建、测试、门禁
├── adding-features.md                 👈 新功能指南
└── maintenance.md                     👈 长期维护
```

### docs/constraints/

```
docs/constraints/
├── vehicle-session-assembler.md       👈 装配器约束
├── known-deviations.md                📋 技术债清单
├── architecture.md                    🏗️  整体依赖方向
├── safety.md                          ⚠️  飞行安全约束
├── testing.md                         🧪 测试约束
├── release-and-hardware.md            📦 发布流程
└── ui.md                              🎨 UI 约束
```

### docs/adr/

```
docs/adr/
├── 0009-vehicle-session-assembler.md  👈 装配器 ADR
└── ...其他决策记录
```

---

## 💡 关键概念速查

| 概念 | 定义 | 查看位置 |
|------|------|---------|
| **VehicleSessionAssembler** | 无状态 FFI 后端的工厂和生命周期管理 | docs/constraints/... |
| **无状态后端** | 仅需 NativeCore，不依赖 ConnectionController 状态 | docs/development/adding-features.md |
| **有状态后端** | 需要 ConnectionController 的状态、回调或私有方法 | docs/development/adding-features.md |
| **业务逻辑 Controller** | 工作流和会话管理（如 LogController、MissionDocumentController） | docs/development/adding-features.md |
| **工厂模式** | 多固件支持时切换不同固件的后端实现 | docs/development/adding-features.md 的多固件部分 |
| **技术债** | 暂时的代码折扣，记录在 known-deviations.md | docs/constraints/known-deviations.md |

---

## ✅ 文档完整性检查

### 新功能开发

- ✅ 决策树（QUICK_REFERENCE）
- ✅ 5 个常见场景（adding-features）
- ✅ 5 步流程（DEVELOPMENT_CHECKLIST）
- ✅ 架构约束（vehicle-session-assembler）
- ✅ 代码示例（各文档中都有）

### 代码审查

- ✅ 代码审查清单（maintenance）
- ✅ 红线清单（QUICK_REFERENCE）
- ✅ 关键问题（DEVELOPMENT_CHECKLIST）
- ✅ 常见违规模式（adding-features）

### 长期维护

- ✅ 时间表（maintenance）
- ✅ 指标监控（maintenance）
- ✅ 衰化迹象（maintenance）
- ✅ 技术债管理（known-deviations）
- ✅ 多固件规划（adding-features）

### 新人入门

- ✅ 快速导航（本文件）
- ✅ 5 分钟快速开始（QUICK_REFERENCE）
- ✅ 完整流程（DEVELOPMENT_CHECKLIST）
- ✅ 入门清单（maintenance）

---

## 🎓 推荐学习路径

### 第 1 天（新人）

```
09:00 - 09:15  阅读 QUICK_REFERENCE.md
09:15 - 09:30  看 DEVELOPMENT_CHECKLIST.md 的决策树
09:30 - 10:00  读第一个 adding-features 的场景
10:00 - 11:00  添加一个简单功能（Hands-on）
11:00 - 11:30  自检、测试、提交
```

### 第 1 周（新开发者）

```
Day 1  上述完整流程
Day 2  审查别人的 PR（用 maintenance.md 的清单）
Day 3  读 vehicle-session-assembler.md 的约束
Day 4  读 adr/0009 理解设计初心
Day 5  添加第二个功能，参照 adding-features 的不同场景
```

### 第 1 个月（成为 Reviewer）

```
Week 1 完成上述学习
Week 2 独立审查 5+ 个 PR
Week 3 做 Release 维护
Week 4 深度理解完整重构史（docs/reports/2026-10-13-assembler-refactoring-report.md）
```

---

## 📞 快速帮助

| 问题 | 查看 |
|------|------|
| 新功能应该放哪里？ | QUICK_REFERENCE.md 决策树 |
| 代码有没有违反约束？ | vehicle-session-assembler.md 代码审查清单 |
| 怎么写测试？ | adding-features.md 的测试注入部分 |
| 项目怎么维护？ | maintenance.md 的时间表 |
| 为什么这样设计？ | adr/0009-vehicle-session-assembler.md |
| 有什么技术债？ | known-deviations.md |
| 我卡住了 | 看 DEVELOPMENT_CHECKLIST.md 的"故障排查" |

---

**最后提示**：这套文档体系的目的是**让长期维护变容易**，而不是限制。有问题就改文档，不要绕过约束。

版本 1.0.0 · 2026-10-13
