# M-GCS

无人机地面站，对标 Mission Planner 与 QGroundControl。优先服务 ArduPilot，兼容 PX4。

## 结构

- `core/`：C++17 核心，包括链路、MAVLink、固件插件、飞行器、任务、校准、遥测、日志。
- `bridge/ffi/`：C ABI 桥接，前端与核心之间唯一的稳定边界。
- `flutter_shell/`：Flutter 前端，包括地图、连接、遥测、任务规划、配置。
- `third_party/`：第三方源码（cJSON、MAVLink 定义），不修改。
- `tools/`：构建、门禁、发布脚本。

依赖方向：`flutter_shell → bridge/ffi → core → third_party`。

## 文档

- [docs/development.md](docs/development.md)：环境、构建、测试、门禁、发布
- [AGENTS.md](AGENTS.md)：协作入口，列出各类任务必读的约束
- [docs/workflow.md](docs/workflow.md)：多 agent 分工、任务流转与验收
- [docs/constraints/](docs/constraints/)：约束细则
- [docs/adr/](docs/adr/)：架构决策记录
