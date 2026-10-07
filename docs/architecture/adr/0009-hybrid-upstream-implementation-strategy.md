# ADR 0009：混合上游实现策略

- 状态：已接受
- 范围：ArduPilot、PX4、MAVLink、任务、参数、校准、连接和跨平台能力

## 背景

M-GCS 同时服务 ArduPilot 和 PX4，但两套上游地面站的优势不同：Mission Planner
对 ArduPilot 的参数、校准、任务字段和现场行为更深入；QGroundControl 在任务模型、
链路生命周期、跨平台结构、取消/重试和测试治理上更成熟。项目不能整体复制某一方，
也不能在没有查上游的情况下重新设计已有协议流程。

## 决策

M-GCS 采用三层组合策略：

1. **官方协议层优先**：MAVLink 消息、枚举、单位、任务协议和 capability 以官方定义
   以及目标飞控行为为准。
2. **QGC 负责通用框架**：任务对象模型、链路生命周期、session/vehicle 边界、状态机、
   超时、重试、取消、跨平台和测试治理优先参考 QGC。
3. **MP 负责 ArduPilot 语义**：ArduPilot 参数元数据、机型差异、校准步骤、飞行模式、
   任务文件 `.waypoints` 和现场兼容行为优先参考 MP。
4. **M-GCS 保留自己的边界**：实现必须落在
   `flutter_shell -> bridge/ffi -> core -> third_party`，不直接引入 QGC 的 Qt/QML/QObject
   或 MP 的 WinForms/全局单例结构。

## 选择规则

当 MP 和 QGC 行为一致时，复用官方字段和共同流程；行为冲突时按以下顺序处理：

1. 查官方 MAVLink、ArduPilot 或 PX4 定义。
2. 比较两边的错误、超时、取消、断链、能力门控和测试覆盖。
3. ArduPilot 主路径优先采用 MP 的现场兼容语义，通用状态机采用 QGC 的结构。
4. PX4 主路径优先采用 QGC 的官方工作流。
5. 在设计文档、模块注释或 ADR 中记录选择依据。

## 许可证边界

QGC 和 MP 为 GPL 系项目，M-GCS 当前为 MIT。默认只复用协议事实、字段、错误码、算法
和交互顺序，在 M-GCS 模块边界内重写；直接移植大段源码前必须单独核对许可证。官方
MAVLink 和许可证兼容的成熟第三方依赖可以优先直接复用。

## 影响

积极影响：

- ArduPilot 主路径不会丢失 MP 的专用语义。
- 通用框架不会重复踩 QGC 已解决的状态机和生命周期问题。
- FFI 和 Flutter 不会被任一上游的 UI/对象模型绑死。
- 后续代码审阅可以明确判断“参考谁、为什么”。

代价：

- 同一功能可能同时需要阅读 MP 和 QGC 两套源码。
- 组合实现必须增加契约测试和差异说明。
- 不能用“照搬某个上游”替代边界设计。

## 关联文档

- [UPSTREAM_CODE_SELECTION.md](../UPSTREAM_CODE_SELECTION.md)
- [QGC_MP_BACKPORT_MAP.md](../upstream/QGC_MP_BACKPORT_MAP.md)
- [AGENTS.md](../../../AGENTS.md)
- [REFACTORING_PLAN.md](../audits/2026-10-03-refactoring-plan.md)
