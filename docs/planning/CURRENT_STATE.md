# 当前状态与完成定义

> 本文件合并原《当前状态审计》和《完成定义与后端维护收口》。上半部分是**当前代码的可复现事实**；下半部分是**维护循环的退出条件**（什么时候可以停）。
>
> 历史冻结审计在 `docs/planning/audits/`；发布变化在 `CHANGELOG.md`。当前代码基线版本：`0.0.80`。

---

## 门禁基线

| 检查 | 当前结果 |
| --- | --- |
| `git diff --check` | 通过 |
| `dart tools/check_ui_tokens.dart` | 通过 |
| Flutter analyze | 通过 |
| Flutter test | 123 个常规测试通过、4 个真实 Fleet FFI UDP 测试默认跳过；设置 `MGCS_FLEET_LIBRARY` 后这四个测试单独通过 |
| 后端 offline/simulation | 37/37 |
| macOS debug 构建 | 通过 |
| Windows build/preflight | workflow 统一调用预检脚本；脚本会先生成 MAVLink 头，再构建 bundle 并校验关键 FFI 导出。未在本机 Windows 复核 |
| Linux Flutter bundle | CI 安装 GTK/CMake/Ninja，生成 MAVLink 头，构建 debug bundle，并校验 `libmgcore.so`、数据文件和关键 FFI 导出。未在本机 Linux 复核 |
| Android APK bridge | Gradle/CMake 构建 `libmgcore.so`，数据 JSON 作为 APK assets 打包；Java USB manager 按 QGC 结构使用 `usb-serial-for-android`，native 侧通过 JNI 回调接入 `ILink`。CI 构建 debug APK 并检查产物；权限交互和真机 USB 仍需 Android 设备验证 |
| 远端 CI | 不跟踪（用户要求 `CI-001` WONTFIX） |

可重复命令：

```bash
tools/preflight.sh
bash tools/audit_status.sh
```

## 多链路单机与 UDP 共享链路（MV1，MV2 UDP 基础）

- 采用 QGC 的连接/焦点分离与 MP 的显式系统地址语义，新增 Flutter Fleet
  配置、不可变快照、目标凭证、可注入控制器和现有 ABI 适配层。
- 当前单机连接接入该框架；多机时连接面板显示链路列表并支持精确选择/断开，
  顶部状态栏显示车辆选择器。待确认命令带目标会话/焦点 revision，切机、断链
  或系统身份改变后永久失效。
- Core 接收解析状态使用官方 caller-owned MAVLink 缓冲，两条独立链路的半帧
  交错不会互相污染；注册表查询通过 shared_ptr 快照持有对象生命周期。
- MV2 已接通 UDP：`MavlinkRouter`/`LinkSession` 共享链路单次解析，按
  `sysid/compid` 分发；首个心跳绑定占位 Vehicle，后续 sysid 自动创建独立
  Vehicle，peer-aware UDP 回复发给对应 sender。serial/TCP 使用同一通用
  `LinkSession` 框架，并已通过 TCP loopback 与 POSIX PTY 多 sysid 回归；
  同机多链路归并和 route 精确关闭已接通。
- MV3 基础层新增 `VehicleLinkManager`：管理 route 心跳时间、`comm_lost`、
  primary route 和总通信丢失状态；手动关闭共享车辆后增加 30 秒重发现抑制。
- `Vehicle` 核心层支持挂接多条 shared route，各链路遥测汇聚到同一 Vehicle
  缓存；`VehicleLinkManager` 选择新 primary 后，`conn()`、校准连接和飞行模式
  连接切换到新 route；每个 `MavlinkAdapter` 独立维护 TX sequence，连接、命令和
  FTP 发送不会互相推进序号或产生错误校验和。适配器还支持实例级 MAVLink
  signing 密钥、时间戳和 stream 校验，未签名包在 signing 开启时会被拒绝；
  `mg_vehicle_set_signing_key`、候选密钥、`clear`、`get_signing_status` 已接通；
  入站已签名包可对候选密钥做 HMAC 自动探测，命中后同步到该 Vehicle 的所有 route。
  地面站配置页已有命名密钥库、活动密钥选择、持久化、连接后自动应用和 SETUP_SIGNING 下发；参数、任务、围栏、Rally、校准和日志可按明确 Vehicle 取消，切机会取消旧机可取消操作，并有 A/B 双机隔离测试。`MultiVehicleManager` 已通过独立 link handle 自动归并同一
  `sysid/compid` 的跨 endpoint 链路，FFI 提供 `mg_connection_list()` 和
  `mg_connection_close_route()`，Flutter 连接面板按 link/route/vehicle 分层显示。
