# 功能对照表

每项功能列出 QGC 与 MP 的参考位置、两者的做法、M-GCS 的现状，以及结论。新增或修改功能前，必须先在本表中补全该功能的参考信息（见 `upstream.md` 的 P1）。

## 参考源码版本

| 项目 | 本地路径 | 提交 | 日期 |
|---|---|---|---|
| QGroundControl | `~/Projects/qgroundcontrol` | `bf0096d97` | 2026-10-06 |
| Mission Planner | `~/Projects/MissionPlanner` | `a2fcd74d6` | 2026-07-11 |

核对日期：2026-10-09。版本变化后，相应条目需要重新核对。

状态取值：`已对齐`、`有差距`、`建议偏差`（不阻断合并，但需写明理由）、`有意不同`（需写明理由）、`未核对`。

本表只记录行为事实与参考位置，不复制源码。规则的约束与建议见 `safety.md` 等文件。

## 安全行为

### 解锁确认方式

- **QGC**：长按确认，500 ms（`GuidedActionsController.qml:375-390`，`GuidedActionConfirm.qml:91-114`）。顶部状态指示器的解锁按钮也是 500 ms 长按，且直接写入 armed 状态（`Toolbar/MainStatusIndicator.qml:228-238`）。强制解锁不经过确认，但默认关闭（`Settings/FlyView.SettingsGroup.json:161`）。手柄解锁无确认（`Joystick.cc:1711-1715`）。
- **MP**：解锁无确认（`GCSViews/FlightData.cs:1034`）。上锁需要 YesNo 确认（`FlightData.cs:1045-1049`）。解锁被拒绝后，弹出 YesNo 询问是否强制解锁（`FlightData.cs:1059-1074`）。手柄解锁无确认（`ExtLibs/ArduPilot/Joystick/JoystickBase.cs:431, 465`）。
- **M-GCS**：顶栏长按 500 ms 确认（`lib/main.dart:219-230`）。
- **结论**：已对齐 QGC。强制解锁入口待核对。

### 模式切换确认

- **QGC**：长按确认。默认要求确认，延迟 500 ms（`Toolbar/FlightModeIndicator.qml:156-172`，设置 `Settings/FlightMode.SettingsGroup.json:100`）。对话框分支被注释掉，实际执行的是长按之后的赋值。
- **MP**：无确认。只有处于 failsafe 时才弹 YesNo（`FlightData.cs:1644-1656`）。快捷按钮 AUTO、LOITER、RTL 直接执行（`FlightData.cs:1415, 1430, 1449`）。
- **M-GCS**：顶栏模式菜单已改为长按确认，时长 500 ms（`widgets/mg_command_confirmation.dart:11` 的 `kMgHoldToConfirmDuration`，`widgets/mg_top_status_bar.dart:163-181` 使用；测试 `test/mg_flight_mode_hold_test.dart:49` 断言长按到底只触发一次）。
- **结论**：已对齐（KD-07 已闭环）。500 ms 与 QGC 的 `defaultDelay` 一致。

### ACK 判定与超时

- **QGC**：只按 `(compid, command)` 匹配，不校验目标系统和确认号（`MavCommandQueue.cc:505-512`）。只有 4 类命令会重试，解锁不重试（`MavCommandQueue.cc:219-248`），超时约 1.2 s（`MavCommandQueue.h:114`）。非 ACCEPTED 的结果弹出提示（`MavCommandQueue.cc:480-503`）。
- **MP**：按 `sysid + compid + command` 匹配（`ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs:2803-2813`）。默认重试 3 次，超时 2 s（`MAVLinkInterface.cs:2729-2731`）。解锁超时 10 s，仍重试 3 次，同步阻塞 UI 线程（`MAVLinkInterface.cs:2764-2768`）。非 ACCEPTED 的结果静默返回 false，只写日志（`MAVLinkInterface.cs:2829-2833`）。
- **M-GCS**：按命令号匹配，不按目标系统匹配（KD-02）。错误是否展示待核对。
- **结论**：有差距。目标系统匹配采用 MP 的做法（`safety.md` 的 ACK 匹配建议），非 ACCEPTED 的结果展示给用户采用 QGC 的做法。解锁不自动重试（采用 QGC 的做法）。

### 任务上传确认与失败处理

