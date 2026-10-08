# ADR 0010：独立实现，不复制 QGC/MP 代码分支

- 状态：已接受
- 日期：2026-10-08
- 范围：整体架构、源代码来源、许可证和上游兼容策略

## 背景

M-GCS 的交互和协议行为会持续参考 QGroundControl（QGC）和
Mission Planner（MP），但项目需要同时保持：

- MIT 许可证边界；
- Flutter 跨平台外壳；
- C++ Core 与稳定 C FFI；
- ArduPilot 优先、PX4 兼容；
- 不引入 Qt/QML 或 WinForms 全局对象模型。

当前需要明确回答 M-GCS 是否属于 QGC/MP 的二次开发分支或复刻。

## 决策

M-GCS 采用**独立实现、协议兼容、按模块借鉴**的路线。

1. M-GCS 是独立仓库和独立架构，不包含 QGC/MP 的 Git 历史、构建系统或
   UI 对象模型。
2. 官方 MAVLink、ArduPilot/PX4 协议事实和成熟第三方依赖优先复用。
3. QGC 主要用于通用状态机、链路生命周期、任务模型和跨平台工作流参考。
4. MP 主要用于 ArduPilot 参数、机型、校准、现场兼容和 `.waypoints` 语义参考。
5. 不直接复制 QGC/MP 的大段源代码。需要移植时，必须单独审查所选许可证、
   再分发义务、Qt 授权和第三方依赖。
6. 兼容目标的验收标准是协议互操作和行为一致性，不是逐像素复制 UI。

## 证据

- M-GCS 依赖方向为 `flutter_shell -> bridge/ffi -> core -> third_party`。
- M-GCS 使用自有 `MissionDocument`、`MissionPlan`、`ConnectionManager`、
  `Vehicle` 和 JSON FFI 契约。
- `third_party/` 当前只包含官方 MAVLink、cJSON 等通用依赖。
- 任务文件互操作通过字段和协议对齐实现，不链接 QGC/MP 二进制或源码。
- M-GCS 为 MIT；当前 QGC checkout 为 Apache-2.0/GPLv3 双许可，MP 为 GPLv3。

## 影响

积极影响：

- 可以保持清晰的 MIT 发布边界。
- Flutter UI 可以按 macOS 设计语言独立演进。
- Core 的测试、线程和生命周期不受 Qt/WinForms 约束。
- 上游兼容可以通过协议测试和互操作文件持续验证。

代价：

- 需要维护自己的任务模型、状态机和错误语义。
- QGC/MP 新增能力不能自动继承，必须逐项评估和重写。
- 文档必须持续记录参考来源、差异和已知缺口。

## 关联文档

- [上游实现选择策略](../../specs/upstream/SELECTION_STRATEGY.md)
- [功能差距分析](../../planning/FEATURE_GAP_ANALYSIS.md)
- [任务规划契约](../../specs/MISSION_PLANNING.md)
- [架构说明](../ARCHITECTURE.md)