- MV5 多机地图继续完善：Fleet 快照携带位置、飞行模式、armed、电量和 GPS 摘要，
  地图非 active 标记支持 Home、完整任务路径、详情提示和点击切换；Fleet 摘要还提供链路健康，顶部车辆选择器显示状态摘要。
- 修复实际双 UDP 清理时的销毁顺序：停止 GCS 心跳、解除事件回调，再关闭车辆。
- `tools/tests/multivehicle_udp_smoke.py` 对真实动态库的双临时 UDP 端口完成 3 轮
  身份、焦点、查询、关闭与销毁验证；Flutter 的 `fleet_ffi_smoke_test.dart`
  还覆盖同一端口 sysid 1/2 自动发现和按车辆精确关闭。
- 显式车辆目标飞行命令、UDP/TCP/PTY 多 sysid、同机多链路合并、route 级精确关闭、
  per-route health、MAVLink TX sequence、signing 密钥库/校验/SETUP_SIGNING、按 session
  的运行时快照、独立 native 任务文档和 Go To 失效处理已完成；真实飞控总线验收、
  异步操作隔离、多机地图基础交付、多选和批量动作已完成第一版；自动密钥探测已完成。

下一切片与验收边界见 [多机连接与焦点管理](MULTI_VEHICLE.md)。

## FFI 消费统计

统计脚本当前输出：

| 项目 | 数量 |
| --- | ---: |
| FFI exports | 318 |
| Flutter 使用 | 84 |
| 测试触达 | 232 |
| 前端和测试都未触达 | 84 |

这些数字必须由 `tools/audit_status.sh` 生成，不手工维护。无人区不等于可删除，公开
兼容 ABI 仍需遵守 API Catalog 的冻结规则。

## 已关闭的问题

- `ARCH-001`：遥测缓存与序列化器已下沉到 `core/include/core/telemetry/`，core 不再依赖 bridge。
- `CAL-001` / `CAL-003`：校准读回失败不再误报成功；历史写盘移出锁内。
- `CAL-002`：guided point 的 COMMAND_LONG 经纬度单位修正为度。
- `CAL-004`：磁力计校准按 QGC/MP 流程先清理旧状态并显式开启
  `MAG_CAL_PROGRESS`/`MAG_CAL_REPORT`；前端修复罗盘进度 JSON 编码导致状态更新中断，
  取消时立即结束本地校准态并在真机上验证进度和取消都有效。
- `HOME-001`：识别飞控后请求 `HOME_POSITION` 和 `EXTENDED_SYS_STATE` 1Hz。
- `HOME-002`：新增 `mg_command_set_home`，设置 Home 前由 UI 确认。
- `HOME-003`：ArduPilot 动态 Home 漂移进入 `home_sync` / `home_sync_count`，不与
  任务项 `differences` 混合；任务页允许采用飞控 Home 或保留本地 Home。
- `LOG-001` / `LOCK-001`：飞行日志接入 STATUSTEXT/连接事件，回调移出锁内。
- `CONN-002`：TCP/UDP 读线程改为共享状态，避免 Windows 超时关闭后的 UAF；
  心跳丢失回收幽灵 Vehicle/session；关闭 active vehicle 后自动选择剩余车辆。
- `UI-007`：任务 controller 绑定当前连接 session，切换飞控后禁止跨机上传；VTOL
  `vtol_state` 接入飞行模式显示；单次遥测读取失败保留最后有效状态。
