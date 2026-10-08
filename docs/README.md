# M-GCS 文档导航

当前版本：`0.0.79`

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
- [多机连接实施计划](planning/MULTI_VEHICLE_PLAN.md) — QGC/MP 混合模型、阶段和验收矩阵
- [多机框架当前状态审核](planning/MULTI_VEHICLE_STATUS_REVIEW.md) — 当前实现、验证证据、风险和待审核边界
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
- [架构图](architecture-diagrams.html)

---

## 文档目录

```text
docs/
├── README.md
├── architecture-diagrams.html
├── architecture/               # 架构、目录和 ADR
├── guides/                     # 开发、协作、拆分和故障排除
├── planning/                   # 路线图、完成定义、状态审计和功能差距
│   └── audits/                 # 冻结的历史审计快照
├── specs/                      # 任务规划、编码和上游互操作规范
│   └── upstream/               # QGC/MP 参考与选择策略
└── research/                   # 专项研究
```

---

## 文档维护原则

1. 当前基线只在 `AGENTS.md`、根 `README.md`、`docs/README.md` 和
   `STATUS_AUDIT.md` 中声明。
2. 历史审计保留原始版本号，并在顶部标注为历史快照。
3. 已合并、已完成或只承担重定向用途的文档直接删除，不留空壳。
4. 相同内容只维护一处，其他位置使用相对链接。
5. 每次发布同步版本号、状态审计、API 清单和受影响的领域规范。
6. 文档中的协议和行为描述必须能追溯到代码、QGC/MP 源码或官方定义。

---

**最后更新**：2026-10-09 v0.0.79（多机/signing 未发布改动）
