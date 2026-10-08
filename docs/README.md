# M-GCS 文档导航

当前版本：`0.0.78`

> **仓库级规则**以根目录 [`AGENTS.md`](../AGENTS.md) 为唯一权威源。

---

## 快速导航

### 入门与协作

- [开发指南](guides/DEVELOPMENT.md) — 构建、门禁、发布与提交规范
- [协作流程](guides/COLLABORATION.md) — 多 Agent 协作和任务卡模板
- [故障排除](guides/TROUBLESHOOTING.md) — 常见运行与构建问题

### 架构与决策

- [架构总览](architecture/ARCHITECTURE.md) — 依赖方向、运行时数据流和模块职责
- [目录结构](architecture/PROJECT_LAYOUT.md) — 当前代码组织
- [ADR 0009：混合上游实现策略](architecture/adr/0009-hybrid-upstream-implementation-strategy.md)
- [ADR 0010：独立实现，不复制 QGC/MP 代码分支](architecture/adr/0010-independent-upstream-compatibility.md)

### 规划与审计

- [功能差距分析](planning/FEATURE_GAP_ANALYSIS.md) — M-GCS 相对 QGC/MP 的已具备项、缺口和优先级
- [开发路线图](planning/DEVELOPMENT_ROADMAP.md) — 阶段目标、依赖与验收条件
- [完成定义](planning/DEFINITION_OF_DONE.md) — 后端维护循环和产品级完成条件
- [当前状态审计](planning/STATUS_AUDIT.md) — 当前 checkout 的可复现事实
- [历史审计快照](planning/audits/) — 冻结的历史审计，不随当前版本改写

### 技术规范

- [任务规划契约](specs/MISSION_PLANNING.md) — `MissionDocument`、`MissionPlan`、传输和验证
- [QGC `.plan` 互操作](specs/upstream/QGC_PLAN_INTERCHANGE.md) — QGC 任务文件字段映射
- [上游实现选择策略](specs/upstream/SELECTION_STRATEGY.md) — QGC/MP/官方协议按模块择优
- [字符编码规范](specs/ENCODING.md) — UTF-8、路径和换行规则
- [FFI API 清单](../bridge/ffi/API_CATALOG.md) — 公开 C ABI、稳定性和兼容入口

### 研究文档

- [字体替代方案](research/FONT_ALTERNATIVES_RESEARCH.md)
- [多机关联研究](research/MULTIVEHICLE_CONNECTION_RESEARCH.md)

---

## 文档目录

```text
docs/
├── README.md
├── architecture/               # 架构、目录和 ADR
├── guides/                     # 开发、协作和故障排除
├── planning/                   # 路线图、完成定义、状态审计和功能差距
├── specs/                      # 任务规划、编码和上游互操作规范
│   └── upstream/               # QGC/MP 参考与选择策略
├── research/                   # 专项研究
├── CLEANUP_REPORT.md           # 历史整理报告
├── CONSTRAINTS.md              # 已废弃，迁移到 AGENTS.md
├── REORGANIZATION_SUMMARY.md   # 历史重组报告
└── UPSTREAM_CODE_SELECTION.md  # 已废弃，迁移到 specs/upstream/
```

---

## 已废弃文档

以下文件仅保留历史链接：

- `CONSTRAINTS.md` → [`AGENTS.md`](../AGENTS.md) 的架构约束章节
- `UPSTREAM_CODE_SELECTION.md` → [`SELECTION_STRATEGY.md`](specs/upstream/SELECTION_STRATEGY.md)
- `specs/upstream/QGC_MP_BACKPORT_MAP.md` → [`SELECTION_STRATEGY.md`](specs/upstream/SELECTION_STRATEGY.md)

---

## 文档维护原则

1. 当前基线只在 `AGENTS.md`、根 `README.md`、`docs/README.md` 和
   `STATUS_AUDIT.md` 中声明。
2. 历史审计和整理报告保留原始版本号，并在顶部标注“历史报告”。
3. 相同内容只维护一处，其他位置使用相对链接。
4. 每次发布同步版本号、状态审计、API 清单和受影响的领域规范。
5. 文档中的协议和行为描述必须能追溯到代码、QGC/MP 源码或官方定义。

---

**最后更新**：2026-10-08 v0.0.78