- `EVENT-001`：`MG_EVENT_WAYPOINT_REACHED` 已接线并带 `seq`。
- `SURVEY-001`：`survey_grid` / `corridor` 已成为参数化任务实体。
- `INTEROP-001`：支持 MP/QGC `QGC WPL 110` `.waypoints` 导入导出。
- `INTEROP-002`：支持 QGC `.plan` 的 `SimpleItem`、Home、默认速度和机型/固件元数据
  导入导出；复杂项、非空 GeoFence/Rally 和无法无损表达的字段不会静默丢弃。
- `MISSION-002`：支持从飞控读取任务并导入任务文档，归一化 `_INT` frame。
- `MISSION-003`：Flutter 任务规划 UI、地图单击、新增/编辑航点、引导操作已接入。
- `MISSION-004`：修正 VTOL 地面启动任务流程；VTOL 不再被当成 QGC
  `Vehicle::fixedWing()`，改为 `GUIDED -> 解锁 -> MISSION_START`。
- `MISSION-005`：任务传输增加 in-progress 门控、mission type 精确路由、来源
  sysid/compid 与期望序号校验；重传/下载清空旧执行进度。
- `MISSION-006`：修正 QGC/ArduPilot `MAV_FRAME_MISSION` 的非定位命令语义；
  `x/y` 不再被无条件当作 degE7 经纬度，非定位命令不会回读成 `(0,0)` 假航点，
  固定翼 `NAV_TAKEOFF` 坐标继续保留。
- `UI-008`：顶部状态栏统一 0.5 秒长按确认按钮；“移动到此处”、返航、开始任务和
  解锁共用白底/蓝色进度填充，确认完成后才下发命令。
- `UI-009`：飞控消息完成中文翻译和固定时间列布局；未知消息保留原文，不猜测翻译。
- `UI-010`：右下角复合 MacGlass 姿态/指南针仪表完成，使用旧版 MacGlass 配色、
  毛玻璃、固定航向箭头和中文方向读数。
- `TEST-001` / `TEST-002` / `TEST-003`：真实 UI FFI、飞行命令、批量参数变更集已有模拟测试。
- `CONTRACT-001` / `DEPRECATION-001`：参数入口和兼容入口规则已登记。
- `TESTGUARD-001` / `TOOL-001` / `TOOL-002`：架构守卫和审计统计不再静默空转。
- `CLEANUP-001`：组件画廊已明确为开发工具，不进入主流程。
- `REFACTOR-001`：Flutter 领域 FFI 适配器已拆分；MAVLink adapter 已完成状态、消息
  处理器、任务协议、参数协议、日志、命令发送和 `process()` 分发的内部拆分，公开
  FFI、JSON 和 MAVLink 行为未变化。
- `REFACTOR-002`：配置工作区从单个 4,099 行文件拆成 `lib/configuration/` 下 35 个
  独立 Dart 文件；旧 `configuration_workspace.dart` 保留为兼容导出，九页路由、
  标签控制器生命周期、校准进度和参数编辑行为均有回归测试。
- `PARAM-001`：参数写入超时后会真正重发 PARAM_SET，并保留目标 sysid/compid；重试
  耗尽进入 failed，发送失败会立即清理 pending，避免后续确认误关联旧请求。
- `PARAM-003`：标准 `PARAM_ERROR` 会立即终止对应写入并映射为 failed，支持
  does-not-exist、out-of-range、permission-denied、read-only 和类型错误等原因。
- `PARAM-004`：描述库 `read_only` 会阻止写入；单写和批量写共享枚举、范围、bool
  校验，批量响应通过 `errors` 返回逐项失败原因。
- `PARAM-005`：新增 `mg_params_refresh`，支持精确名称和 `PREFIX*`，对当前参数缓存
  去重解析并返回 resolved/sent/unknown/duplicates。
- `PARAM-006`：批量刷新具备 1 秒超时、最多 2 次重发、PARAM_VALUE 完成和 PARAM_ERROR
  立即失败；`mg_params_get_refresh_status` 提供 active/total/completed/pending/round/failed。