- **QGC**：上传无确认（`PlanView/PlanView.qml:112-127`）。解锁状态下禁止在任务模式上传（`MissionController.cc:2423-2433`）。超时 1.5 s，重试间隔 250 ms，最多 5 次（`MissionManager/PlanManager.h:59-62`）。收到 MISSION_ACK 错误时立即失败，不重试（`PlanManager.cc:560-641`）。失败只弹出提示（`Vehicle/Vehicle.cc:1611-1615`）。**写入采用双槽**：待写列表 `_writeMissionItems` 与当前任务 `_missionItems` 分开，只有 ACK 成功后才用待写列表覆盖当前任务，失败则丢弃待写列表、当前任务保持原样（`PlanManager.cc:833-847`）。`_handleMissionAck` 会先校验 `mission_type`，不匹配直接忽略（`PlanManager.cc:568`）。QGC 不在此处校验 sysid，因为它为每架飞机建立独立 MAVLink 通道，来源过滤发生在上游。
- **MP**：上传无确认，只在绝对高度时询问（`FlightPlanner.cs:650-656`）。单个条目超时 450 ms，重试 10 次（`MAVLinkInterface.cs:4061-4093`）。失败在进度框中显示。取消后留下部分写入的任务（`FlightPlanner.cs:6264-6271`）。接收 MISSION_ACK 时校验来源 `buffer.sysid == req.target_system`（`MAVLinkInterface.cs:4098`，另见 `:3863`、`:4273`），不匹配的 ACK 不参与本次事务判定。
- **M-GCS**：上传无确认（KD-08）。上传 ACK 的来源校验与失败回滚已在 `mavlink_adapter_mission.cc:266-296` 补齐：校验 `mission_type`（取 QGC）+ 校验来源 sysid/compid（取 MP，多机共用链路时必需）+ 上传前备份、被拒绝/发送失败/超时后回滚（取 QGC 的双槽语义，但用单槽 + 备份实现，改动面更小）。清除任务的发送失败与 NACK 也走同一套回滚。
- **结论**：建议偏差（上传确认）。已决定为建议级别，见下文"已记录的决定"第 1 条。超时已返回明确错误（KD-14 已闭环：超时不再被误报成「用户取消」，见上）。失败处理按 QGC 的做法，要求显示明确的错误。来源校验与失败回滚三项已对齐两者较优做法：QGC 有双槽与 `mission_type` 校验但依赖上游 sysid 过滤，MP 有 sysid 校验但无回滚，M-GCS 三者兼有。

### 校准取消

- **QGC**：APM 罗盘校准启动前先发送 `DO_CANCEL_MAG_CAL`，确认后再发送 `DO_START_MAG_CAL`（`APM/APMSensorsComponentController.cc:215-285`）。取消时发送 `DO_CANCEL_MAG_CAL`（`cc:428-436`）。其他类型取消只在本地停止，PX4 发送全零的 `PREFLIGHT_CALIBRATION`（`Vehicle/Vehicle.cc:2485-2497`）。**PX4 上 QGC 不发任何取消命令**——其 PX4 控制器直接注明固件不支持取消、只能等飞控自己结束（`PX4/SensorsComponentController.cc:397-399`）。QGC 自身没有校准超时，依赖固件超时。遥控器校准取消纯本地（`VehicleSetup/RemoteControlCalibrationController.cc:499-502`）。
- **MP**：罗盘取消发送 `DO_CANCEL_MAG_CAL`（`GCSViews/ConfigurationView/ConfigHWCompass.cs:449-510`）。无超时。加速度、电调、遥控器校准无取消（`ConfigAccelerometerCalibration.cs`，`ConfigESCCalibration.cs:31-38`，`ConfigRadioInput.cs:197-227`）。`PREFLIGHT_CALIBRATION` 的超时为 25 s，重试 1 次（`MAVLinkInterface.cs:2725-2760`）。
- **M-GCS**：取消已按类型下发命令——陀螺/气压/遥控发全零 `PREFLIGHT_CALIBRATION`（对齐 QGC 的 `Vehicle::stopCalibration`），罗盘发 `DO_CANCEL_MAG_CAL` 并关闭流，加速度发 `ACCELCAL_VEHICLE_POS`（位置 16777216 = 取消）。停摆超时 30 s 判失败（`core/src/calibration/calibration_service.cc`）。**PX4 分支**：固件只认 `MAV_CMD_PREFLIGHT_CALIBRATION`（`PX4-Autopilot/src/modules/commander/Commander.cpp:1440-1500` 的 param1/param2/param3/param4/param5 映射），罗盘启动因此改用 `241 param2=1`——原 `DO_START_MAG_CAL` 会被 PX4 直接忽略，界面看着在校准、实际什么都没发生；取消与六面确认在 PX4 上明确返回「不支持」，而不是假装成功（S8）。
- **结论**：有差距。两者的罗盘取消都会下发命令，M-GCS 已下发（`safety.md` 的 S8）。无超时违反 S4，已用 30 s 停摆超时修复。PX4 的命令映射与「不支持取消」的诚实提示均已按 QGC 的做法对齐；超时数值仍可参考 MP 的 25 s 收窄。

