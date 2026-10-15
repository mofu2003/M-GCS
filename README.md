# M-GCS

无人机地面站，对标 Mission Planner 与 QGroundControl。

**当前版本（1.0.0）支持 ArduPilot 固件**（Copter/Plane/VTOL/Rover）。

## 结构

- `core/`：C++17 核心，包括链路、MAVLink、固件插件、飞行器、任务、校准、遥测、日志。
- `bridge/ffi/`：C ABI 桥接，前端与核心之间唯一的稳定边界。
- `flutter_shell/`：Flutter 前端，包括地图、连接、遥测、任务规划、配置。
- `third_party/`：第三方源码（cJSON、MAVLink 定义），不修改。
- `tools/`：构建、门禁、发布脚本。

依赖方向：`flutter_shell → bridge/ffi → core → third_party`。

## 文档

- [docs/development.md](docs/development.md)：环境、构建和测试
- [AGENTS.md](AGENTS.md)：当前仍然有效的少量硬约束
- [CHANGELOG.md](CHANGELOG.md)：版本变更
- `docs/constraints/`、`docs/adr/`、`docs/reports/`：历史设计和审计参考，不是普通改动的前置流程
- [bridge/ffi/API_CATALOG.md](bridge/ffi/API_CATALOG.md)：公开 FFI 接口目录
