# M-GCS 文档导航

当前版本：`0.0.80`

> 仓库级规则以根目录 [`AGENTS.md`](../AGENTS.md) 为唯一权威源。

---

## 快速导航

### 入门与协作

- [开发指南](guides/DEVELOPMENT.md) — 构建、门禁、发布与提交规范
- [协作流程](guides/COLLABORATION.md) — 多 Agent 协作和任务卡模板
- [故障排除](guides/TROUBLESHOOTING.md) — 常见运行与构建问题

### 架构与决策

- [架构说明](architecture/ARCHITECTURE.md) — 依赖方向、运行时数据流和模块职责
- [项目结构](architecture/PROJECT_LAYOUT.md) — 当前代码组织
- [架构决策记录（ADR）](architecture/adr/) — 0001–0010，历史决策，只追加不改写

### 当前状态与规划

- [当前状态与完成定义](planning/CURRENT_STATE.md) — 门禁基线、可复现事实、维护循环的退出条件
- [路线图与功能差距分析](planning/ROADMAP.md) — 相对 QGC/MP 的差距、开发顺序与阶段验收
- [多机连接与焦点管理](planning/MULTI_VEHICLE.md) — 多机单一权威文档：状态、架构、验证证据、风险
- [历史审计快照](planning/audits/) — 冻结的历史审计，不随当前版本改写

### 技术规范

- [任务规划契约](specs/MISSION_PLANNING.md) — `MissionDocument`、`MissionPlan`、传输和验证
- [QGC `.plan` 互操作](specs/upstream/QGC_PLAN_INTERCHANGE.md) — QGC 任务文件字段映射
- [上游实现选择策略](specs/upstream/SELECTION_STRATEGY.md) — QGC/MP/官方协议按模块择优
- [编码规范](specs/ENCODING.md) — UTF-8、路径和换行规则
- [FFI API 清单](../bridge/ffi/API_CATALOG.md) — 公开 C ABI、稳定性和兼容入口

### 研究文档

- [字体替代方案](research/FONT_ALTERNATIVES_RESEARCH.md)
- [多机关联研究](research/MULTIVEHICLE_CONNECTION_RESEARCH.md) — QGC/MP 源码对照
- [架构图](architecture-diagrams.html)

---

## 文档目录

```text
docs/
├── README.md                  # 本文件：文档导航
├── architecture-diagrams.html # 架构图
├── architecture/              # 架构、目录和 ADR
│   └── adr/                   # 0001–0010 架构决策记录
├── guides/                    # 开发、协作和故障排除
├── planning/                  # 当前状态、路线图、多机、历史审计
│   └── audits/                # 冻结的历史审计快照
├── specs/                     # 任务规划、编码和上游互操作规范
│   └── upstream/              # QGC/MP 参考与选择策略
└── research/                  # 专项研究
```

---

## 文档维护原则

1. 当前基线只在 `AGENTS.md`、根 `README.md`、`docs/README.md` 和
   `planning/CURRENT_STATE.md` 中声明。
2. 历史审计和 ADR 保留原始内容与版本号，只做失效链接修复，不改写结论。
3. 已合并、已完成或只承担重定向用途的文档直接删除，不留空壳。
4. 相同内容只维护一处，其他位置使用相对链接：
   - 当前事实 → `planning/CURRENT_STATE.md`
   - 规划与差距 → `planning/ROADMAP.md`
   - 多机能力 → `planning/MULTI_VEHICLE.md`
   - 上游选择 → `specs/upstream/SELECTION_STRATEGY.md`
5. 每次发布同步四处版本号（`CMakeLists.txt`、`pubspec.yaml`、根 `README.md`、
   `CHANGELOG.md`）以及受影响的规范文档。
6. 文档中的协议和行为描述必须能追溯到代码、QGC/MP 源码或官方定义。

---

**最后更新**：2026-10-09 v0.0.80（多机/signing 未发布改动）
