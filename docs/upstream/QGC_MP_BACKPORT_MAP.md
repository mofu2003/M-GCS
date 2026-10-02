# QGC / MP 可借鉴项对照表

本文件把 M-GCS 路线表 A-D 中的剩余工作，对照本地 QGroundControl 与 Mission Planner
源码，记录可以直接沿用、需要改写或暂不适合照搬的部分。路线表 E 是依赖 A-D 结果的
0.1.0 产品化门槛，不在这里单独展开上游复用项。它是工程参考，不改变 M-GCS 的 FFI、
Flutter 或 Core 边界。

## 使用原则

- M-GCS 首先服务 ArduPilot。不能把“参考 QGC”或“参考 MP”当成项目站队；同一能力
  先按正确性、状态机、分层、测试、平台覆盖、数据覆盖和许可证比较，选择质量更高
  的实现。默认分项归属见 [MP / QGC 上游实现择优准则](../UPSTREAM_CODE_SELECTION.md)。
- **优先复用协议字段、状态含义、错误码和交互顺序**，不要重新发明 MAVLink 任务、参数、校准和连接流程。
- QGC 和 MP 都是 GPL 系项目，M-GCS 当前是 MIT。大段源码不能直接复制进 `core/` 或 `flutter_shell/`，除非项目单独处理许可证。默认可复用其字段、算法、状态机和交互约定，在 M-GCS 的模块边界内实现。
- UI 只能借鉴工作流和信息层级，不照搬 Qt Widgets/QML 布局。M-GCS 继续使用 `macos_ui` 和现有设计 token。
- 每次移植先确认对应 M-GCS 契约和测试，尤其是任务、参数、校准、连接和飞行命令。

## A：任务闭环可靠性

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 任务脏状态、覆盖确认 | `MissionManager/MissionController.cc`、`PlanMasterController.cc`、`PlanView/PlanView.qml` | `MainV2`、`FlightPlanner` 的任务读写入口 | 保留现有脏状态；继续对齐 QGC 的 `dirtyChanged`、下载前确认和本地覆盖提示 |
| 上传进度、超时、重试、结果文本 | `MissionManager/PlanManager.cc`：`_writeMissionItems`、`_startAckTimeout`、`_ackTimeout`、`_handleMissionAck`、`_finishTransaction`、`_missionResultToString` | `MAVLinkInterface` 任务读写 | 已有 operation 状态；下一步补齐 QGC 的 `progressPctChanged` 语义、ACK 结果到中文错误映射、失败后本地任务保留策略 |
| 下载读取、完成回读 | `PlanManager::loadFromVehicle`、`_handleMissionCount`、`_handleMissionItem` | `readWaypoints` / `writeWaypoints` | 已接入；重点对齐 QGC 的部分下载失败清理、回读 provenance 和覆盖确认 |
| 命令目录、校验参数 | `MissionManager/MissionCommandTree.*`、`MissionCommandUIInfo.*`、`MavCmdInfo*.json`、`SimpleMissionItem.cc` | `Controls/MavCommandSelection.cs` | 扩充 M-GCS command catalog；参数 label/unit/default/enum 优先从 QGC 的 MavCmdInfo 对齐 |
| 航点列表、地图选中、排序 | `PlanView/PlanTreeView.qml`、`MissionController.cc`、`MissionItemMapVisual.qml` | FlightPlanner 航点列表和地图联动 | 前端继续沿用 M-GCS 设计；只对齐选中/当前航点/无效项提示的行为 |
| Home 和 RTL 安全 | QGC `FirmwarePlugin/APM/APMFirmwarePlugin`、`Vehicle::doSetHome` | MP Home 设置与确认流程 | 已实现确认；继续核对 PX4/ArduPilot Home 和 RTL 差异，不把 Home 当普通航点写入任务 |

## B：任务文件与领域互操作

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| QGC `.plan` 文件 | `MissionManager/MissionController.cc` 的 `load/save`、`PlanMasterController`、`MissionItem.cc`、`ComplexMissionItem` | `.waypoints` 读写 | 已完成 SimpleItem、旧 `coordinate`、`ComplexItem`、`geoFence`、`rallyPoints` 的文件结构映射 |
| Survey / Corridor | `SurveyComplexItem`、`CorridorScanComplexItem`、`TransectStyleComplexItem`、`SurveyPlanCreator` | Grid / Survey 计算逻辑 | M-GCS 既有自有 survey/corridor；QGC ComplexItem 的展开直接复用其存入的 `TransectStyleComplexItem.Items`，不再自己重推几何。后续参数命名、相机和重叠率字段继续按 QGC 对齐 |
| GeoFence | `GeoFenceManager`、`GeoFenceController`、`GeoFenceMapVisuals.qml` | MP Fence 配置与任务编辑 | 已完成 Fence 文档模型（polygon/circle/inclusion/breach-return）、几何校验、按 QGC 编码的编译/上传/下载/回读/verify |
| Rally Points | `RallyPointManager`、`RallyPointController`、`RallyPointMapVisuals.qml` | MP Rally/集结点流程 | 已完成 Rally 独立模型、读写、编译/上传/下载/回读/verify，不硬塞进普通 waypoint 列表 |
| 任务文件无损失往返 | QGC `PlanMasterController` 的 mission/geoFence/rally 组合文件 | MP `.waypoints`、任务保存/读取 | M-GCS 文档已有正式 fence/rally/complexItem 容器字段；ComplexItem 可展开参与编译（几何来自 QGC，内部字段仍只读），无法展开的复杂项会明确告警 |