### 断链后的状态清理

- **QGC**：3500 ms 无心跳即标记链路为失联（`Vehicle/VehicleLinkManager.h:85`，`VehicleLinkManager.cc:101-160`）。所有链路都失联时设置 `communicationLost` 并通知界面（`FlyViewToolBar.qml:18`，`MainStatusIndicator.qml:50`）。解锁状态和飞行模式不会被清零（`Vehicle.cc:898-966`）。所有链路移除后才删除飞行器（`VehicleLinkManager.cc:231-248`）。内置自动重连，带退避（`Comms/LinkManager.cc:540-585`，`Comms/LinkConfiguration.h:76-88`）。用户主动断开不会重连。
- **MP**：没有心跳状态机。数据中断超过 3 s 时显示警告，且只在解锁时显示（`MainV2.cs:2803-2811`）。断开连接时不重置解锁状态和模式（`MainV2.cs:1389-1398`）。没有自动重连（`ExtLibs/Utilities/AutoConnect.cs` 只发现新端点）。
- **M-GCS**：状态残留（KD-15、KD-16）。断链通知待核对。
- **结论**：有差距。解锁状态必须失效（`safety.md` 的 S5 约束），这一点比两者都严格。通知和自动重连参考 QGC，属于建议。见下文"已记录的决定"第 2 条。

## 其他功能

### 链路管理与连接

- **QGC**：由 `LinkManager` 统一管理创建、连接、断开和日志回放（`Comms/LinkManager.h:29-66`）。类型包括串口、UDP、TCP、蓝牙、模拟（仅 debug 构建）和日志回放（`Comms/LinkConfiguration.h:106-120`）。串口发现由 `SerialPortManager` 和 `SerialAutoConnect` 负责。
- **MP**：没有统一管理类。链路类型分散在 `ExtLibs/Comms/`，都继承自 `CommsBase`（`CommsBase.cs:18`）。类型包括串口、TCP、UDP、WebSocket、BLE、WinUSB 和文件回放。连接逻辑在 `MainV2`（`MainV2.cs:1523, 3322`）和 `AutoConnect` 中。
- **M-GCS**：串口、TCP、UDP、内存链路，由核心的 `ConnectionManager` 统一管理。前端事件靠 `Timer.periodic` 轮询（KD-27）。
- **结论**：结构已对齐 QGC（统一管理）。事件推送是建议偏差（KD-27，待决定）。

### 参数

- **QGC**：发送 PARAM_SET 后最多等待 1000 ms 的 PARAM_VALUE（`FactSystem/ParameterManager.cc:306-462`，`ParameterManager.h:113-115`）。回读比对参数名、类型和值，浮点数允许模糊比较（`ParameterManager.cc:330-365`）。PARAM_ERROR 直接视为失败。重试 2 次。失败后弹出"Parameter write failed"并重新读取该参数。
- **MP**：发送 PARAM_SET，只按参数名匹配 PARAM_VALUE，不比较回传的值，也不处理 PARAM_ERROR（`MAVLinkInterface.cs:1623-1790`）。重试 3 次，每次间隔 700 ms，超时后抛出 `TimeoutException`。
- **M-GCS**：参数写入无确认（KD-09）。写入后是否回读校验，待核对。
- **结论**：建议偏差（写入确认）。写入后的回读校验采用 QGC 的做法，属于建议。

### 任务规划

- **QGC**：上传、清除、围栏与集结点都有对应入口。清除任务时有 Yes/Cancel 确认（`PlanView/PlanToolBarIndicators.qml:92-101`）。清除时先清空本地计划，再发送清除命令（`MissionManager/PlanMasterController.cc:521-538`）。失败无回滚。
- **MP**：本地清除无确认，且不影响飞控（`FlightPlanner.cs:2064-2081`）。没有 MISSION_CLEAR_ALL，上传时总是包含 Home 点（`FlightPlanner.cs:6226`），因此无法通过上传把飞控任务清为零项。
- **M-GCS**：上传、清除、围栏与集结点已实现。清除任务使用长按确认（`lib/mission/mission_planning_panel.dart:588`）。清除与上传失败都会回滚本地计划（KD-19 已闭环）。上传无确认（KD-08）。
- **结论**：清除的确认方式（长按）满足 S3。上传确认是建议偏差。回滚两者都没有，M-GCS 已超出参考做到回滚。

