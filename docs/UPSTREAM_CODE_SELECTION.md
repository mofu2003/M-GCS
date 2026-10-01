# MP / QGC 上游实现择优准则

> 状态：当前开发准则。M-GCS 首先服务 ArduPilot，同时保留 PX4/QGC 兼容能力。
> 本文件规定同一能力同时存在 Mission Planner（MP）和 QGroundControl（QGC）
> 实现时，默认参考谁、何时组合，以及如何记录依据。

本准则的正式架构决策见 [ADR 0009：混合上游实现策略](adr/0009-hybrid-upstream-implementation-strategy.md)。

## 目标

M-GCS 不逐项复制某一套地面站，也不因为 QGC 的模块化程度高就忽略 MP 对
ArduPilot 的长期现场积累。正确做法是按能力比较：

1. 官方 MAVLink、ArduPilot 源码和协议规范是否已经给出权威定义。
2. 模块的正确性、边界、错误语义、状态机完整性和真实使用覆盖谁更强。
3. 直接复用是否受许可证、语言、线程模型或依赖边界限制。
4. 选择结果能否在 M-GCS 的 `flutter_shell -> bridge/ffi -> core -> third_party`
   边界内稳定实现和测试。

质量接近时，不强行判定“整体赢家”；可以组合，但必须说明哪部分来自谁。

## 总体判断

- **通用软件工程质量：QGC 领先。** 任务对象模型、链路生命周期、跨平台结构、
  错误/取消语义和测试规模整体更成熟。
- **ArduPilot 专用领域质量：MP 领先。** 参数元数据、机型差异、校准步骤、固件
  清单、日志字段和现场飞行行为更贴近 ArduPilot。
- **M-GCS 自身边界：保持现有设计。** 不把 QGC 的 Qt/QML/QObject 结构或 MP 的
  WinForms/全局单例耦合搬入 Core；只吸收领域实现、协议事实和状态语义。

因此，上游选择默认按模块判断，而不是给 MP 或 QGC 打一个总分后全盘照搬。

## 质量比较维度

同一模块按以下维度比较，而不是按代码行数或个人偏好选择：

- **协议和 ArduPilot 语义正确性**：命令参数、模式、能力门控、参数元数据和机型差异。
- **状态机完整性**：初始化、重试、超时、取消、断链、重连和终态是否明确。
- **分层和可测试性**：职责是否单一，能否脱离 UI、硬件和全局单例测试。
- **错误语义**：拒绝、超时、能力不可用、部分完成和恢复是否有稳定表达。
- **平台覆盖**：Windows、Linux、macOS、Android 和不同飞控固件是否经过验证。
- **数据覆盖**：ArduPilot 参数、MAV_CMD、日志和固件元数据是否完整且持续更新。
- **许可证与维护状态**：能否合规复用，上游是否仍在维护，接口是否适合当前项目。

## 默认质量归属

下表是当前默认选择，不是永久结论。上游实现变化或 M-GCS 获得更强的实测证据后，
应更新本表并记录原因。