- `PARAM-007`：参数写入和精确刷新使用 ArduPilot 固件版本重映射；旧固件上的
  `WP_SPD` 会转换为 `WPNAV_SPEED`，新固件、PX4 和未知参数保持原名。
- `PARAM-008`：未分类参数进入稳定的 `other` 分组，保留原始名称、实时值和推断的
  MAVLink 基础类型；`found:false` 明确表示缺少本地扩展元数据。
- `PARAM-014`：参数写入确认绑定 sysid/compid、类型、值代次；参数缓存、下载完成和
  refresh 按当前目标组件及索引校验，旧回包不再污染新会话。
- `PARAM-009`：前缀刷新支持固件版本反查；旧固件缓存里的 `WPNAV_*` 能被
  `WP_*` 刷新请求正确匹配，PX4 和未知版本保持原名。
- `PARAM-010`：`mg_config_get_metadata_coverage` 报告描述库覆盖率、fallback 数量和
  缺失参数名，用于按真实设备参数集补齐元数据。
- `PARAM-011`：覆盖率按固件 canonical 名称计算，旧固件的 `WPNAV_*` 不会被误报为
  缺失；补充 `PILOT_SPD_DN`、`Q_PILOT_SPD_DN`、`ARMING_SKIPCHK` 重映射。
- `PARAM-012`：根据覆盖率补齐高频 ArduPilot 参数基础描述，模拟参数集覆盖率从
  77% 提升到 97%；剩余 fallback 是测试专用未知参数。
- `PARAM-013`：Rover、Submarine、AntennaTracker 现在分别获得 rover/sub/tracker
  profile 标识，不再落入空 profile；当前使用通用描述回退。
- `PLAN-001`：QGC `.plan` 的 ComplexItem、GeoFence、Rally 不再导入时静默丢失或直接
  拒绝，全部纳入正式文档模型并在导出时重建。
- `INTEROP-003`：本地 Survey/Corridor 导出为 QGC `Survey` / `CorridorScan`
  ComplexItem；无法完整表达时退回 SimpleItem + warning，不再直接失败。
- `RALLY-001`：QGC Rally version 2 提升为正式 `rally_points` 模型，支持经纬度和
  高度随任务文档 JSON 及 `.plan` 往返。
- `FENCE-001`：GeoFence 提升为正式模型，覆盖 polygon、circle、inclusion 和
  breach-return。
- `FENCE-002` / `RALLY-002`：校验层补齐围栏与 Rally 几何校验（顶点数量、经纬度范围、
  圆半径、breach-return 点及高度），返回 `geo_fence.*` / `rally.*` 错误码；围栏和
  Rally 属独立 MAVLink mission type，作为合法文档保留，不阻断任务上传。
- `FENCE-003`：`geo_fence` 可编译成 fence mission type 任务项并经
  `mg_mission_doc_upload_fence` / `mg_mission_doc_clear_fence` 下发/清除，编码按 QGC
  `GeoFenceManager`（5001/5002 顶点、5003/5004 圆形、5000 breach-return）；
  `mg_mission_doc_compile_fence` 提供不接触飞控的编译预览。
- `RALLY-003`：`rally_points` 可编译成 rally mission type 任务项并经
  `mg_mission_doc_upload_rally` / `mg_mission_doc_clear_rally` 下发/清除，编码按 QGC
  `RallyPointManager`（5100、`MAV_FRAME_GLOBAL_RELATIVE_ALT`）；`mg_mission_doc_compile_rally`
  提供编译预览。
- `TEST-001`：mock 飞控按 mission type 分库并回显 mission type，新增
  `test_sim_fence_rally` 端到端验证围栏/Rally 上传（fence 5 项、rally 2 项、
  fence clear）。后端 simulation 测试从 13 增至 14。
