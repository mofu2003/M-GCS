# ADR 0001：初始架构

## 状态

已接受

## 背景

项目需要构建一款面向 ArduPilot 与 PX4 的工业级地面站，主要目标平台包括 Windows 和 Android。

项目同时要求：

- 严格的前后端分离。
- 长期可维护。
- 合理参考 MissionPlanner 和 QGroundControl 的成熟经验。
- 避免遗留地面站常见的强耦合架构。

## 决策

项目采用：

- C++ Core Engine 作为后端核心。
- 本地 bridge API 层作为未来界面和后端之间的分离边界。

后端仓库架构按职责拆分为：

- `core/`
- `bridge/`
- `tests/`
- `docs/`

## 备选方案

### Qt/QML + C++ Core

优点：

- 与 QGroundControl 生态最接近。
- 复用 QGC 架构经验的路径清晰。

缺点：

- 项目早期明确希望尝试非 Qt 的现代前端路线。

### .NET 现代桌面

优点：

- 与 MissionPlanner 历史技术路线接近。
- Windows 体验成熟。

缺点：

- 与 Windows + Android 双平台目标不够匹配。
- 与 QGroundControl 的复用路径不如 C++ 清晰。

## 影响

1. 任何界面层都不得包含 MAVLink、飞控、传输层或固件逻辑。
2. core 必须保持 UI 无关和平台无关。
3. bridge 层是前端与后端之间最重要的架构边界。
4. QGC/MP 应被研究和选择性复用，不能盲目照搬。
5. 测试必须按层次拆分。

## 后续调整

前端应用曾经从本分支移除，只保留 `core/`、`bridge/ffi/` 和后端测试。这个历史状态
后来已经调整：当前仓库重新包含 `flutter_shell/`，它通过 `bridge/ffi/ffi_api.h` 的
C ABI 接入后端。原始决策“后端保持 UI 无关、FFI 是唯一边界”仍然有效；本段保留历史
变化，当前产品结构以 [`docs/ARCHITECTURE.md`](../ARCHITECTURE.md) 为准。

## 复用说明

- QGroundControl 的架构模式用于指导 core 拆分。
- MissionPlanner 的领域覆盖范围用于指导功能和工作流规划。
- 官方 MAVLink 定义和成熟协议处理实践必须优先复用。