### 地图操作

- **QGC**：Goto、Orbit、ROI、设置 Home、设置航向、设置当前航点都经过长按确认（`FlyView/GuidedActionConfirm.qml:91` 的 `QGCDelayButton`）。**确认延迟是 500 ms**：`QmlControls/QGCDelayButton.qml:24` 的 `defaultDelay: 500`，全仓只有 `Toolbar/FlightModeIndicator.qml:159` 覆盖它（设置开关决定 500 或 0）。注意同文件 `:42` 的 `interval: 3000` 是帮助文案的隐藏计时器，**不是**确认延迟——本表此前把它误记为确认延迟。Orbit 与 ROI 另有高度滑块（`GuidedActionsController.qml:230-240`）。
- **MP**：设置 Home 有 OK/Cancel 确认（`FlightData.cs:4869-4871`）。ROI 只输入高度，无确认（`FlightData.cs:4513-4519`）。Goto 直接执行（`FlightData.cs:3084-3120`）。Orbit 与设置航向在代码中未找到。
- **M-GCS**：地图菜单里的六条命令——移动到此处、环绕飞行、设置兴趣点、设置 Home、设置估计原点、设置航向——**全部**经过长按确认（`map/amap_map_view.dart:545-585`）。瓦片源仍固定（KD-37）。
- **结论**：已对齐（KD-12 已闭环），确认延迟 500 ms 与 QGC 一致，无需改代码。`safety.md` 的 S3 在此满足。

### 遥测与波形

- **未核对**。本轮核对没有覆盖两者的遥测界面，需要单独核对。
- M-GCS 侧：遥测仪表（`lib/widgets/mg_telemetry_dashboard.dart`）与实时波形（`lib/components/realtime_waveform.dart`）已实现。

### 校准

- 与"安全行为"中的"校准取消"合并核对。
- **QGC**：启动使用 `PREFLIGHT_CALIBRATION`，不经过 ACK 队列，不重试，也不显示错误（`Vehicle/Vehicle.cc:2405-2484`）。
- **MP**：加速度、电调、遥控器的校准分别在对应配置页面中实现（见上文）。
- **M-GCS**：已按类型下发取消命令，并有 30 s 停摆超时（KD-17 已闭环）。PX4 上不存在的取消命令会明确返回「不支持」而非假装成功，详见上文「校准取消」。

### 固件与飞机配置

- **QGC**：有 `FirmwarePlugin` 抽象，按固件选择插件（`FirmwarePlugin/FirmwarePluginManager.cc:75-91`）。APM 按机型细分子类（`FirmwarePlugin/APM/APMFirmwarePlugin.h:22`）。模式列表、参数元数据与 MAV_CMD 信息按固件分离。
- **MP**：没有插件抽象。固件是一个枚举（`ExtLibs/ArduPilot/Firmwares.cs:6-18`）。模式列表在 `Common.cs` 中硬编码，参数名与命令编码在配置页面中硬编码。
- **M-GCS**：核心层有固件插件（与 QGC 结构相同）。RTL / 降落 / 悬停已改为按**语义角色**向插件请求（`FirmwareAdapter::resolve_mode_role`），通用代码里不再出现模式号字面量。该机型没有这个模式时**不下发**并打印警告（Rover 与 Plane 没有 Land、Sub 没有 RTL），而不是把别的机型的数值发出去。
- **结论**：结构已对齐 QGC，S6 与 KD-18 闭环。核对过程中还发现模式表本身有错：三张表把「模式列表下标」当成 `custom_mode` 枚举值，从某一号起整体错位——Copter 的 9 写成 Drift（真值 LAND，于是降落时界面显示「Drift」），Rover 的 9 写成 Dock（真值 CIRCLE，而旧 `land()` 恰好发 9，在 Rover 上「降落」= 原地转圈），Tracker 的 1 写成 Scan（真值 STOP）。已按 `ArduCopter/mode.h`、`Rover/mode.h`、`AntennaTracker/mode.h` 的枚举值重写，并用 `test_firmware_adapter.cc` 钉住关键项。

### 日志