## C：诊断、长任务和参数可追踪

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| 统一长任务模型 | 无单一类；分别看 `PlanManager`、`ParameterManager`、`LogManager`、`FTPManager` | 各 UI 页面自己的进度状态 | M-GCS 保留统一 operation snapshot；字段和终态语义对齐 QGC 各 manager，而不是强拆到一个状态机 |
| 参数读取、写入确认、重试 | `FactSystem/ParameterManager.*`、`ParameterEditorController.*`、`BulkRefreshJob.*` | ConfigRawParams、MAVLinkInterface 参数流程 | 已加入参数历史；继续对齐 QGC 的 pending/confirmed/失败原因、参数类型回退和批量变更顺序 |
| 连接质量、心跳、断线重连 | `Vehicle/VehicleLinkManager.*`、`Comms/LinkManager.*`、`Vehicle/Vehicle.cc` | ConnectionStats、重连与串口恢复 | 把 link quality、heartbeat age、reconnect、link loss 事件统一进 diagnostics；保留 M-GCS 现有 session 目标模型 |
| 日志列表、下载、取消 | `LogManager/LogManager.*`、`AnalyzeView/OnboardLogs/OnboardLogController.*`、`OnboardLogsFtp/OnboardLogFtpController.*`、`Vehicle/FTPManager.*` | LogBrowse、LogDownloadMavLink | 已加入 operation 取消；继续对齐 QGC 的日志条目状态、FTP fallback、错误文本和文件命名 |
| 校准状态、历史、验证 | `VehicleSetup` 校准 controllers、相关 QML | `Config*` 校准页面和参数读回 | 已有统一 CalibrationService；继续对齐计时、用户提示、失败原因和不同固件的校准差异 |

## D：平台和 USB 串口

| M-GCS 方向 | QGC 参考 | MP 参考 | 建议 |
| --- | --- | --- | --- |
| Android USB 串口 | `android/src/org/mavlink/qgroundcontrol/QGCUsbSerialManager.java`、`QGCUsbSerialProber.java`、`src/Android/AndroidSerial.cc` | 无 Android 同等实现 | 已改为 QGC 结构：`usb-serial-for-android` + `SerialInputOutputManager` + JNI token 回调；继续补权限交互和真机拆卸/重连测试 |
| Windows 串口和发现 | `src/Comms/SerialLink.*`、`VehicleLinkManager.*` | `ExtLibs/Comms/SerialPort.cs`、Windows 设备发现 | 对照 Win32 超时、读写线程关闭、端口描述/VID/PID 和串口重连行为 |
| Linux 串口 | `src/Comms/SerialLink.*`、`LinkManager.*` | Mono/Linux 兼容层 | 对照 POSIX open/termios、热插拔、设备权限和 tty 命名 |
| 跨平台构建 | QGC Android/CMake/Gradle 构建组织 | MP Windows 习惯 | M-GCS 已补 Windows/Linux/Android CI；下一步只补真实平台运行和 bundle 启动验证 |

## 推荐优先级

1. **参数和任务错误语义**：先对齐 QGC 的 ACK/超时/拒绝/取消错误文本和终态，不改外部 FFI 名称。
2. **QGC `.plan` 完整格式**：ComplexItem、GeoFence、Rally 已无损保留并可参与编译/上传/回读/verify（ComplexItem 内部字段仍只读）。
3. **Fence/Rally 文档模型**：已完成，独立于 waypoint，不再用普通航点模拟。
4. **Android USB 真机**：权限、attach/detach、USB 重连、任务/参数/校准流程验收。
5. **Windows/Linux 真机**：串口、断线重连、参数和任务上传下载。

## 不直接照搬的部分

- MP 的全局 `MainV2.comPort` 状态耦合不能进入 Core。
- QGC 的 Qt object/property/signal 结构不能原样移植到 Flutter FFI。
- MP/QGC 的 UI 布局、控件尺寸和主题不进入 M-GCS；只借工作流、状态和字段。
- GPL 源码的大段复制需要单独处理许可证；M-GCS 当前优先做接口和算法对齐。

## 本地源码位置

- QGC：`/Users/fansicheng/Projects/qgroundcontrol`
- MP：`/Users/fansicheng/Projects/MissionPlanner`

本文只记录可追溯的工程方向；具体实现仍以 M-GCS 当前契约、测试和 `AGENTS.md` 的架构规则为准。
