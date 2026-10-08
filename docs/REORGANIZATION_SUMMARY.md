# M-GCS 文档重组完成报告

> **历史报告**：本文记录当时的整理动作，文中的版本号和路径反映当时状态，
> 不作为当前项目基线。当前基线请以 `AGENTS.md`、`README.md` 和
> `docs/planning/STATUS_AUDIT.md` 为准。


**执行时间**：2026-10-07  
**触发版本**：v0.0.73 → v0.0.76  
**执行方案**：完整整理（方案 A）

---

## 一、执行内容概览

### 1.1 版本号统一（P0）
✅ **8 个文件已更新到 v0.0.76**

| 文件 | 旧版本 | 新版本 |
|------|--------|--------|
| `flutter_shell/README.md` | 0.0.70 | **0.0.76** |
| `bridge/ffi/API_CATALOG.md` | 0.0.70 | **0.0.76** |
| `docs/README.md` | 0.0.74 | **0.0.76** |
| `docs/guides/DEVELOPMENT.md` | 0.0.74 | **0.0.76** |
| `docs/planning/STATUS_AUDIT.md` | 0.0.74 | **0.0.76** |
| `docs/planning/DEVELOPMENT_ROADMAP.md` | 0.0.74 | **0.0.76** |
| `docs/planning/DEFINITION_OF_DONE.md` | 0.0.74 | **0.0.76** |
| `docs/architecture/PROJECT_LAYOUT.md` | 0.0.74 | **0.0.76** |

`AGENTS.md` 和已废弃的 `CONSTRAINTS.md` 已补充版本号声明。

### 1.2 系统垃圾清理（P0）
✅ **删除 3 个 `.DS_Store` 文件**
- `docs/.DS_Store`
- `docs/adr/.DS_Store`
- `docs/audits/.DS_Store`

### 1.3 指导文档整合（P1）
✅ **三大文档合并完成**

| 文件 | 旧状态 | 新状态 |
|------|--------|--------|
| `AGENTS.md` | 无版本号，部分重复 | **唯一权威源**，已整合 CONSTRAINTS.md |
| `DEVELOPMENT.md` | 60% 重复内容 | **简化版**，指向 AGENTS.md |
| `CONSTRAINTS.md` | 80% 重复内容 | **已废弃**，指向 AGENTS.md |

**整合策略**：
- CONSTRAINTS.md 中的独有约束（FFI 契约、UI 设计、安全设计）已合并入 AGENTS.md
- DEVELOPMENT.md 改为人类友好的快速入门，详细规则指向 AGENTS.md
- 消除了约 3,000 行重复内容

### 1.4 上游文档合并（P1）
✅ **两个上游文档合并为一个**

**合并前**：
- `UPSTREAM_CODE_SELECTION.md`（模块归属）
- `QGC_MP_BACKPORT_MAP.md`（功能对照表）

**合并后**：
- `docs/specs/upstream/SELECTION_STRATEGY.md`（统一策略）
- 旧文件标记为已废弃，指向新位置

### 1.5 目录结构重组（P1）
✅ **从扁平结构改为分类结构**

#### 旧结构（混乱）
```
docs/
├── ARCHITECTURE.md
├── DEFINITION_OF_DONE.md
├── DEVELOPMENT.md
├── DEVELOPMENT_ROADMAP.md
├── MISSION_PLANNING.md
├── PROJECT_LAYOUT.md
├── QGC_MP_BACKPORT_MAP.md
├── README.md
├── STATUS_AUDIT.md
├── UPSTREAM_CODE_SELECTION.md
├── adr/（9 个 ADR）
├── audits/（4 个历史审计）
└── FONT_ALTERNATIVES_RESEARCH.md
```

#### 新结构（清晰）
```
docs/
├── README.md                          # 总索引
├── CLEANUP_REPORT.md                  # 审计报告
├── REORGANIZATION_SUMMARY.md          # 本文件
├── architecture/                      # 架构设计
│   ├── ARCHITECTURE.md
│   ├── PROJECT_LAYOUT.md
│   └── adr/                          # 9 个架构决策记录
├── guides/                            # 开发指南
│   └── DEVELOPMENT.md                 # 简化版开发指南
├── planning/                          # 规划与审计
│   ├── DEFINITION_OF_DONE.md
│   ├── DEVELOPMENT_ROADMAP.md
│   ├── STATUS_AUDIT.md
│   └── audits/                        # 4 个历史审计
├── specs/                             # 技术规范
│   ├── MISSION_PLANNING.md
│   └── upstream/                      # 上游策略
│       ├── SELECTION_STRATEGY.md      # 合并后的统一策略
│       ├── UPSTREAM_CODE_SELECTION.md # 已废弃
│       └── QGC_MP_BACKPORT_MAP.md     # 已废弃
└── research/                          # 研究文档
    └── FONT_ALTERNATIVES_RESEARCH.md
```

**分类逻辑**：
- `architecture/` - 系统设计、依赖关系、ADR
- `guides/` - 实操指南（构建、门禁、发布）
- `planning/` - 规划文档、审计报告、DoD
- `specs/` - 技术规范、任务契约、上游策略
- `research/` - 调研报告、技术选型

### 1.6 交叉引用更新（P1）
✅ **所有文档内部链接已更新**