- **QGC**：实现了 MAVLink 日志协议（`LOGGING_DATA_ACKED`，`Vehicle/MAVLinkLogManager.h/.cc`）。遥测日志 `.tlog` 由设置控制（`Comms/MAVLinkProtocol.cc:319-358`）。未发现 ArduPilot 机载 dataflash 下载。
- **MP**：每个连接写入 `.tlog` 与 `.rlog`（`MainV2.cs:1592-1630`）。支持机载 dataflash 下载（`LOG_REQUEST_LIST`，`MAVLinkInterface.cs:6017-6240`）。内置日志分析器（`LogAnalyzer/`）。
- **M-GCS**：核心日志域包含 MAVLink FTP。飞行日志写入与机载下载的完整性待核对。
- **结论**：未核对（M-GCS 侧）。

### 围栏与集结点

- **QGC**：上传无确认（`MissionManager/PlanMasterController.cc:319-345`）。发送顺序为任务、围栏、集结点（`PlanMasterController.cc:276-315`）。疑似缺陷：任务上传失败后，`_sendMissionComplete` 不接收错误参数，流程可能仍继续发送围栏和集结点（`PlanMasterController.cc:276`，待运行验证）。失败只弹出提示（`Vehicle/Vehicle.cc:1617-1627`）。
- **MP**：上传无确认。围栏上传时先把 `FENCE_ACTION` 置为 0，失败后不回滚（`FlightPlanner.cs:3820-3856`）。集结点失败后 `RALLY_TOTAL` 保持已修改（`FlightPlanner.cs:5936-5956`）。在勾选后，每 5 s 后台写入一次集结点，无逐次确认（`Controls/MovingBase.cs:230-245`）。
- **M-GCS**：上传无确认（KD-08）。
- **结论**：建议偏差（上传确认）。见下文"已记录的决定"第 1 条。

## 已记录的决定

以下四项在核对之后已经决定。依据是：涉及飞机状态改变的，写入约束；只影响交互体验的，写入建议。

1. **确认门槛**。
   - 决定：飞行状态命令（解锁、上锁、起飞、降落、返航、急停、模式切换、任务启动、Goto、Orbit、ROI）必须确认，属于 `safety.md` 的 S3 约束。写入类操作（任务上传、围栏与集结点上传、参数写入）建议确认，属于建议，偏离时写明理由。
   - 理由：前者会立即改变飞机的运动或状态。后者只影响后续行为，QGC 与 MP 也都不确认，硬性要求会拖慢配置流程。

2. **断链后的状态**。
   - 决定：断链后必须通知界面，并将依赖链路的解锁状态标为"未知或过期"，属于 `safety.md` 的 S5 约束。清零进行中的传输与校准、自动重连作为建议。
   - 理由：断链后看到的解锁状态可能已经过时，显示为当前值会误导操作者。模式等其他状态的处理方式可以由具体功能决定。

3. **地图操作的确认时长**。
   - 决定：地图指令需要确认，属于约束（S3）。时长不写入约束，作为建议：采用 QGC 的 500 ms（`QGCDelayButton.qml:24` 的 `defaultDelay`），解锁同样 500 ms。
   - 理由：时长是交互参数，不是安全不变量。此前本表把 QGC 的 3000 ms 帮助文案计时器误当成确认延迟，据此建议过 3 s；核对上游源码后更正为 500 ms，代码无需改动。若将来要更长的延迟，属于产品选择而非 parity 要求。

4. **QGC 的疑似缺陷**。
   - 决定：不采纳，不写入任何规则。在本表中保留"待运行验证"的标记。
   - 理由：尚未运行验证，不能作为依据。M-GCS 的做法已经由 `safety.md` 的约束决定。

5. **终止飞行（DO_FLIGHTTERMINATION）的暴露方式**。
   - 决定：core 保留能力，**界面不暴露**。FFI 主名改为 `mg_command_terminate_flight`，旧名 `mg_command_emergency_stop` 保留为转发以免打断并行分支。
   - 理由：该命令让飞控立即切断动力，空中执行等于坠机且不可撤销。QGC 全仓 0 处引用，即刻意不暴露；MP 在 Actions 下拉里以 `Terminate_Flight` 暴露并要求一次 Yes/No 确认。M-GCS 的 `safety.md` S3 只规定"若有入口必须确认"，并未要求该入口存在。
   - 一并澄清：S3 里的"急停"指的是 `COMPONENT_ARM_DISARM` 的 `param2=21196`（MAVLink 官方描述即 emergency stop），**可恢复**，与终止飞行是两件事。该能力已接通（`disarm(force: true)`），界面暂未入口；将来若要加"急停"按钮，应加这一个，且按 S3 做确认。
