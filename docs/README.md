# 文档索引

M-GCS 的文档分为三类：

- **当前规范**：描述现在的架构、任务规划、构建、测试和协作方式。
- **接口契约**：`bridge/ffi/API_CATALOG.md` 是公开 C ABI 的分组说明。
- **历史记录**：`docs/adr/` 的废止决策、`docs/audits/` 的冻结审计和 `CHANGELOG.md` 不回写成当前状态。

## 推荐阅读顺序

1. [README](../README.md)：项目范围和快速验证。
2. [项目结构](PROJECT_LAYOUT.md)：目录职责和生成物边界。
3. [架构说明](ARCHITECTURE.md)：Core、FFI、Flutter 和连接/任务数据流。
4. [开发约束](CONSTRAINTS.md)：安全、性能、依赖方向和 UI 规则。
5. [开发指南](DEVELOPMENT.md)：构建、测试、发布和平台运行。
6. [任务规划契约](MISSION_PLANNING.md)：任务文档、命令目录、编译、传输和互操作。
7. [QGC `.plan` 互操作](QGC_PLAN_INTERCHANGE.md)：字段映射、往返保证和错误边界。
8. [故障排查](TROUBLESHOOTING.md)：连接、参数、任务、校准和共享库问题。
9. [协作流程](COLLABORATION.md)：任务卡、worktree、独占资源和预检。
10. [FFI API Catalog](../bridge/ffi/API_CATALOG.md)：稳定 ABI、兼容入口和错误语义。
11. [现状审计](STATUS_AUDIT.md)：当前事实、已修复问题和剩余风险。
12. [后续发展计划](DEVELOPMENT_ROADMAP.md)：与 MP/QGC 的取舍、阶段路线、依赖和验收条件。
13. [QGC/MP 可借鉴项对照](QGC_MP_BACKPORT_MAP.md)：按 A-D 阶段列出可复用模块、建议改写方式和优先级。
14. [MP / QGC 上游实现择优准则](UPSTREAM_CODE_SELECTION.md)：按模块选择质量更高的上游实现，不按项目整体站队。
15. [QGC 代码质量审计](QGC_CODE_QUALITY_AUDIT.md)：M-GCS 与 QGC 的评分、差距和逐步替换策略。
16. [分阶段重构计划](REFACTORING_PLAN.md)：执行中的模块拆分、版本里程碑和验收条件。
17. [架构图](architecture-diagrams.html)：可视化结构总览。

## 研究参考

- [字体替代方案调研](FONT_ALTERNATIVES_RESEARCH.md)：字体选择的测量记录，不替代 ADR 0007。
- [多飞控连接调研](MULTIVEHICLE_CONNECTION_RESEARCH.md)：QGC/MP 连接模型对比，不替代当前架构说明。
- [历史审计](audits/)：按 commit/tag 冻结的旧快照，不代表当前状态。

## 架构决策

- [ADR 0001：初始架构](adr/0001-initial-architecture.md)
- [ADR 0002：复用官方 MAVLink](adr/0002-official-mavlink-reuse.md)
- [ADR 0003：本地 FFI 桥接](adr/0003-ffi-bridge.md)
- [ADR 0005：旧任务规划决策（已废止，仅作历史追溯）](adr/0005-mission-cad-domain-boundary.md)
- [ADR 0006：波形显示管线](adr/0006-waveform-display-pipeline.md)
- [ADR 0007：统一字体栈](adr/0007-unified-font-stack.md)
- [ADR 0008：任务文件格式按飞控生态分层](adr/0008-mission-file-format-priority.md)

## 文档维护原则

1. 同一事实只保留一个权威出处，其他文档使用链接。
2. 当前文档使用当前代码和当前门禁结果；历史结论放入审计快照或 CHANGELOG。
3. 新增 FFI、任务、连接或安全敏感行为时，同时更新契约、测试和文档。
4. 不把未实现功能写成“已支持”。
