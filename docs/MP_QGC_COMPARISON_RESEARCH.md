# M-GCS 与 Mission Planner / QGroundControl 能力对比研究

> 研究日期：2026-09-21
> 资料范围：Mission Planner 与 QGroundControl 官方文档、官方仓库；M-GCS 当前仓库代码与项目状态文档。
> 目标：识别 M-GCS 与成熟地面站之间的产品级差距。这里将“源码/FFI 中存在接口”和“用户可完成的端到端工作流”严格区分。

## 0.0.4 更新

本版本已经把此前“只有 specialized FFI、Flutter 没有完整入口”的一部分差距收进应用层：

- 任务编辑器现在支持本地航点、地图坐标插入、校验、JSON/CSV 导出和任务传输进度；仍不是完整的 Mission Planner/QGroundControl 命令覆盖。
- 围栏、Rally、飞行日志和 MAVLink FTP 文件树已有前端页面或控制器入口；日志目前解决的是浏览/上传/下载，不等于 DataFlash/TLog 分析器。
- 多机发现、活动机切换和批量基础控制已经接入；批量命令仍依赖 active vehicle 兼容路由，距离 QGC/MP 的完整多机隔离和并发调度还有差距。
- `SessionCapabilities`、应用层 controller 和可替换 shell contract 已落地，后续更换 Flutter 外壳不需要改动 Core/FFI 领域代码。

因此，0.0.4 的定位是“从接口覆盖走向应用层工作流”，而不是宣称已经达到 MP/QGC 的产品成熟度。

## 结论摘要

M-GCS 已经具备一个可继续扩展的地面站骨架：连接、遥测、参数树、部分校准、飞控配置页面、任务/围栏/日志/固件等 C++/FFI 领域接口均已存在，macOS 桌面端已有 ArduPilot 只读连接验证。但它目前还不是 Mission Planner 或 QGroundControl 级别的完整地面站，主要原因不是单个页面缺失，而是以下端到端闭环尚未形成：

1. **任务规划深度不足**：成熟地面站支持可视化任务编辑、复杂 MAV_CMD、测绘航线、相机参数、地形跟随、围栏和 Rally 点的编辑、保存、上传、下载、恢复与失败诊断；M-GCS 当前 Flutter 只接入了简化的任务传输面板，任务/测绘/围栏等多数接口仍属于 specialized FFI，尚未形成完整前端工作流。
2. **飞行前配置和安全闭环不足**：QGC 会按 PX4/ArduPilot 动态显示配置页面，并用红色状态提示未完成的飞行前配置；MP 还提供固件刷写、初始设置、校准、参数、硬件测试和专用调参工具。M-GCS 当前已做参数、机架、电机输出、端口、遥控、PID、安全和校准页面，但真实硬件验证仍主要是只读，固件刷写和飞行控制仍未完成生产级验收。
3. **日志与诊断能力不足**：MP 提供 DataFlash/TLog 下载、图表化字段分析和 KMZ 输出；QGC 提供日志刷新/下载/删除、Log Replay、MAVLink Inspector、实时消息字段图表等开发诊断工具。M-GCS 有日志 API、记录器和回放类，但 Flutter 尚未接入完整的下载、浏览、曲线、回放和诊断工作流。
4. **地图、离线与载荷能力不足**：QGC 支持地图源、离线瓦片集、缓存、令牌、定制地图 URL、测绘相机参数、视频显示/录制和相机控制；M-GCS 当前地图为在线基础地图表面，商业地图、离线地图、密钥和授权清单尚未完成，视频也只有底层接收/解码结构。
5. **产品成熟度与验证广度不足**：QGC/MP 的能力经过长期多飞控、多硬件、多链路使用；M-GCS 目前的核心风险是“接口覆盖看起来很广，但真实飞控、失败恢复、危险操作和跨平台发布覆盖还不够”。

因此，M-GCS 当前更准确的定位是：**具有较好领域模型和 macOS 风格前端基础的早期可用地面站，而不是 MP/QGC 的功能等价替代品**。

## 官方能力基线

### Mission Planner（ArduPilot 官方文档）

