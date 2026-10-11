# M-GCS

无人机地面站，对标 Mission Planner 与 QGroundControl。优先服务 ArduPilot，兼容 PX4。

当前版本：`0.2.0`。

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
- [docs/workflow/COLLABORATION.md](docs/workflow/COLLABORATION.md)：多 agent 协作指南
- [docs/constraints/](docs/constraints/)：约束细则（架构、FFI 契约、安全、测试、UI、上游研究、对照）
- [docs/constraints/known-deviations.md](docs/constraints/known-deviations.md)：已知偏差台账（未解决 / 已关闭）
- [docs/adr/](docs/adr/)：架构决策记录
- [bridge/ffi/API_CATALOG.md](bridge/ffi/API_CATALOG.md)：从公开头文件和前端绑定生成的接口目录
- [docs/capabilities.md](docs/capabilities.md)：FFI 模块交付状态与 UI 覆盖矩阵（阶段 0 基线）
- [docs/plans/mgcs-development-plan-2026-10-10.md](docs/plans/mgcs-development-plan-2026-10-10.md)：分阶段开发计划与当前进度
- [docs/mission-format-contract.md](docs/mission-format-contract.md)：任务文件（原生 / `.waypoints` / QGC `.plan`）单位、版本与 round-trip 契约
- [docs/mission-capability-table.md](docs/mission-capability-table.md)：任务命令机型能力表（哪些 `MAV_CMD` 在 Copter/Plane/VTOL/Rover 上执行，以及导入/上传/前端三处门禁）
- [docs/comparisons/ground-station-comparison-2026-10-10.md](docs/comparisons/ground-station-comparison-2026-10-10.md)：与 QGroundControl、Mission Planner 的多维对比报告
- [docs/features/](docs/features/)：阶段 3 功能简报（逐方向的需求、上游研究、差距、验收标准）：[视频/相机/云台](docs/features/video-camera-gimbal.md)、[测绘与地形](docs/features/survey-and-terrain.md)、[飞行日志与回放](docs/features/flight-logging-and-replay.md)、[PX4 对齐](docs/features/px4-parity.md)
- [docs/sitl-scenario-evidence.md](docs/sitl-scenario-evidence.md)、[docs/sitl-scenario-evidence-plane.md](docs/sitl-scenario-evidence-plane.md)：SITL 端到端任务场景证据（ArduCopter / ArduPlane 各一轮，含复算命令）
- [CHANGELOG.md](CHANGELOG.md)：按版本记录的变更（Keep a Changelog + SemVer）