- `FENCE-004` / `RALLY-004`：`geo_fence_from_plan` / `rally_points_from_plan` 提供
  `compile_*` 的逆过程（围栏分组按 QGC `GeoFenceManager`），FFI 新增
  `mg_mission_doc_import_fence_from_vehicle` / `mg_mission_doc_import_rally_from_vehicle`，
  围栏/Rally 形成上传 + 下载 + 回读闭环；`test_sim_fence_rally` 扩展到 25 项断言。
- `MAV-001`：修复围栏/Rally 下载时 `send_mission_request_int` / `send_mission_ack`
  把 mission type 写死为 0 的 bug（由端到端测试发现）。
- `FENCE-005` / `RALLY-005`：新增 `mg_mission_doc_verify_fence` /
  `mg_mission_doc_verify_rally`，比较文档与飞控围栏/Rally，返回 `matches` 与结构化
  `differences`；`test_sim_fence_rally` 扩展到 29 项断言，覆盖清空飞控后 verify 报差异。
- `UI-003`：围栏/Rally 完成前后端对接。`mg_operations_get_snapshot` 新增
  `fence`/`rally` 进度区段（与 `mission` 同构），Flutter 侧补齐
  `MissionDocumentBackend` 绑定、`MissionDocumentController` 流程和任务面板操作组。
- `UI-004`：任务执行进度。适配器按 seq 去重统计 `MISSION_ITEM_REACHED`，新增
  `mg_mission_get_execution_progress`；任务面板显示"第 N/共 M 项 + 已到达"进度条，
  最后一项到达显示完成。此前 `MISSION_ITEM_REACHED` 只透传回调，启动任务后无进度可看。
- `UI-005`：飞行模式操作台。新增 `mg_flight_get_modes` / `mg_flight_set_mode`，前端
  贴左上角悬浮显示当前模式与模式表（不可切换项置灰），并让连接 / 航线规划 / 飞行模式
  三个浮层可以同时打开。模式表来自既有 `firmware_plugin_ardupilot` /
  `firmware_plugin_px4`，没有新造模式数据。
- `UI-006`：模式映射配置对标 MP `ConfigFlightModes`。`mg_flight_get_mode_mapping`
  给出模式通道、六档参数值与名称、可选模式表以及 `SIMPLE`/`SUPER_SIMPLE`；
  `mg_config_read_param` 按机型注入模式名选项，配置页六档因此渲染为模式名下拉。
  尚未做：`SIMPLE`/`SUPER_SIMPLE` 位掩码的勾选 UI 与模式通道 PWM 数值显示。
- `COMPLEX-001`：QGC ComplexItem（Survey、CorridorScan 等）提升为正式
  `MissionComplexItem` 模型，保存 `id`、`item_type` 和原始 JSON，`.plan` 往返无损；
  新增 `entity_insert_index` 后导出时与普通航点按原顺序交错；内部字段目前只读，
  暂不语义化编辑。最初的“一律阻断上传”策略已由 `COMPLEX-002` 收窄。
- `COMPLEX-002`：复用 QGC 存在 `TransectStyleComplexItem.Items` 里的已算好任务项，
  `expand_complex_item_entities` / `expand_document_entities` 在编译和估算时按
  `entity_insert_index` 展开复杂项，含可展开复杂项的任务可正常校验/编译/上传；
  缺少该区段仍报 `complex_item.unsupported`，未知命令报
  `complex_item.unsupported_command`，可展开项给出 `complex_item.expanded` 警告。
- `CI-001`：按用户要求标记为 WONTFIX，不再列为待办。

## 当前剩余项

### `MV-003`：共享链路生命周期与串口/TCP

UDP 共享链路的 `sysid/compid` 解复用、多 peer、占位 Vehicle 绑定、同机多链路
归并、冗余 primary 切换和 route 精确关闭已落地，TX sequence 也已按适配器隔离。
下一阶段仍需完成真实飞控总线验收、系统 UID/固件刷写产品取舍和 ComplexItem 语义编辑；这些能力按 [多机连接与焦点管理](MULTI_VEHICLE.md) 后续阶段验收。


本节的判定口径与“能不能停”见 [当前状态与完成定义](CURRENT_STATE.md)。
与 QGC/MP 的完整能力差距、项目定位和建议优先级见
[路线图与功能差距分析](ROADMAP.md)。
概览：