| 能力 | 默认优先来源 | 原因 | 参考入口 |
| --- | --- | --- | --- |
| ArduPilot 固件识别、机型、飞行模式和 MAV_CMD 语义 | MP | 面向 ArduPilot，参数和机型细节覆盖更直接 | `ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs`、`mavcmd.xml`、`Common.cs` |
| ArduPilot 参数元数据、默认值、枚举和参数文件 | MP | 参数库和 ArduPilot 专用语义更完整 | `ParameterFactMetaData.xml`、`ExtLibs/Utilities/ParameterMetaDataRepository*.cs` |
| 参数管理器的异步状态、写入确认和批量刷新 | QGC | `ParameterManager`/`FactSystem` 的边界、类型和状态表达更成熟 | `src/FactSystem/ParameterManager.*`、`FactMetaData.*` |
| `.waypoints` / MP 任务文件 | MP | ArduPilot/MP 生态的默认交换格式 | `GCSViews/FlightPlanner.cs`、`ExtLibs/ArduPilot/mav_mission.cs` |
| `.plan`、ComplexItem、GeoFence、Rally | QGC | 文件结构、复杂项模型和组合规划能力更完整 | `src/MissionManager/MissionController.*`、`ComplexMissionItem.*`、`GeoFenceManager.*`（围栏 mission 编码：5000–5004）、`RallyPoint*` |
| 任务传输 ACK、超时、取消和结果文本 | QGC | `PlanManager` 的生命周期和错误语义更集中 | `src/MissionManager/PlanManager.cc` |
| 任务编辑对象模型和能力门控 | QGC | SimpleItem/ComplexItem/视觉任务项分层更清晰 | `src/MissionManager/MissionController.*` |
| ArduPilot 校准动作和机型差异 | MP | 面向 ArduPilot 的校准顺序、提示和硬件差异经验更多 | `GCSViews/ConfigurationView/Config*Calibration.cs`、`ExtLibs/Utilities/CompassCalibrator.cs` |
| 校准生命周期、取消和状态收敛 | QGC | controller/state 表达更统一，便于接入 M-GCS operation snapshot | `src/Vehicle/VehicleSetup/*Calibration*` |
| 多链路、链路优先级、自动重连和链路诊断 | QGC | 跨平台 LinkManager/VehicleLinkManager 的覆盖更完整 | `src/Comms/LinkManager.cc`、`src/Vehicle/VehicleLinkManager.cc` |
| Windows 串口设备兼容和 ArduPilot 现场异常 | MP | Windows 是 MP 的主要使用平台，设备侧经验更多 | `ExtLibs/Comms/*`、`MissionPlanner` 串口发现流程 |
| Android USB 串口 | QGC | 有成熟的 Java/JNI 和 `usb-serial-for-android` 集成 | `android/src/org/mavlink/qgroundcontrol/QGCUsbSerialManager.java`、`src/Android/AndroidSerial.cc` |
| ArduPilot 引导命令、现场飞行操作 | MP | 更贴近 ArduPilot 使用习惯和能力差异 | `GCSViews/FlightPlanner.cs`、`MAVLinkInterface.cs` |
| 日志下载生命周期、FTP fallback 和取消 | QGC | `LogManager`/FTP 状态和错误处理更完整 | `src/LogManager/*`、`src/Vehicle/FTPManager.*` |
| ArduPilot 日志分析和数据字段解释 | MP | 面向 ArduPilot 日志的工具链和数据经验更丰富 | `Log/`、`dataflashlog.xml` |
| ArduPilot 固件选择和刷写流程 | MP | ArduPilot 固件清单、板卡识别和刷写经验更多 | `ConfigFirmware.cs`、`ExtLibs/ArduPilot/APFirmware.cs`、`ExtLibs/Flasher/` |
| PX4 固件升级和 QGC 平台行为 | QGC | PX4/QGC 官方工作流更直接 | `src/Vehicle/VehicleSetup/FirmwareUpgradeController.*` |
| M-GCS 自身 UI、FFI 和内部领域边界 | M-GCS | 保持现有 Flutter、macOS 设计 token 和 FFI 契约 | `AGENTS.md`、`docs/CONSTRAINTS.md`、`bridge/ffi/API_CATALOG.md` |

## 组合规则

允许并且鼓励按边界组合，例如：

- 任务文件格式采用 QGC 的 `.plan` 结构，ArduPilot 默认导出采用 MP 的 `.waypoints`。
- 参数管理器采用 QGC 的异步状态模型，参数元数据和 ArduPilot 默认值采用 MP。
- 链路管理采用 QGC 的生命周期和重连结构，Windows 串口兼容细节参考 MP。
- 校准流程采用 QGC 的状态和取消模型，ArduPilot 机型相关步骤参考 MP。

组合时必须明确区分：

- 哪部分是协议事实，必须遵循官方定义；
- 哪部分是数据结构或状态机，参考了谁；
- 哪部分是 ArduPilot 或 PX4 的固件行为；
- 哪部分是 M-GCS 自己的 FFI、Flutter 或领域模型。

## 冲突处理

当 MP 和 QGC 对同一行为给出不同做法时：

1. 先回到官方 MAVLink、ArduPilot 或 PX4 定义确认真实协议语义。
2. 再比较两边对超时、拒绝、取消、断链和能力不可用的处理是否完整。
3. 以当前目标飞控的实际行为为准；ArduPilot 主路径优先 MP 的现场兼容经验，
   PX4 主路径优先 QGC 的官方工作流。
4. 如需组合，写成明确边界，例如“QGC 负责结构，MP 负责 ArduPilot 语义”。
5. 在 ADR、模块 README、设计说明或代码注释中记录选择理由和上游文件。

## 禁止事项

- 不做“QGC 全盘优先”或“MP 全盘优先”的项目站队。
- 不因为 MP 界面旧、QGC 结构现代，就直接认定某一模块质量更高。
- 不在未查官方协议、MP、QGC 和现有依赖的情况下自行发明流程。
- 不把 QGC 的 Qt/QML/QObject 结构或 MP 的 WinForms/全局 `MainV2` 耦合直接搬入 Core。
- 不在未确认许可证的情况下大段复制 GPL 源码。

## 关联文档

- [QGC 代码质量审计](QGC_CODE_QUALITY_AUDIT.md)：当前 M-GCS 与 QGC 的分项评分。
- [QGC / MP 可借鉴项对照表](QGC_MP_BACKPORT_MAP.md)：按开发阶段列出的具体模块。
- [开发约束](CONSTRAINTS.md)：安全、依赖、FFI 和任务文件边界。
- [后续发展计划](DEVELOPMENT_ROADMAP.md)：阶段目标和验收条件。
