# M-GCS 文档整理报告

生成时间：2026-10-07
当前版本：0.0.76

## 一、已完成的整理

### 1.1 版本号统一（✅ 已完成）

已将以下 8 个文件的版本号更新到 `0.0.76`：

- `AGENTS.md` - 新增版本号声明
- `flutter_shell/README.md` - 0.0.70 → 0.0.76
- `bridge/ffi/API_CATALOG.md` - 0.0.70 → 0.0.76
- `docs/DEVELOPMENT.md` - 0.0.74 → 0.0.76
- `docs/STATUS_AUDIT.md` - 0.0.74 → 0.0.76
- `docs/DEVELOPMENT_ROADMAP.md` - 0.0.74 → 0.0.76
- `docs/PROJECT_LAYOUT.md` - 0.0.74 → 0.0.76
- `docs/DEFINITION_OF_DONE.md` - 0.0.74 → 0.0.76
- `docs/README.md` - 0.0.74 → 0.0.76

### 1.2 发现的系统垃圾文件

- `/Users/fansicheng/Projects/M-GCS/.DS_Store`
- `/Users/fansicheng/Projects/M-GCS/build/.DS_Store`
- `/Users/fansicheng/Projects/M-GCS/build/CMakeFiles/.DS_Store`

**建议**：立即删除，并将 `.DS_Store` 加入 `.gitignore`

## 二、文档重复分析

### 2.1 核心指导文档重复（高优先级）

**问题**：三个文件存在大量内容重复

| 文件 | 行数 | 主要内容 | 重复度 |
| --- | ---: | --- | --- |
| `AGENTS.md` | ~400 | 仓库级协作规则，供 AI agent 阅读 | 80% 重复 |
| `docs/DEVELOPMENT.md` | 163 | 开发指南 | 80% 重复 |
| `docs/CONSTRAINTS.md` | 77 | 架构约束 | 60% 重复 |

**重复内容**：
1. 依赖方向规则（`flutter_shell → bridge → core → third_party`）
2. 门禁命令（`ctest` / `flutter analyze` / `flutter test`）
3. FFI 契约变更成套规则
4. 上游实现择优准则（MP/QGC/MAVLink）
5. 提交规范（`<type>: <summary>`）
6. 禁区清单（不提交 build*/不修改 third_party）

**当前维护问题**：
- `AGENTS.md` 第 4 行要求："改动本文件前先确认它与 `docs/DEVELOPMENT.md`、`docs/CONSTRAINTS.md` 一致"
- 实际上三个文件需要**手动同步**，容易遗漏

**整合建议**：

**方案 A：保留三个文件，明确分工**
- `AGENTS.md` - **仅供 AI agent 阅读**，包含完整规则 + AI 特定指令（如字体规则）
- `docs/DEVELOPMENT.md` - **人类开发者指南**，侧重工作流程和命令
- `docs/CONSTRAINTS.md` - **架构原则清单**，纯约束，无操作指令

**方案 B：单一权威源 + 引用（推荐）**
- `AGENTS.md` 作为**唯一权威源**，包含所有规则
- `docs/DEVELOPMENT.md` 改为**人类友好的摘要** + 指向 `AGENTS.md` 的引用
- `docs/CONSTRAINTS.md` **合并入** `AGENTS.md`，作为其中一个章节

### 2.2 上游对标文档重复（中优先级）

| 文件 | 行数 | 主要内容 | 可合并性 |
| --- | ---: | --- | --- |
| `docs/UPSTREAM_CODE_SELECTION.md` | 111 | 择优准则（MP/QGC/MAVLink） | 高 |
| `docs/upstream/QGC_MP_BACKPORT_MAP.md` | 78 | QGC/MP 可借鉴项对照表 | 高 |
| `docs/upstream/QGC_PLAN_INTERCHANGE.md` | 130 | QGC .plan 格式详解 | 低 |

**重复内容**：
- `UPSTREAM_CODE_SELECTION.md` 定义**选择规则**
- `QGC_MP_BACKPORT_MAP.md` 是**具体对照表**
- 两者应该合并

**整合建议**：
- 合并 `UPSTREAM_CODE_SELECTION.md` + `QGC_MP_BACKPORT_MAP.md` → `docs/upstream/SELECTION_STRATEGY.md`
- 保留 `QGC_PLAN_INTERCHANGE.md`（技术细节，不重复）

### 2.3 历史审计文档（可归档）

| 文件 | 行数 | 日期 | 状态 |
| --- | ---: | --- | --- |
| `docs/audits/2026-09-29-v0.0.8.md` | 359 | 9月29日 | 历史快照 |
| `docs/audits/2026-10-01-qgc-quality-audit.md` | 174 | 10月1日 | 历史快照 |
| `docs/audits/2026-10-03-refactoring-plan.md` | 228 | 10月3日 | 历史快照 |

**当前问题**：
- 这些文档是**历史快照**，不应该被频繁访问
- 与 `docs/STATUS_AUDIT.md`（当前审计）混在一起