| 条目 | 性质 | 说明 |
| --- | --- | --- |
| `PARAM-002` | 产品决策 | 未确定覆盖率阈值，无阈值则无终止点 |
| `DEAD-001` | 产品决策 | 固件刷写需确认做或标为 out of scope |
| `VIDEO-001` | 环境阻塞 | 本机无 FFmpeg 开发环境，改动无法编译验证 |
| `JOY-001` 摇杆输入 | 暂不做 | 已调研（见下），用户决定先不实现 |

### 已完成的前端诊断与异步命令接入

Fleet 入口、明确 Vehicle 目标、批量动作、异步飞行命令 submit/poll、长任务取消和
多链路 signing 状态已经接入当前 Flutter 连接控制器。剩余工作是现场验收和后续统一
诊断信息层级，不再作为当前后端未完成项。

### `PARAM-002`：参数元数据和覆盖仍可继续增强

参数写入已经具备 pending/confirmed/failed、超时重发、目标信息、PARAM_ERROR 明确
拒绝、描述库写入校验、批量刷新重试/状态跟踪、ArduPilot 版本参数重映射（含前缀）、
未分类参数回退、canonical 覆盖率报告、高频 common 参数、常见 4.7 重映射和
Rover/Sub/Tracker profile 标识。任务领域中 Rally、GeoFence 和 ComplexItem 均已
正式建模；ComplexItem 内部字段仍为只读保留。

**阻塞**：这是无边界加深项，需要先确定覆盖率阈值才能判定完成；在阈值确定前不应
继续扩大投入。

### `DEAD-001`：固件刷写占位

`FirmwareFlasher` 仍是未接通的占位实现；`mg_firmware_flash` 诚实返回 not supported。
除非要实现完整下载和 bootloader 流程，否则不只改状态接口。

**阻塞**：属于产品取舍。要么计划实现完整流程，要么明确标为 out of scope 并关闭该条。

### `VIDEO-001`：FFmpeg 回调并发风险

启用 FFmpeg 时，`FFmpegDecoder` 的 frame callback 需要进一步做并发保护；当前默认构建
未启用 FFmpeg，因此尚未改动和验证。

**阻塞**：本机没有 FFmpeg 开发环境，改动无法编译与运行验证；需要先具备验证条件。

### `JOY-001`：摇杆 / 手柄输入（评估过，暂不做）

现状：仓库内没有任何手柄支持（`joystick` / `gamepad` / `SDL` / `hidapi` 均为零）。
已验证的上游事实：

- QGC 用 SDL2（`src/Joystick/JoystickSDL.*`），主轴发 `MANUAL_CONTROL`，辅助轴可额外映射到
  RC 5–10 走 `RC_CHANNELS_OVERRIDE`；按载具独立使能。
- MP 走 RC 通道模拟（`JoystickSetup.cs` 把轴写入 `ch1..ch16`），按钮绑定任意命令
  （`Joy_ChangeMode`、`Joy_Do_Set_Servo`、`Joy_Do_Set_Relay`、`Joy_Mount_Mode` 等）。
- ArduPilot `handle_manual_control` 会把 `MANUAL_CONTROL` 当作 GCS 心跳
  （`sysid_mygcs_seen`），因此**持续发摇杆数据会抑制 GCS 失控保护**；`RC_CHANNELS_OVERRIDE`
  的超时由 `RC_OVERRIDE_TIME` 决定，该参数为负值时 override 永不过期。

结论：属于中等工作量、且直接给飞机加输入源，安全要求高（断流必须立刻停发、默认不得发
RC override、使能要确认）。用户 2026-10-02 决定暂不实现；若日后要做，建议先只做
`MANUAL_CONTROL` + 四主轴 + 死区/反转 + 断流保护，用 SITL 验收后再扩。

### 任务规划边界

