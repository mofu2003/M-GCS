# M-GCS 上游实现选择策略

当前版本：`0.0.78`

本文定义 M-GCS 如何借鉴 QGroundControl、Mission Planner、官方 MAVLink 和成熟第三方库的实现。

> **工作原则**：混合策略，不站队。按模块比较质量，择优借鉴。

---

## 一、总原则

M-GCS 首先服务 **ArduPilot**，同时保持 **PX4** 兼容。实现上游已有能力时：

1. **不按项目整体站队**，而要按模块比较 MP、QGC、官方协议和成熟第三方库的质量
2. **优先复用**官方 MAVLink、许可证兼容的成熟上游库或 QGC/MP 实现
3. **不能直接复用时**，以质量更高的上游实现的字段、状态机、错误语义为参考
4. **冲突时记录比较依据**，不凭偏好选择
5. **不允许凭个人习惯从零设计**，必须先查过 QGC/MP 对应模块

### 许可证约束

- 当前 QGC checkout 为 **Apache-2.0 / GPLv3 双许可**，MP 为 **GPLv3**，M-GCS 当前是 **MIT**
- **大段源码不能直接复制**；选择 Apache-2.0 分支时仍需核对 Qt 商业授权和再分发义务
- **可复用**：协议事实、算法、字段、流程、允许再分发的第三方库
- UI 只借工作流、状态和信息层级，不照搬 Qt/QML/WinForms 布局

### 本地源码位置

- QGC：`/Users/fansicheng/Projects/qgroundcontrol`
- MP：`/Users/fansicheng/Projects/MissionPlanner`

---

## 二、领域默认归属

### ArduPilot 语义 → 优先参考 Mission Planner

| 领域 | QGC 参考 | MP 参考 | 选择理由 |
| --- | --- | --- | --- |
| **参数元数据** | `ParameterManager`、`FactMetaData` | `ParameterMetaDataParser`、`apm.pdef.xml` | MP 完整覆盖 ArduPilot 参数定义、版本重映射、机型差异 |
| **机型差异** | `APMFirmwarePlugin` | `APMFirmware`、`Copter/Plane/Rover` 配置 | MP 涵盖 ArduPilot 全机型（Copter/Plane/Rover/Sub/Tracker/Heli） |
| **校准动作** | `VehicleSetup` 校准控制器 | `Config*` 校准页面（陀螺、加速度、磁力、气压、遥控器） | MP 与 ArduPilot 校准流程同步更新，状态文本准确 |
| **固件上传** | `FirmwareUpgradeController` | `Firmware` 下载和烧录 | MP 直接对接 ArduPilot 发布，px4/bootloader 处理完整 |
| **飞行行为** | `APMFirmwarePlugin` 的 ArduPilot 差异 | `MAVState`、各机型飞行模式 | MP 覆盖 ArduPilot 特有模式（LAND/RTL/AUTO/GUIDED 等） |

### 通用框架 → 优先参考 QGroundControl

| 领域 | QGC 参考 | MP 参考 | 选择理由 |
| --- | --- | --- | --- |
| **任务对象模型** | `MissionItem`、`SimpleMissionItem`、`ComplexMissionItem`、`MissionController` | `MAVLinkInterface` 的任务读写 | QGC 完整分离 SimpleMissionItem/ComplexItem，支持 Survey/Corridor/GeoFence/Rally，状态机清晰 |
| **链路生命周期** | `LinkManager`、`VehicleLinkManager`、`SerialLink`、`TCPLink`、`UDPLink` | `MAVLinkInterface`、`SerialPort` | QGC 支持多链路、重连、心跳监控、链路质量统计 |
| **跨平台结构** | 完整 Android/iOS/Linux/macOS/Windows CMake 构建 | 仅 Windows（.NET Framework / Mono） | QGC 跨平台构建成熟，Android USB 串口完整 |
| **状态机完整性** | `Vehicle`、`FirmwarePlugin`、各 Manager 分层清晰 | `MAVState`、`MainV2.comPort` 全局耦合 | QGC 状态机独立、可测、无全局单例 |
| **测试治理** | 单元测试、集成测试、模拟链路 | 测试覆盖较弱 | QGC 有完整测试框架，mock vehicle/link |

### 协议定义 → 使用官方 MAVLink

