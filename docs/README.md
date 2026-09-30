# 文档索引

`docs/` 只保存当前仍有效的项目维护文档和架构决策。一次性计划、临时规格、调试输出和已经完成的迁移记录不进入仓库。

## 推荐顺序

1. [项目结构](PROJECT_LAYOUT.md)：目录职责和生成物边界。
2. [开发指南](DEVELOPMENT.md)：构建、测试和硬件验证命令。
3. [架构说明](ARCHITECTURE.md)：Core、FFI、固件能力和诊断契约。
4. [开发约束](CONSTRAINTS.md)：安全、FFI、性能、商业授权和仓库规则。
5. [故障排查](TROUBLESHOOTING.md)：当前可复用的排障路径。
6. [编码规范](ENCODING.md)：UTF-8、LF 和 Windows 终端设置。
7. [ADR](adr/)：已经采纳的长期架构决策。
8. [任务规划后端契约](MISSION_PLANNING.md)：文档模型、编辑命令、编译、上传和回读。
9. [协作流程](COLLABORATION.md)：worktree、任务卡、独占资源和本地预检。
10. [更新日志](../CHANGELOG.md)：版本发布内容与迁移说明。
11. [FFI API Catalog](../bridge/ffi/API_CATALOG.md)：公开 C ABI 分组、兼容入口和弃用规则。
12. [架构图](architecture-diagrams.html)：整体、前端详细、后端详细三张图。
13. [现状审计](STATUS_AUDIT.md)：当前滚动审计、问题 ID、FFI 消费统计和复现命令。
14. [历史审计](audits/)：按 commit/tag 冻结的审计快照。

## ADR

- [ADR 0001：初始架构](adr/0001-initial-architecture.md)
- [ADR 0002：复用官方 MAVLink](adr/0002-official-mavlink-reuse.md)
- [ADR 0003：本地 FFI 桥接](adr/0003-ffi-bridge.md)
- [ADR 0005：旧任务规划决策（已废止，仅作历史追溯）](adr/0005-mission-cad-domain-boundary.md)
- [ADR 0006：波形显示管线——采集保真、显示可读](adr/0006-waveform-display-pipeline.md)

## 维护规则

1. 可执行命令和开发流程写入 `DEVELOPMENT.md`。
2. 长期架构选择写入 `ARCHITECTURE.md` 或 ADR。
3. 已解决问题只在仍有复用价值时保留于 `TROUBLESHOOTING.md`。
4. 同一事实只保留一个权威出处，其他文档使用链接。