`.waypoints` 与 QGC `.plan` 的任务文件互操作已可用：`.plan` 支持普通 SimpleItem、
Home、默认速度和固件/机型元数据；ComplexItem、GeoFence、Rally 均已进入正式文档模型
并往返无损。ComplexItem 复用 QGC 存入的 `TransectStyleComplexItem.Items` 展开参与
编译/估算/上传，缺少该区段的复杂项仍以 `complex_item.unsupported` 阻断。GeoFence 与
Rally 按 QGC 编码编译，可上传、下载、回读文档并 verify（`mg_mission_doc_*_fence` /
`*_rally`）。无法无损表达的字段返回明确错误或 warnings。

仍未实现：在 M-GCS 内编辑 ComplexItem 内部字段（几何来自 QGC，当前只读），以及
围栏/Rally/ComplexItem 的统一编辑工作流。任务规划页面已接入回读验证、脏状态提示、
校验诊断定位、地图/列表选中和拖动排序。

### `REFACTOR-001` 已完成

Flutter 领域 FFI 适配器在 `0.0.20` 到 `0.0.24` 完成拆分。MAVLink adapter 在
`0.0.25` 到 `0.0.28` 完成状态、消息处理器、任务协议、参数协议、日志、命令发送和
`process()` 分发的内部拆分。`mavlink_adapter_internal.h` 当前只保留 `Impl` 状态、
方法声明和底层 `send_msg()`；后续只对新领域扩展重复该边界，不再为拆分而拆分。

## 当前工作区

提交前必须确保：

```bash
git status --short --branch
git diff --check
tools/preflight.sh
```

不要把 `build/`、`build_check/`、生成头、硬件日志、SITL 生成物和本地记忆提交。

## 架构维护结论

当前架构方向保留，不进行推倒重写。后续维护遵循以下顺序：

1. 先冻结公开 FFI 和 JSON 契约，兼容入口只修正确性，不承载新功能。
2. 先拆内部职责，再改变协议行为：优先拆 `ConnectionController` 和
   `MavlinkAdapter::Impl` 的职责，不改变现有调用方契约。
3. 任务、参数、连接和校准分别记录 MP/QGC 的选择依据；ArduPilot 语义参考 MP，
   通用状态机参考 QGC。
4. 新的“支持”必须同时有代码、聚焦测试、文档和实际平台验收记录；只有构建路径
   不能宣称平台已支持。

---

# 完成定义（什么时候可以停）

> 当前基线：`0.0.80`。本文件定义“什么时候可以停”，用来判断维护循环是否还有
> 必须做的后端工作，避免把“还能再改一点”当成“还没做完”。

## 为什么需要这份文件

M-GCS 对标 Mission Planner（MP）和 QGroundControl（QGC），功能面几乎不可能“穷尽”。
如果没有明确的收口条件，“继续优化”会变成没有终点的循环：每个版本都能再找到一件
可以做的事。

因此把“完成”拆成三层：**后端维护循环的退出条件**、**产品级 0.1.0 门禁**、
**上游对标评分**。三层各自独立判定，不互相代替。

## 第一层：后端维护循环的退出条件

满足以下全部条件，后端维护循环即可停止，转入产品验收阶段。这些条件是判断口径，
不是发布日期承诺。

| 条件 | 判定标准 | 当前状态 |
| --- | --- | --- |
| C1 复杂任务可编译 | QGC ComplexItem 不再被一刀切阻断，能展开为任务项参与编译/上传 | 已达成（`0.0.53`，复用 QGC 存入的 `TransectStyleComplexItem.Items`） |
| C2 围栏/Rally 往返闭环 | 围栏与 Rally 可编译、上传、下载、回读文档并 verify | 已达成（`0.0.48`–`0.0.52`） |
| C3 参数元数据达到约定阈值 | 需要先确定覆盖率阈值；达标即关闭，否则该条目永不“完成” | 待用户决定阈值（见 `PARAM-002`） |
| C4 明确放弃或不做的条目 | 未实现项必须显式标注为 out of scope，而不是长期挂在待办里 | 待用户决定（见 `DEAD-001`） |

判定要点：