| 领域 | QGC 参考 | MP 参考 | M-GCS 实现 |
| --- | --- | --- | --- |
| **MAVLink 消息** | 官方 MAVLink C 头生成 | 官方 MAVLink C# 绑定 | 使用官方 MAVLink（`tools/generate-mavlink.sh`），不自行发明协议 |
| **协议流程** | `MissionProtocol`、`ParameterProtocol`、`LogProtocol` | `MAVLinkInterface` 的协议实现 | 复用官方定义，状态机参考 QGC，错误语义参考 MP |

---

## 三、具体模块参考清单

### A. 任务规划

| M-GCS 需求 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 任务文档模型 | `MissionItem`、`MissionController`、`PlanMasterController` | `MainV2` 任务读写 | **QGC 为主**：SimpleMissionItem/ComplexItem 分离，支持 GeoFence/Rally |
| Survey / Corridor | `SurveyComplexItem`、`CorridorScanComplexItem`、`TransectStyleComplexItem` | Grid / Survey 计算 | **QGC 为主**：ComplexItem 展开直接复用其存入的 `TransectStyleComplexItem.Items` |
| 上传/下载/验证 | `PlanManager`：`_writeMissionItems`、`_handleMissionAck`、`_missionResultToString` | `MAVLinkInterface` 任务读写 | **QGC 为主**：进度、超时、重试、ACK 结果到错误映射 |
| 命令目录 | `MissionCommandTree`、`MavCmdInfo*.json` | `Controls/MavCommandSelection.cs` | **QGC 为主**：参数 label/unit/default/enum 从 MavCmdInfo 对齐 |
| GeoFence | `GeoFenceManager`、`GeoFenceController` | MP Fence 配置 | **QGC 为主**：polygon/circle/inclusion/breach-return 模型 |
| Rally Points | `RallyPointManager`、`RallyPointController` | MP Rally 流程 | **QGC 为主**：独立模型，不硬塞进普通 waypoint |
| `.plan` 文件 | `MissionController.load/save`、`PlanMasterController` | `.waypoints` 读写 | **QGC 为主**：ComplexItem、geoFence、rallyPoints 完整格式 |

### B. 参数管理

| M-GCS 需求 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 参数元数据 | `ParameterManager`、`FactMetaData` | `ParameterMetaDataParser`、`apm.pdef.xml` | **MP 为主**：ArduPilot 参数定义、版本重映射、机型差异 |
| 读取/写入/确认 | `ParameterManager`、`BulkRefreshJob` | `ConfigRawParams`、MAVLink 参数流程 | **QGC 为主**：pending/confirmed/失败原因、批量变更顺序 |
| 参数历史 | `ParameterEditorController` | 无 | **QGC 为主**：参数类型回退和历史记录 |

### C. 校准

| M-GCS 需求 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 校准流程 | `VehicleSetup` 校准控制器 | `Config*` 校准页面（陀螺、加速度、磁力、气压、遥控器） | **MP 为主**：与 ArduPilot 校准流程同步，状态文本准确 |
| 校准状态 | 相关 QML | 参数读回和验证 | **QGC 为主**：统一 CalibrationService、计时、用户提示 |

### D. 连接与链路

| M-GCS 需求 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 链路管理 | `LinkManager`、`VehicleLinkManager`、`SerialLink` | `MAVLinkInterface` | **QGC 为主**：多链路、重连、心跳监控、链路质量 |
| 连接质量 | `Vehicle/VehicleLinkManager`、`Vehicle/Vehicle.cc` | `ConnectionStats` | **QGC 为主**：link quality、heartbeat age、reconnect、link loss 统一进 diagnostics |
| Android USB 串口 | `android/src/org/mavlink/qgroundcontrol/QGCUsbSerialManager.java`、`AndroidSerial.cc` | 无 Android 同等实现 | **QGC 为主**：`usb-serial-for-android` + `SerialInputOutputManager` + JNI token 回调 |
| Windows 串口 | `src/Comms/SerialLink`、`VehicleLinkManager` | `ExtLibs/Comms/SerialPort.cs` | **混合**：Win32 超时、读写线程关闭、端口描述/VID/PID 参考 MP |
| Linux 串口 | `src/Comms/SerialLink`、`LinkManager` | Mono/Linux 兼容层 | **QGC 为主**：POSIX open/termios、热插拔、设备权限 |

### E. 日志

| M-GCS 需求 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 日志列表/下载 | `LogManager`、`OnboardLogController`、`FTPManager` | `LogBrowse`、`LogDownloadMavLink` | **QGC 为主**：日志条目状态、FTP fallback、错误文本、文件命名 |
| 取消操作 | `LogManager` | 无 | **QGC 为主**：operation 取消、进度回报 |

