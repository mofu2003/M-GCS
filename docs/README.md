# M-GCS 文档导航

当前版本：`0.0.76`

> **仓库级规则**请参阅根目录的 [`AGENTS.md`](../AGENTS.md)

---

## 快速导航

### 🚀 新手入门
- [开发指南](guides/DEVELOPMENT.md) - 构建、门禁、提交规范
- [协作流程](guides/COLLABORATION.md) - 多 Agent 协作、任务卡模板
- [故障排除](guides/TROUBLESHOOTING.md) - 常见问题与解决方案

### 🏗️ 架构设计
- [架构总览](architecture/ARCHITECTURE.md) - 依赖方向、模块职责
- [目录结构](architecture/PROJECT_LAYOUT.md) - 代码组织说明
- [ADR（架构决策记录）](architecture/adr/) - 重要设计决策的历史记录

### 📋 规划与审计
- [开发路线图](planning/DEVELOPMENT_ROADMAP.md) - 里程碑、剩余工作
- [完成定义](planning/DEFINITION_OF_DONE.md) - 什么算"完成"
- [当前状态审计](planning/STATUS_AUDIT.md) - 进度跟踪、已知问题
- [历史审计快照](planning/audits/) - 过往版本的审计记录

### 📖 技术规范
- [任务规划契约](specs/MISSION_PLANNING.md) - MissionDocument、MissionPlan、任务文件格式
- [上游选择策略](specs/upstream/SELECTION_STRATEGY.md) - QGC/MP 择优准则、模块对照表
- [QGC .plan 格式详解](specs/upstream/QGC_PLAN_INTERCHANGE.md) - QGroundControl 任务文件格式
- [字符编码规范](specs/ENCODING.md) - UTF-8、路径处理

### 🔧 研究文档
- [Flutter 状态管理调研](../flutter_shell/docs/STATE_MANAGEMENT_EVALUATION.md)
- [地图实现调研](../flutter_shell/docs/MAP_IMPLEMENTATION_RESEARCH.md)

---

## 文档结构

```
docs/
├── README.md                    # 本文件
├── CLEANUP_REPORT.md            # 文档整理报告（2026-10-07）
├── CONSTRAINTS.md               # ⚠️ 已废弃，迁移到 AGENTS.md
├── UPSTREAM_CODE_SELECTION.md   # ⚠️ 已废弃，迁移到 specs/upstream/
│
├── guides/                      # 开发指南
│   ├── DEVELOPMENT.md           # 人类友好的快速参考
│   ├── COLLABORATION.md         # 协作流程
│   └── TROUBLESHOOTING.md       # 故障排除
│
├── architecture/                # 架构设计
│   ├── ARCHITECTURE.md          # 架构总览
│   ├── PROJECT_LAYOUT.md        # 目录结构
│   └── adr/                     # 架构决策记录
│       ├── 0001-xxx.md
│       └── ...
│
├── planning/                    # 规划审计
│   ├── DEVELOPMENT_ROADMAP.md   # 路线图
│   ├── DEFINITION_OF_DONE.md    # 完成定义
│   ├── STATUS_AUDIT.md          # 当前审计
│   └── audits/                  # 历史审计
│       ├── 2026-09-29-v0.0.8.md
│       └── ...
│
├── specs/                       # 技术规范
│   ├── MISSION_PLANNING.md      # 任务规划契约
│   ├── ENCODING.md              # 字符编码
│   └── upstream/                # 上游参考
│       ├── SELECTION_STRATEGY.md         # QGC/MP 择优准则
│       ├── QGC_PLAN_INTERCHANGE.md       # QGC .plan 格式
│       ├── QGC_MP_BACKPORT_MAP.md        # ⚠️ 已废弃
│       └── ...
│
└── research/                    # 研究文档（预留）
```

---

## 废弃文档说明

以下文档已废弃并合并到新位置：

- **CONSTRAINTS.md** → 合并到根目录 [`AGENTS.md`](../AGENTS.md) 的"架构约束"章节
- **UPSTREAM_CODE_SELECTION.md** → 合并到 [`specs/upstream/SELECTION_STRATEGY.md`](specs/upstream/SELECTION_STRATEGY.md)
- **upstream/QGC_MP_BACKPORT_MAP.md** → 合并到 [`specs/upstream/SELECTION_STRATEGY.md`](specs/upstream/SELECTION_STRATEGY.md)

废弃时间：2026-10-07

---

## 相关资源

- **仓库级规则**：[`AGENTS.md`](../AGENTS.md)（根目录，唯一权威源）
- **项目记忆**：`.workbuddy/memory/MEMORY.md`
- **FFI API 清单**：[`bridge/ffi/API_CATALOG.md`](../bridge/ffi/API_CATALOG.md)
- **变更日志**：[`CHANGELOG.md`](../CHANGELOG.md)

---

## 文档维护原则

1. **版本号统一**：每次发布时同步更新所有文档中的版本号声明
2. **避免重复**：相同内容只在一处维护，其他地方引用
3. **废弃标注**：废弃文档保留并指向新位置，不直接删除（便于历史追溯）
4. **交叉引用**：使用相对路径链接，便于重命名和移动
5. **权威源单一**：`AGENTS.md` 是唯一权威规则源，其他文档不应与其冲突

---

**最后更新**：2026-10-07 v0.0.76