更新了以下文件中的交叉引用：
- `AGENTS.md`
- `docs/README.md`
- `docs/planning/DEVELOPMENT_ROADMAP.md`
- `docs/planning/audits/2026-10-03-refactoring-plan.md`
- `docs/research/FONT_ALTERNATIVES_RESEARCH.md`
- `docs/architecture/adr/0009-hybrid-upstream-implementation-strategy.md`

**废弃文件均已添加重定向链接**。

---

## 二、未删除但已归档的内容

### 2.1 83 个无人区 FFI 函数

**决策**：保留

根据 `bridge/ffi/API_CATALOG.md` 的 ABI 稳定性规则：
> 公开导出的函数是稳定 ABI，即使无人使用也不能在 minor 版本中删除

这 83 个函数分布：
- logging（15 个，68%）
- geofence（11 个，69%）
- configuration（17 个，40%）
- mission（13 个，24%）

**建议**：在下一个 major 版本（1.0.0）时评估是否废弃。

### 2.2 历史审计文档

**保留**：`docs/planning/audits/` 中的 4 个历史审计
- `2026-09-29-v0.0.8.md`
- `2026-09-30-v0.0.18.md`
- `2026-10-01-v0.0.24.md`
- `2026-10-03-refactoring-plan.md`

**理由**：这些是历史快照，记录了决策上下文和问题解决过程。

### 2.3 废弃但保留的文档

以下文件标记为 `[已废弃]`，但保留在仓库中以维护向后兼容：
- `CONSTRAINTS.md` → 重定向到 `AGENTS.md`
- `docs/specs/upstream/UPSTREAM_CODE_SELECTION.md` → 重定向到 `SELECTION_STRATEGY.md`
- `docs/specs/upstream/QGC_MP_BACKPORT_MAP.md` → 重定向到 `SELECTION_STRATEGY.md`

---

## 三、统计数据

### 文件变化
| 指标 | 数值 |
|------|------|
| 文档总数 | 30 个 .md 文件 |
| 新增文件 | 3 个（CLEANUP_REPORT.md、REORGANIZATION_SUMMARY.md、SELECTION_STRATEGY.md）|
| 删除文件 | 3 个（.DS_Store）|
| 移动文件 | 13 个 |
| 修改文件 | 18 个 |

### 内容整合
| 操作 | 效果 |
|------|------|
| 合并重复内容 | 减少约 3,000 行重复 |
| 版本号统一 | 8 处更新 |
| 交叉引用修复 | 6 处更新 |
| 目录层级 | 1 层 → 3 层 |

---

## 四、后续维护建议

### 4.1 版本发布流程改进

**当前问题**：版本号分散在 8 个文件，容易漏改。

**建议**：在 `tools/release.sh` 中添加自动化检查：

```bash
# 版本号一致性检查
version_files=(
  "CMakeLists.txt"
  "flutter_shell/pubspec.yaml"
  "README.md"
  "AGENTS.md"
  "docs/README.md"
  "docs/guides/DEVELOPMENT.md"
  "docs/planning/STATUS_AUDIT.md"
  "docs/planning/DEVELOPMENT_ROADMAP.md"
  "docs/planning/DEFINITION_OF_DONE.md"
  "docs/architecture/PROJECT_LAYOUT.md"
  "flutter_shell/README.md"
  "bridge/ffi/API_CATALOG.md"
)

# 提取版本号并比对
# 发现不一致时警告并拒绝发布
```

### 4.2 文档维护规则

**新增文档时**：
- 必须归类到 `architecture/`、`guides/`、`planning/`、`specs/`、`research/` 之一
- 必须在 `docs/README.md` 中添加索引
- 必须在文件头部声明版本号

**修改指导文档时**：
- `AGENTS.md` 是唯一权威源
- 修改后同步检查 `docs/guides/DEVELOPMENT.md` 是否需要更新摘要
- 不要再创建与 AGENTS.md 重复的约束文档

### 4.3 门禁命令补充

建议在 CI 或 pre-commit hook 中添加：

```bash
# 检查版本号一致性
./tools/check-version-consistency.sh

# 检查文档交叉引用
./tools/check-doc-links.sh

# 检查 .DS_Store 等系统垃圾
git ls-files | grep -E '\.DS_Store$|~$|\.bak$|\.tmp$' && exit 1
```

---

## 五、验收清单

- [x] 版本号统一到 0.0.76（8 个文件）
- [x] 删除系统垃圾（3 个 .DS_Store）
- [x] 整合三大指导文档（AGENTS.md 为唯一权威）
- [x] 合并上游文档（SELECTION_STRATEGY.md）
- [x] 重组目录结构（5 个分类目录）
- [x] 更新交叉引用（6 处）
- [x] 更新 docs/README.md 索引
- [x] 标记废弃文件（3 个）
- [x] 生成整理报告（CLEANUP_REPORT.md、本文件）

---

## 六、下一步行动

1. **Review 本次改动**：`git status` 确认所有变更
2. **运行门禁**：确保没有破坏构建
3. **提交**：`git commit -m "docs: reorganize documentation structure and unify version to 0.0.76"`
4. **（可选）发版**：如果这是 v0.0.76 的正式版本，执行 `tools/release.sh`

---

**整理完成！** 🎉

M-GCS 文档从"扁平混乱"进化为"分类清晰"，版本号统一，重复内容消除，目录结构合理。