### F. 固件

| M-GCS 需求 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 固件上传 | `FirmwareUpgradeController` | `Firmware` 下载和烧录 | **MP 为主**：直接对接 ArduPilot 发布，px4/bootloader 处理完整 |

---

## 四、QGC / MP 可借鉴项对照表

本节把 M-GCS 路线表中的剩余工作，对照本地 QGC 和 MP 源码，记录可以直接沿用、需要改写或暂不适合照搬的部分。

### A. 任务闭环可靠性

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 任务脏状态、覆盖确认 | `MissionManager/MissionController.cc`、`PlanMasterController.cc`、`PlanView/PlanView.qml` | `MainV2`、`FlightPlanner` 的任务读写入口 | 保留现有脏状态；继续对齐 QGC 的 `dirtyChanged`、下载前确认和本地覆盖提示 |
| 上传进度、超时、重试、结果文本 | `MissionManager/PlanManager.cc`：`_writeMissionItems`、`_startAckTimeout`、`_ackTimeout`、`_handleMissionAck`、`_finishTransaction`、`_missionResultToString` | `MAVLinkInterface` 任务读写 | 已有 operation 状态；下一步补齐 QGC 的 `progressPctChanged` 语义、ACK 结果到中文错误映射、失败后本地任务保留策略 |
| 下载读取、完成回读 | `PlanManager::loadFromVehicle`、`_handleMissionCount`、`_handleMissionItem` | `readWaypoints` / `writeWaypoints` | 已接入；重点对齐 QGC 的部分下载失败清理、回读 provenance 和覆盖确认 |
| 命令目录、校验参数 | `MissionManager/MissionCommandTree.*`、`MissionCommandUIInfo.*`、`MavCmdInfo*.json`、`SimpleMissionItem.cc` | `Controls/MavCommandSelection.cs` | 扩充 M-GCS command catalog；参数 label/unit/default/enum 优先从 QGC 的 MavCmdInfo 对齐 |
| 航点列表、地图选中、排序 | `PlanView/PlanTreeView.qml`、`MissionController.cc`、`MissionItemMapVisual.qml` | FlightPlanner 航点列表和地图联动 | 前端继续沿用 M-GCS 设计；只对齐选中/当前航点/无效项提示的行为 |
| Home 和 RTL 安全 | QGC `FirmwarePlugin/APM/APMFirmwarePlugin`、`Vehicle::doSetHome` | MP Home 设置与确认流程 | 已实现确认；继续核对 PX4/ArduPilot Home 和 RTL 差异，不把 Home 当普通航点写入任务 |

### B. 任务文件与领域互操作

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| QGC `.plan` 文件 | `MissionManager/MissionController.cc` 的 `load/save`、`PlanMasterController`、`MissionItem.cc`、`ComplexMissionItem` | `.waypoints` 读写 | 已完成 SimpleItem、旧 `coordinate`、`ComplexItem`、`geoFence`、`rallyPoints` 的文件结构映射 |
| Survey / Corridor | `SurveyComplexItem`、`CorridorScanComplexItem`、`TransectStyleComplexItem`、`SurveyPlanCreator` | Grid / Survey 计算逻辑 | M-GCS 既有自有 survey/corridor；QGC ComplexItem 的展开直接复用其存入的 `TransectStyleComplexItem.Items`，不再自己重推几何。后续参数命名、相机和重叠率字段继续按 QGC 对齐 |
| GeoFence | `GeoFenceManager`、`GeoFenceController`、`GeoFenceMapVisuals.qml` | MP Fence 配置与任务编辑 | 已完成 Fence 文档模型（polygon/circle/inclusion/breach-return）、几何校验、按 QGC 编码的编译/上传/下载/回读/verify |
| Rally Points | `RallyPointManager`、`RallyPointController`、`RallyPointMapVisuals.qml` | MP Rally/集结点流程 | 已完成 Rally 独立模型、读写、编译/上传/下载/回读/verify，不硬塞进普通 waypoint 列表 |
| 任务文件无损失往返 | QGC `PlanMasterController` 的 mission/geoFence/rally 组合文件 | MP `.waypoints`、任务保存/读取 | M-GCS 文档已有正式 fence/rally/complexItem 容器字段；ComplexItem 可展开参与编译（几何来自 QGC，内部字段仍只读），无法展开的复杂项会明确告警 |