Mission Planner 官方总览将其定义为 Plane、Copter、Rover 的地面站，并明确列出三类核心用途：向飞控加载固件、在地图上规划/保存/加载自主任务，以及记录遥测日志。来源：[Mission Planner Overview](https://ardupilot.org/planner/docs/mission-planner-overview.html)。

Mission Planner 的任务文档展示了从 Home、Takeoff、Waypoint、等待、Return/Land 到复杂任务命令的完整任务模型；官方 Mission Commands 文档还覆盖条件命令、DO 命令、相机触发、ROI、云台控制、围栏开关和任务恢复等 MAV_CMD 场景。来源：[Planning a Mission with Waypoints and Events](https://ardupilot.org/planner/docs/common-planning-a-mission-with-waypoints-and-events.html)、[Mission Commands](https://ardupilot.org/planner/docs/common-mavlink-mission-command-messages-mav_cmd.html)。

Mission Planner 的 Rally Points 文档说明了多个备降/返航点的设置、距离限制、Home 是否纳入候选点，以及 Plane/Copter 在 RTL 时的不同处理。这意味着 Rally 不是简单的“保存几个坐标”，而是与安全策略和 RTL 行为关联的工作流。来源：[Rally Points](https://ardupilot.org/planner/docs/common-rally-points.html)。

Mission Planner 官方固件文档覆盖自动识别飞控板、选择 ArduPilot 机型/固件变体、稳定版/定制固件和刷写进度等流程。来源：[Loading Firmware to Boards with an ArduPilot Compatible Bootloader](https://ardupilot.org/planner/docs/common-loading-firmware-onto-pixhawk.html)。

Mission Planner 官方日志文档覆盖 DataFlash 与 Telemetry Log、日志下载、字段选择、曲线绘制、消息/参数查看以及 KMZ 导出到 Google Earth 等分析流程。来源：[Downloading and Analyzing Data Logs in Mission Planner](https://ardupilot.org/planner/docs/common-downloading-and-analyzing-data-logs-in-mission-planner.html)。

Mission Planner 还提供 ArduPilot 相关的罗盘、加速度计、遥控器校准等专门流程。来源：[Compass Calibration](https://ardupilot.org/planner/docs/common-compass-calibration-in-mission-planner.html)、[Accelerometer Calibration](https://ardupilot.org/planner/docs/common-accelerometer-calibration.html)、[Radio Control Calibration](https://ardupilot.org/planner/docs/common-radio-control-calibration.html)。

### QGroundControl（官方用户指南）

QGC Plan View 官方文档把计划视图定义为自主任务规划与上传入口，同时用于编辑 GeoFence 和 Rally Points。其 UI 包含地图、任务工具、可折叠计划树、Mission/GeoFence/Rally 图层切换、保存/上传/从飞控下载、任务统计、地形面板和上传失败排查。来源：[Plan View](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/plan_view/plan_view.html)。

QGC Survey 官方文档支持在任意多边形上生成测绘网格，配置航向、相机、前向/侧向重叠、地面分辨率、相机触发间距、转弯距离、入口点、90 度重飞、相对高度和地形跟随，并显示面积、照片间隔、照片间距与照片数量统计。来源：[Survey Pattern](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/plan_view/pattern_survey.html)。

QGC Vehicle Configuration 官方文档强调“首次飞行前配置”和“已配置飞控调参”两个场景；页面根据 PX4 或 ArduPilot 动态变化，提供搜索、Summary 红色未完成提示、固件、参数、传感器、Radio、Flight Modes、Safety/Failsafes、Motors/Actuators、Tuning、Gimbal、Logging 等页面。来源：[Vehicle Configuration](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/setup_view.html)。

QGC 固件页面支持桌面端向 Pixhawk 类板卡安装 PX4 或 ArduPilot 稳定版、Beta、Daily 或本地固件，显示下载/擦除/升级/重启/重连的进度步骤。来源：[Loading Firmware](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/firmware.html)。

QGC 参数页面支持按组浏览、搜索名称和描述、查看参数详情与是否需要重启，并提供参数文件导入/导出。来源：[Parameters](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/parameters.html)。

QGC Fly View 将飞行控制和监视组合在一页：状态指标、动作、相机工具、视频、可配置遥测值、姿态/罗盘、地图、多机位置与任务显示。来源：[Fly View](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/fly_view/fly_view.html)。视频页面还支持切换地图/视频、相机录制、地面站录制、分离视频窗口和录制格式/存储配置。来源：[Video](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/fly_view/video.html)。

QGC Analyze View 提供日志列表刷新/下载/删除，以及桌面端 MAVLink Inspector：按消息、系统/组件 ID、更新频率查看实时 MAVLink 流，并对字段进行实时图表化。来源：[Log Download](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/analyze_view/log_download.html)、[MAVLink Inspector](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/analyze_view/mavlink_inspector.html)。

QGC Maps 设置提供地图提供商、离线瓦片集、API token、定制瓦片 URL 和缓存管理；离线地图可按区域和缩放级别下载多个 tile set。来源：[Maps](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/settings_view/maps.html)。Telemetry 设置还提供 GCS MAVLink System ID、心跳、协议版本限制、MAVLink 转发、链路质量和 PX4 MAVLink 2 日志流配置。来源：[Telemetry Settings](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/settings_view/telemetry.html)。

官方源码入口：[QGroundControl GitHub](https://github.com/mavlink/qgroundcontrol)、[Mission Planner GitHub](https://github.com/ArduPilot/MissionPlanner)。

## 对照 M-GCS 当前状态

| 能力域 | MP/QGC 官方能力基线 | M-GCS 当前仓库证据 | 差距判断 |
|---|---|---|---|
| 连接与遥测 | 多链路、链路状态、心跳、转发/恢复、实时飞行态势 | `core/src/transport`、`vehicle`、`telemetry`；Flutter 已接入串口/TCP/UDP，ArduPilot 有只读验证 | 基础连接已具备；自动恢复、链路冗余、复杂诊断和真实跨平台验证不足 |
| 参数 | 搜索、分组、描述、重启提示、导入/导出、批量工作流 | 参数树、元数据、同步、读写、文件 API；`docs/PROJECT_STATUS.md` 明确前端已接入参数 | 目前是 M-GCS 最接近成熟的后台模块；还需版本匹配、变更集回滚/校验、失败恢复和 PX4/ArduPilot 深度验证 |
| 飞行前配置 | 按飞控栈显示页面，未完成项红色提示，完整校准/安全/电源/电机/遥控配置 | 已有机架、电机输出、端口、遥控、PID、安全、校准页面 | 页面覆盖在扩大，但端到端流程、危险操作保护和硬件验收不足 |
| 任务规划 | 任务树、Takeoff/Waypoint/Pattern/ROI/Return、GeoFence、Rally、统计、地形、上传下载与失败恢复 | `core/src/mission`、`mission_transfer`、`survey_calculator`，Flutter 仅有 `mission_transfer_panel.dart` | 领域代码有基础，用户级规划器明显不足；这是最高优先级缺口之一 |
| 测绘/航测 | 相机模型、重叠率、GSD、触发、转弯、地形跟随、统计 | 有 `survey_calculator.cc`，但未见等价的完整 Flutter 规划工作流 | “有算法”不等于“可用航测产品”；需 UI、验证、导出/上传、地形和相机联动 |
| 围栏/Rally/安全 | 地图编辑、RTL/Rally 行为、围栏策略、Failsafe/Flight Safety | 有 geofence/rally/fence FFI 和安全页面 | 缺少地图编辑、策略解释、上传下载状态和真实安全验证 |
| 固件 | 下载稳定/Beta/Daily/自定义固件，刷写、擦除、重启、重连、进度 | `firmware_flasher.cc`、插件存在，但项目状态写明固件下载与刷写仍为占位 | 生产级阻断项；需先完成硬件、引导程序、校验和异常恢复 |
| 日志/诊断 | MP 图表分析/KMZ；QGC 下载、回放、MAVLink Inspector、实时图表 | 有 logging API、flight log writer/replayer，Flutter 尚未接入完整工作流 | 需要独立 Analyze 产品，而不是只有记录/列表接口 |
| 地图/离线地图 | 地图源、缓存、token、自定义 URL、离线区域下载 | `mg_map_catalog.dart`、`mg_map_surface.dart`；项目状态明确商业地图、离线地图、Key/授权未完成 | 在线基础地图不足以支撑航测和无网任务；中高优先级 |
| 相机/视频/云台 | Fly View 相机控制、视频切换/录制、载荷任务命令 | 有 `camera_manager.cc`、`video_receiver.cc`、FFmpeg decoder；项目状态明确只有基础结构 | 需完成协议适配、UI、录制、设备状态、任务联动和实机验证 |
| 多机 | QGC Fly Map/状态支持多机，M-GCS 需要活动飞控和路由 | 有 `multi_vehicle_manager.cc` 和 `mg_vehicle_*` 路由；catalog 建议多机客户端使用该族 API | 后端方向正确，但前端多机选择、分组、告警、任务隔离尚未形成 |
| PX4/ArduPilot 双栈 | QGC 按栈动态页面；MP 以 ArduPilot 深度为主 | 有 ArduPilot/PX4 profile、firmware plugin、PX4 专用 FFI | 不能仅凭 profile 判断兼容；应建立每个飞控栈/版本的功能矩阵和实机回归 |

## M-GCS 最需要补齐的产品能力

### P0：形成安全可用闭环

- 完整的连接状态机：连接、参数下载、心跳超时、重连、链路切换、活动飞控选择和错误可解释性。
- 任务规划器：任务树、地图编辑、航点/起飞/返航/降落/ROI/相机/条件/DO 命令、Fence/Rally、保存/导入/导出、上传/下载、失败重试和任务恢复。
- 飞行前检查：把校准、传感器、GPS/EKF、电池、遥控、模式、Failsafe、输出映射和固件状态聚合为可解释的 Preflight/Health 页面。
- 危险动作保护：解锁、起飞、降落、RTL、切模式、参数写入、校准、电机测试、固件刷写必须有明确的飞控状态、二次确认、超时与结果反馈。

### P1：补足专业用户工作流

- Analyze：日志下载/删除、DataFlash/TLog 解析、曲线、多轴对比、参数快照、MAVLink Inspector、回放和故障报告。
- 地图：离线瓦片、多地图源、缓存、token/授权、地形服务、测绘相机模型和地形跟随。
- 载荷：相机/云台发现、控制、录制、视频流、任务中的相机触发和 ROI。
- 多机：按车辆隔离参数/任务/日志/告警，支持多链路和批量只读操作。

### P2：提高生态和交付成熟度

- PX4 与 ArduPilot 按版本生成能力矩阵和自动化兼容性测试。
- Windows/macOS 实机串口验收，Android/iOS 恢复后补权限、USB、文件和打包测试。
- 发布渠道、自动更新、崩溃诊断、导出诊断包、国际化、布局/主题持久化和操作审计。

## 评估时应避免的误判

1. **“有 C++ 文件/FFI 符号”不等于“前端功能完成”**。例如 M-GCS 的 FFI catalog 已明确 mission、logging、firmware、geofence、payload 等接口有一部分属于 specialized，当前 Flutter 尚未接入。
2. **“离线测试通过”不等于“飞控工作流安全”**。当前测试主要验证 JSON/接口不崩溃；参数写入、解锁、切模式、任务执行、校准和电机测试仍需真实硬件的分层验收。
3. **“页面数量接近”不等于“能力等价”**。MP/QGC 的核心优势在于上传下载协议、状态机、错误恢复、固件/参数元数据、日志诊断和飞控栈差异处理，而不只是 UI 页面。

## 参考资料

- ArduPilot Mission Planner 文档：[Mission Planner Overview](https://ardupilot.org/planner/docs/mission-planner-overview.html)、[Mission Planning](https://ardupilot.org/planner/docs/common-mission-planning.html)、[Planning a Mission with Waypoints and Events](https://ardupilot.org/planner/docs/common-planning-a-mission-with-waypoints-and-events.html)、[Mission Commands](https://ardupilot.org/planner/docs/common-mavlink-mission-command-messages-mav_cmd.html)、[Rally Points](https://ardupilot.org/planner/docs/common-rally-points.html)、[Loading Firmware](https://ardupilot.org/planner/docs/common-loading-firmware-onto-pixhawk.html)、[Log Analysis](https://ardupilot.org/planner/docs/common-downloading-and-analyzing-data-logs-in-mission-planner.html)。
- QGroundControl 官方用户指南：[Plan View](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/plan_view/plan_view.html)、[Survey](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/plan_view/pattern_survey.html)、[Vehicle Configuration](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/setup_view.html)、[Firmware](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/firmware.html)、[Parameters](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/setup_view/parameters.html)、[Fly View](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/fly_view/fly_view.html)、[Video](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/fly_view/video.html)、[Log Download](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/analyze_view/log_download.html)、[MAVLink Inspector](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/analyze_view/mavlink_inspector.html)、[Maps](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/settings_view/maps.html)、[Telemetry](https://docs.qgroundcontrol.com/master/en/qgc-user-guide/settings_view/telemetry.html)。
- 官方仓库：[mavlink/qgroundcontrol](https://github.com/mavlink/qgroundcontrol)、[ArduPilot/MissionPlanner](https://github.com/ArduPilot/MissionPlanner)。
- M-GCS 内部依据：[项目状态](PROJECT_STATUS.md)、[FFI API Catalog](../bridge/ffi/API_CATALOG.md)、[MP/QGC 离线接口覆盖测试](../tests/test_mp_qgc_features.cc)。
