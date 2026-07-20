# 文档索引

本目录只保存长期有效的项目文档。一次性计划、临时规格和历史 agent 记录已经移到 `docs/archive/`，不作为当前开发依据。

## 推荐阅读顺序

1. [项目状态](PROJECT_STATUS.md)：当前已经验证什么、还缺什么。
2. [项目结构](PROJECT_LAYOUT.md)：目录职责和哪些内容不应进入源码树。
3. [开发指南](DEVELOPMENT.md)：构建、测试、真实硬件和前端验证命令。
4. [架构说明](ARCHITECTURE.md)：后端、FFI、Flutter 应用契约和依赖方向。
5. [前端组件库](FRONTEND_COMPONENTS.md)：Flutter UI 组件依赖和使用原则。
6. [开发约束](CONSTRAINTS.md)：安全、FFI、前端、仓库卫生和文档边界。
7. [故障记录](TROUBLESHOOTING.md)：已踩过的问题、根因、修复和防再犯规则。
8. [中文与编码规则](ENCODING.md)：UTF-8、LF、CRLF/BOM 和 Windows 终端显示。
9. [ADR](adr/)：已经采纳的长期架构决策。

## 文档职责

- `README.md`：顶层项目入口，只放简明状态、目录和常用命令。
- `PROJECT_STATUS.md`：验收结论、验证记录、风险和下一步。
- `PROJECT_LAYOUT.md`：目录职责、源码树边界和生成物清理规则。
- `DEVELOPMENT.md`：日常开发、构建、测试和硬件验证流程。
- `FRONTEND_COMPONENTS.md`：前端社区组件库、版本和采用边界。
- `ARCHITECTURE.md`：分层、依赖方向、核心模块和契约。
- `CONSTRAINTS.md`：不可随意突破的工程约束。
- `TROUBLESHOOTING.md`：已发生故障的复盘和防再犯规则。
- `ENCODING.md`：中文、编码和换行规则。
- `adr/`：架构决策记录。
- `archive/`：历史临时计划归档，不参与当前开发流程。

## 已采纳 ADR

- [ADR 0001：初始架构](adr/0001-initial-architecture.md)
- [ADR 0002：复用官方 MAVLink](adr/0002-official-mavlink-reuse.md)
- [ADR 0003：本地 FFI 桥接](adr/0003-ffi-bridge.md)
- [ADR 0004：前端接入契约以连接会话为中心](adr/0004-frontend-session-contract.md)

## 维护规则

1. 当前事实和验证状态写入 `PROJECT_STATUS.md`。
2. 日常命令和流程写入 `DEVELOPMENT.md`。
3. 目录职责变化写入 `PROJECT_LAYOUT.md`。
4. 长期架构方向写入 `ARCHITECTURE.md` 或 ADR。
5. 临时计划、调试记录、dump 输出和构建产物不要进入长期文档。