### C. 诊断、长任务和参数可追踪

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 统一长任务模型 | 无单一类；分别看 `PlanManager`、`ParameterManager`、`LogManager`、`FTPManager` | 各 UI 页面自己的进度状态 | M-GCS 保留统一 operation snapshot；字段和终态语义对齐 QGC 各 manager，而不是强拆到一个状态机 |
| 参数读取、写入确认、重试 | `FactSystem/ParameterManager.*`、`ParameterEditorController.*`、`BulkRefreshJob.*` | ConfigRawParams、MAVLinkInterface 参数流程 | 已加入参数历史；继续对齐 QGC 的 pending/confirmed/失败原因、参数类型回退和批量变更顺序 |
| 连接质量、心跳、断线重连 | `Vehicle/VehicleLinkManager.*`、`Comms/LinkManager.*`、`Vehicle/Vehicle.cc` | ConnectionStats、重连与串口恢复 | 把 link quality、heartbeat age、reconnect、link loss 事件统一进 diagnostics；保留 M-GCS 现有 session 目标模型 |
| 日志列表、下载、取消 | `LogManager/LogManager.*`、`AnalyzeView/OnboardLogs/OnboardLogController.*`、`OnboardLogsFtp/OnboardLogFtpController.*`、`Vehicle/FTPManager.*` | LogBrowse、LogDownloadMavLink | 已加入 operation 取消；继续对齐 QGC 的日志条目状态、FTP fallback、错误文本和文件命名 |
| 校准状态、历史、验证 | `VehicleSetup` 校准 controllers、相关 QML | `Config*` 校准页面和参数读回 | 已有统一 CalibrationService；继续对齐计时、用户提示、失败原因和不同固件的校准差异 |

### D. 平台和 USB 串口

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| Android USB 串口 | `android/src/org/mavlink/qgroundcontrol/QGCUsbSerialManager.java`、`QGCUsbSerialProber.java`、`src/Android/AndroidSerial.cc` | 无 Android 同等实现 | 已改为 QGC 结构：`usb-serial-for-android` + `SerialInputOutputManager` + JNI token 回调；继续补权限交互和真机拆卸/重连测试 |
| Windows 串口和发现 | `src/Comms/SerialLink.*`、`VehicleLinkManager.*` | `ExtLibs/Comms/SerialPort.cs`、Windows 设备发现 | 对照 Win32 超时、读写线程关闭、端口描述/VID/PID 和串口重连行为 |
| Linux 串口 | `src/Comms/SerialLink.*`、`LinkManager.*` | Mono/Linux 兼容层 | 对照 POSIX open/termios、热插拔、设备权限和 tty 命名 |
| 跨平台构建 | QGC Android/CMake/Gradle 构建组织 | MP Windows 习惯 | M-GCS 已补 Windows/Linux/Android CI；下一步只补真实平台运行和 bundle 启动验证 |

---

## 五、推荐优先级

1. **参数和任务错误语义**：先对齐 QGC 的 ACK/超时/拒绝/取消错误文本和终态，不改外部 FFI 名称
2. **QGC `.plan` 完整格式**：ComplexItem、GeoFence、Rally 已无损保留并可参与编译/上传/回读/verify（ComplexItem 内部字段仍只读）
3. **Fence/Rally 文档模型**：已完成，独立于 waypoint，不再用普通航点模拟
4. **Android USB 真机**：权限、attach/detach、USB 重连、任务/参数/校准流程验收
5. **Windows/Linux 真机**：串口、断线重连、参数和任务上传下载

---

## 六、不直接照搬的部分

- MP 的全局 `MainV2.comPort` 状态耦合不能进入 Core
- QGC 的 Qt object/property/signal 结构不能原样移植到 Flutter FFI
- MP/QGC 的 UI 布局、控件尺寸和主题不进入 M-GCS；只借工作流、状态和字段
- GPL/上游源码的大段复制需要单独处理许可证和授权；M-GCS 当前优先做接口和算法对齐

---

## 七、参考资料

- **完整规则**：[`AGENTS.md`](../../../AGENTS.md)（仓库根目录）
- **QGC .plan 格式详解**：[`QGC_PLAN_INTERCHANGE.md`](QGC_PLAN_INTERCHANGE.md)
- **任务规划契约**：[`MISSION_PLANNING.md`](../MISSION_PLANNING.md)

---

**本文只记录可追溯的工程方向；具体实现仍以 M-GCS 当前契约、测试和 `AGENTS.md` 的架构规则为准。**
