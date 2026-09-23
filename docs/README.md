# 文档索引

`docs/` 只保存当前仍有效的后端维护文档和架构决策。一次性计划、临时规格、调试输出和已经完成的迁移记录不进入仓库。

## 推荐顺序

1. [项目状态](PROJECT_STATUS.md)：后端能力、验证范围和生产缺口。
2. [项目结构](PROJECT_LAYOUT.md)：目录职责和生成物边界。
3. [开发指南](DEVELOPMENT.md)：构建、测试和硬件验证命令。
4. [架构说明](ARCHITECTURE.md)：Core、FFI、固件能力和诊断契约。
5. [开发约束](CONSTRAINTS.md)：安全、FFI、商业授权和仓库规则。
6. [Mission CAD 初步设计](MISSION_CAD.md)：前端无关的参数化航线模型、编译边界和实施分期。
7. [故障排查](TROUBLESHOOTING.md)：当前可复用的排障路径。
8. [编码规范](ENCODING.md)：UTF-8、LF 和 Windows 终端设置。
9. [ADR](adr/)：已经采纳的长期架构决策。
10. [更新日志](../CHANGELOG.md)：版本发布内容与迁移说明。
11. [FFI API Catalog](../bridge/ffi/API_CATALOG.md)：公开 C ABI 分组、兼容入口和弃用规则。

## ADR

- [ADR 0001：初始架构](adr/0001-initial-architecture.md)
- [ADR 0002：复用官方 MAVLink](adr/0002-official-mavlink-reuse.md)
- [ADR 0003：本地 FFI 桥接](adr/0003-ffi-bridge.md)
- [ADR 0005：Mission CAD 由后端领域模型负责](adr/0005-mission-cad-domain-boundary.md)

## 维护规则

1. 当前事实和验收范围写入 `PROJECT_STATUS.md`。
2. 可执行命令和开发流程写入 `DEVELOPMENT.md`。
3. 长期架构选择写入 `ARCHITECTURE.md` 或 ADR。
4. 已解决问题只在仍有复用价值时保留于 `TROUBLESHOOTING.md`。
5. 同一事实只保留一个权威出处，其他文档使用链接。