**整合建议**：
- 保持 `docs/audits/` 目录，但在 `docs/README.md` 中明确标注"历史记录"
- 或者移动到 `docs/.archive/audits/`

### 2.4 字体相关文档重复（低优先级）

| 文件 | 行数 | 主要内容 |
| --- | ---: | --- |
| `docs/adr/0007-unified-font-stack.md` | 144 | ADR：统一字体栈决策 |
| `docs/research/FONT_ALTERNATIVES_RESEARCH.md` | 128 | 研究：字体方案调研 |

**关系**：
- `FONT_ALTERNATIVES_RESEARCH.md` 是**研究过程**
- `0007-unified-font-stack.md` 是**最终决策**（ADR）

**整合建议**：
- 保留两个文件（ADR 应该独立）
- 但在 ADR 中引用研究文档，避免重复描述备选方案

## 三、目录结构优化建议

### 3.1 当前结构

```text
docs/
├── README.md                    # 文档索引
├── ARCHITECTURE.md
├── COLLABORATION.md
├── CONSTRAINTS.md
├── DEFINITION_OF_DONE.md
├── DEVELOPMENT.md
├── DEVELOPMENT_ROADMAP.md
├── ENCODING.md
├── MISSION_PLANNING.md
├── PROJECT_LAYOUT.md
├── STATUS_AUDIT.md
├── TROUBLESHOOTING.md
├── UPSTREAM_CODE_SELECTION.md
├── adr/                         # 架构决策记录（9 个）
├── audits/                      # 历史审计（3 个）
├── research/                    # 研究文档（2 个）
└── upstream/                    # 上游对照（2 个）
```

**问题**：
- 根目录有 **13 个 .md 文件**，太多
- 没有明确的分类逻辑

### 3.2 优化后结构（建议）

```text
docs/
├── README.md                    # 总索引（保留）
│
├── guides/                      # 指南（给人类看）
│   ├── DEVELOPMENT.md           # 开发指南
│   ├── COLLABORATION.md         # 多人协作
│   ├── TROUBLESHOOTING.md       # 故障排除
│   └── ENCODING.md              # 编码规范
│
├── architecture/                # 架构与设计
│   ├── OVERVIEW.md              # 架构总览（原 ARCHITECTURE.md）
│   ├── PROJECT_LAYOUT.md        # 目录结构
│   ├── CONSTRAINTS.md           # 约束清单
│   └── adr/                     # ADR（保留）
│
├── planning/                    # 规划与审计
│   ├── ROADMAP.md               # 路线图（原 DEVELOPMENT_ROADMAP.md）
│   ├── DEFINITION_OF_DONE.md   # 完成定义
│   ├── STATUS_AUDIT.md          # 当前审计
│   └── audits/                  # 历史审计（保留）
│
├── specs/                       # 技术规范
│   ├── MISSION_PLANNING.md      # 任务规划契约
│   └── upstream/                # 上游对照（保留）
│       ├── SELECTION_STRATEGY.md # 择优策略（合并后）
│       └── QGC_PLAN_INTERCHANGE.md
│
└── research/                    # 研究文档（保留）
```

**优势**：
- 按**受众**和**用途**分类（guides / architecture / planning / specs）
- 根目录只剩 `README.md` + 4 个子目录
- 便于导航和维护

## 四、优先级建议

### P0（立即执行）

1. ✅ **版本号统一**（已完成）
2. **删除 .DS_Store 文件**
3. **在 `.gitignore` 中添加 `.DS_Store`**

### P1（本周内）

4. **选择方案 A 或 B 整合三大指导文档**
5. **合并上游对标文档**（`UPSTREAM_CODE_SELECTION.md` + `QGC_MP_BACKPORT_MAP.md`）
6. **执行目录重组**（如果你同意 3.2 的结构）

### P2（可选）

7. **在 `docs/CONSTRAINTS.md` 中添加版本号声明**（当前缺失）
8. **为历史审计文档添加"归档"标注**

## 五、需要你决策的问题

1. **三大指导文档整合**：选择方案 A（保留三个）还是方案 B（单一权威源）？
2. **目录重组**：是否执行 3.2 的新结构？
3. **历史审计**：保留在 `docs/audits/` 还是移动到 `docs/.archive/`？
4. **上游文档合并**：是否合并 `UPSTREAM_CODE_SELECTION.md` + `QGC_MP_BACKPORT_MAP.md`？

## 六、自动化建议

### 6.1 版本号同步脚本

在 `tools/release.sh` 中添加自动更新步骤，确保每次发布时所有文档版本号同步。

### 6.2 文档一致性检查

添加 `tools/check-docs.sh`，检查：
- 版本号是否一致
- 三大指导文档是否同步
- 是否有 .DS_Store 文件

### 6.3 .gitignore 补充

```gitignore
# macOS
.DS_Store
.AppleDouble
.LSOverride

# 已有的构建产物规则...
```