- C1/C2 属于“行为正确性”，已通过离线与模拟测试复核，不依赖真机。
- C3/C4 不是技术阻塞，而是**产品决策**。在用户给出阈值或取舍前，后端维护循环
  不应继续为这两项做无边界加深。

## 第二层：产品级 0.1.0 门禁

产品是否“可以发 0.1.0”由 [路线图与功能差距分析](ROADMAP.md) 的阶段 E 判定，
本文不重复其内容。核心要求是：

- A–D 以及本轮承诺的多机阶段 MV 切片验收通过，或明确列出已知限制。
- 每个平台的支持声明必须等于实际验收结果；只有构建路径不等于“已验证”。
- FFI 稳定契约、弃用入口、配置迁移与发布回滚策略有测试或明确说明。
- 关键错误可诊断，数据与任务文件升级策略有覆盖。

注意：当前只有 macOS 是本机实机验证路径；Windows/Linux/Android 只完成构建与 CI
产物检查，真机验收未完成。这一层不因文档更新而改变。

## 第三层：上游对标评分

“MP/QGC 打 100 分，M-GCS 至少做到 85 分”只能按模块判定，不能整体打分，否则同样
没有止境。做法是：

1. 选定模块（如任务互操作、参数写入语义、校准状态机、平台覆盖、链路生命周期）。
2. 对照 MP/QGC 的对应实现，列出差距条目和判定依据。
3. 逐条判定“已达到 / 明确不作为目标 / 受资源阻塞”。
4. 只有在所有适用模块都落到上述三类之一时，该层才算完成。

当前参考记录：[QGC 代码质量审计](audits/2026-10-01-qgc-quality-audit.md)、
[QGC/MP 可借鉴项对照](../specs/upstream/SELECTION_STRATEGY.md)、
[MP / QGC 上游实现择优准则](../specs/upstream/SELECTION_STRATEGY.md)。

## 剩余项与阻塞原因

以下条目来自 [现状审计](CURRENT_STATE.md) 的“当前剩余项”，此处只记录阻塞性质。多机入口、明确目标命令、异步命令轮询和 signing route 隔离已不再列为未完成项。

| 条目 | 性质 | 阻塞原因 |
| --- | --- | --- |
| `PARAM-002` 参数元数据加深 | 产品决策 | 未确定覆盖率阈值；无阈值则无终止点 |
| `DEAD-001` 固件刷写 | 产品决策 | 完整下载 + bootloader 流程是大工程；需确认做或标为 out of scope |
| `VIDEO-001` FFmpeg 回调并发 | 环境阻塞 | 本机无 FFmpeg 开发环境，改动无法编译验证 |
| Windows/Linux/Android 真机验收 | 资源阻塞 | 需要对应平台设备与飞控，飞控串口为排他资源 |

不属于“未完成”的条目：

- `CI-001` 远端 CI：按用户要求标记为 WONTFIX，不再跟踪。

## 标注要求

本文档与相关审计中判断“参考了谁”时，保持已确立的择优准则：

- ArduPilot 专用语义（参数、校准、模式、任务字段、现场兼容）优先参考 MP。
- 框架、生命周期、状态机、任务模型、跨平台结构与测试治理优先参考 QGC。
- 协议事实（MAVLink 消息、枚举、单位、任务协议）以官方定义为准。

引用具体上游实现时写明模块与理由，例如围栏 mission 编码指
QGC `GeoFenceManager`、ComplexItem 展开指 QGC `TransectStyleComplexItem::_save`、
Rally 编码指 QGC `RallyPointManager`。详细规则见
[../specs/upstream/SELECTION_STRATEGY.md](../specs/upstream/SELECTION_STRATEGY.md)。

## 使用方式

- 评估“还能不能继续优化”时，先对照第一层的 C1–C4，再看第二、三层。
- 第一层未达成项若属于产品决策或环境阻塞，应先向用户确认，而不是自行选择方向。
- 新增的未完成项必须同时写进本文件和 [现状审计](CURRENT_STATE.md) 的“当前剩余项”。
