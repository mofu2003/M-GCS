# M-GCS 全量代码与接口盘点

盘点日期：2026-10-12　版本：0.2.2　分支：main
口径：以工作区实际文件为准（非文档），逐项验证可达性。

---

## 0. 总览

| 层 | 内容 | 规模 |
|---|---|---|
| 前端 | Flutter/Dart，`flutter_shell/lib` | 30 个页面/面板 + 9 控制器 + 17 后端 + 20 组件 |
| 桥接 | C ABI，`bridge/ffi` | 11 个公开头，**154** 个导出（127 已绑定）+ 2 个公开类型 |
| 核心 | C++17，`core/` | 15 个模块 |
| 第三方 | `third_party/` | MAVLink 生成头、cJSON（禁止修改） |
| 测试 | 41 个套件 | 32 `core/tests` + 5 `tests` + 4 `tests/manual`；默认注册进 CTest 的 34 个 |
| 工具 | `tools/` | 19 个脚本 + 2 份基线 JSON（6 个进 preflight 门禁） |

依赖方向：`flutter_shell → bridge/ffi → core → third_party`（单向，架构 A1 由编译期强制）。

---

## 1. 公开 FFI 接口（154 个导出，按头文件）

### 1.1 `ffi_api_core.h` — 生命周期与操作（11 导出）

| 导出 | 功能 | 绑定 |
|---|---|---|
| `mg_create` / `mg_destroy` | 核心实例生命周期 | Dart |
| `mg_free_string` | 释放 `char*` 返回值（F3 所有权规则） | Dart |
| `mg_operations_get_snapshot` | 所有进行中操作的一致快照 | Dart |
| `mg_operations_cancel` | 按 operation_id 取消 | Dart |
| `mg_operations_cancel_for_vehicle` | 按 (vehicle, operation) 定向取消 | Dart |
| `mg_system_get_diagnostics` | 系统级诊断聚合 | 打包校验 |
| `mg_error_string` | 错误码 → 常量字符串 | 内部能力 |
| `mg_get_last_error` / `mg_set_last_error` / `mg_clear_last_error` | 线程局部错误码 | ABI 基础设施 |

**公开类型**：`mg_core_t`（不透明句柄）、`mg_error_t`（17 个错误码枚举）。

### 1.2 `ffi_api_connection.h` — 连接与多机（19 导出）

| 导出 | 功能 | 绑定 |
|---|---|---|
| `mg_connection_open` | 按 URI 开链路（串口/UDP/TCP），返回 link_id | Dart |
| `mg_connection_close` | 关单条链路或车辆会话 | Dart |
| `mg_connection_close_route` | 只关一条 (link, vehicle) 路由，保留共享链路 | Dart |
| `mg_connection_list` | 链路与已发现车辆路由 | Dart |
| `mg_connection_list_ports` | 枚举本机串口 | Dart |
| `mg_connection_get_session_state` | 指定会话的连接/心跳状态 | Dart |
| `mg_connection_get_diagnostics` | 连接诊断（心跳存活、RTT 等） | 打包校验 |
| `mg_stream_set_interval` | 设置 MAVLink 消息流频率 | Dart |
| `mg_vehicle_list` | 车辆清单（含心跳新鲜度、通信丢失标记） | Dart |
| `mg_vehicle_select` | 切换 active 焦点 | Dart |
| `mg_vehicle_get_active` | 读当前 active vehicle id | Dart |
| `mg_vehicle_open` | 按 URI 直接开单机会话 | 内部 |
| `mg_vehicle_get_session_mission` | 某车的任务快照 | Dart |
| `mg_vehicle_get_signing_status` | MAVLink 签名状态 | Dart |
| `mg_vehicle_set_signing_key` / `mg_vehicle_clear_signing_key` | 本机密钥库读写 | Dart |
| `mg_vehicle_set_signing_candidates` / `mg_vehicle_clear_signing_candidates` | 候选密钥管理 | Dart |
| `mg_vehicle_send_setup_signing` | 下发 `SETUP_SIGNING` | Dart |

### 1.3 `ffi_api_telemetry.h` — 遥测（16 导出）

| 导出 | 功能 | 绑定 |
|---|---|---|
| `mg_telemetry_get_dashboard_snapshot` | 仪表盘聚合（姿态/GPS/电池/位置/VFR/健康） | 打包校验 |
| `mg_telemetry_get_freshness` | 各遥测项的新鲜度 | Dart |
| `mg_telemetry_get_messages` | 增量消息流（按 seq 拉取） | Dart |
| `mg_vehicle_get_summary` | 心跳与机载信息（固件/板卡/产品名） | Dart |
| `mg_vehicle_get_attitude` | 姿态（roll/pitch/yaw + 角速度） | Dart |
| `mg_vehicle_get_global_position` | 全局位置 | Dart |
| `mg_vehicle_get_home_position` | Home 点 | Dart |
| `mg_vehicle_get_vfr_hud` | 空速/地速/高度/油门/航向 | Dart |
| `mg_vehicle_get_extended_sys_state` | 着陆状态、VTOL 状态 | Dart |
| `mg_vehicle_get_firmware_version` | 飞控版本（含 `MAV_PROTOCOL_CAPABILITY`） | Dart |
| `mg_vehicle_get_session_status` | 某车的会话状态 | Dart |
| `mg_rc_get_channels` | RC 通道原始值 | Dart |
| `mg_servo_get_outputs` | 舵机 PWM 输出 | Dart |
| `mg_sensors_get_health` | 传感器健康位（present/enabled/healthy） | Dart |
| `mg_waveform_get_instances` | 波形实例清单（各 IMU/磁力计） | Dart |
| `mg_waveform_poll` | 取波形采样点（类型化 `float*`，非 JSON） | Dart |

### 1.4 `ffi_api_flight.h` — 飞行指令与模式（18 导出）

| 导出 | 功能 | 绑定 |
|---|---|---|
| `mg_command_submit_for_vehicle` | **唯一生产入口**：提交命令，返回 ticket | Dart |
| `mg_command_poll` | 轮询 ticket 结果（ACK 接受才算成功） | Dart |
| `mg_flight_get_modes` | 按机型给模式表 + 当前模式 + 心跳新鲜度 | Dart |
| `mg_command_execute_for_vehicle` | 批量场景下按车定向执行 | 内部 |
| `mg_command_arm` / `mg_command_disarm` | 解锁/上锁（`force` 走 param2=21196 急停语义） | 内部 |
| `mg_command_takeoff` / `mg_command_land` / `mg_command_rtl` | 起飞/降落/返航 | 内部 |
| `mg_command_set_mode` | 按 base_mode + custom_mode 切模式 | 内部 |
| `mg_command_set_home` / `mg_command_set_global_origin` | 设 Home / 设估计原点 | 内部 |
| `mg_command_orbit` / `mg_command_set_roi` | 环绕 / 兴趣点 | 内部 |
| `mg_command_guided_point` / `mg_command_change_altitude` / `mg_command_change_heading` | 引导点 / 改高度 / 改航向 | 内部 |
| `mg_command_guided_pause` | 暂停（ArduPilot 走模式切换） | 内部 |

> 「内部」= 由 `mg_command_submit_for_vehicle` 的 worker 解析 JSON 后实际调用。前端不直接绑定。

### 1.5 `ffi_api_mission.h` — 任务与围栏（33 导出）

| 分组 | 导出 | 功能 |
|---|---|---|
| 文档生命周期 | `mg_mission_doc_create`/`get`/`apply`/`load`/`save` | 任务文档 CRUD 与落盘 |
| 编辑 | `mg_mission_doc_undo`/`redo` | 撤销/重做 |
| 校验与估算 | `mg_mission_doc_validate`/`estimate` | 校验报告与航程/用时估算 |
| 编译 | `mg_mission_doc_compile` | 文档 → 传输计划 |
| 传输 | `mg_mission_doc_upload`/`mg_mission_download`/`mg_mission_clear` | 上传/下载/清除（带 revision 防竞态） |
| 核对 | `mg_mission_doc_verify` | 本地计划与飞控实际比对 |
| 交换 | `mg_mission_doc_export`/`export_format`/`import`/`import_format` | JSON / `waypoints`(QGC WPL 110) / QGC `.plan` |
| 飞控回读 | `mg_mission_doc_import_from_vehicle` | 从飞控重建文档 |
| 围栏 | `mg_mission_doc_{{compile,upload,clear,verify}}_fence`、`mg_mission_doc_import_fence_from_vehicle`、`mg_fence_download` | 围栏全生命周期 |
| 集结点 | `mg_mission_doc_{{compile,upload,clear,verify}}_rally`、`mg_mission_doc_import_rally_from_vehicle`、`mg_rally_download` | Rally 全生命周期 |
| 目录与进度 | `mg_mission_get_command_catalog`、`mg_mission_get_execution_progress` | 可编辑命令目录 / 执行进度 |

### 1.6 `ffi_api_params.h` — 参数（7 导出，全部绑定）

`mg_params_request_all`（全量请求）、`mg_params_get_all`（快照）、`mg_params_get_by_name`、`mg_params_get_progress`（下载进度）、`mg_config_read_param`（含描述符元数据）、`mg_config_write_param`（写入并回读校验）、`mg_params_get_write_status`、`mg_config_get_param_roles`（**语义角色表**，S6：通用代码按角色取固件参数名）。

### 1.7 `ffi_api_configuration.h` — 校准与串口（6 导出，全部绑定）

`mg_calibration_start`（陀螺/加速度/罗盘/气压/遥控）、`mg_calibration_cancel`（**异步确认**：等 `COMMAND_ACK` 才落 `cancelled`，S8）、`mg_calibration_accel_confirm`（六面）、`mg_calibration_get_status`、`mg_config_get_param_roles`、`mg_serial_get_uart_names`（读飞控 `@SYS/uarts.txt` 得 SERIALn→UART 真实映射）。

### 1.8 `ffi_api_logging.h` — 日志与文件（18 导出，全部绑定）

| 分组 | 导出 |
|---|---|
| 机载 DataFlash | `mg_onboard_log_{{request_list,get_entries,download,download_status,download_cancel,erase}}` |
| 链路 tlog | `mg_tlog_{{start,stop,status}}` 录制；`mg_tlog_replay_{{open,step,seek,close}}` 离线回放 |
| MAVLink FTP | `mg_fs_{{list_directory,download_file,upload_file,transfer_status,cancel}}`（异步状态机） |

### 1.9 `ffi_api_payload.h` — 相机/云台/视频（23 导出，全部绑定）

| 分组 | 导出 |
|---|---|
| 相机 | `mg_camera_{{request_info,get_status,take_photo,set_mode,set_zoom,start_video,stop_video,stop_capture}}` |
| 云台 | `mg_gimbal_{{request_info,get_status,get_attitude}}`、`mg_command_gimbal_{{control,recenter}}` |
| 视频 | `mg_video_{{start,stop,status,get_streams}}`、`mg_video_poll_frame`（RGBA 帧取用）、`mg_video_capture_frame`（截图） |
| 录制 | `mg_video_{{start_recording,stop_recording,recording_status}}`、`mg_video_renderer_status` |

### 1.10 `ffi_api_firmware.h` — 固件识别（3 导出，全部绑定）

`mg_firmware_get_type`（固件类型 + capability 位 + 最小起飞高度）、`mg_vehicle_get_firmware`、`mg_vehicle_request_firmware_version`。

### 1.11 未绑定导出（27 个）的定位

| 类别 | 数量 | 说明 |
|---|---|---|
| 命令族 | 15 | `mg_command_*`，由 submit/poll worker 内部调用 |
| ABI 基础设施 | 4 | 错误码读写与字符串化 |
| 打包校验必需 | 3 | 诊断与仪表盘聚合，`tools/ffi_bundle_symbols.dart` 要求可解析 |
| 任务文档 JSON / 围栏·Rally 编译 | 4 | `mg_mission_doc_export`·`import`·`compile_fence`·`compile_rally`：前端曾有 Dart 包装但零调用（导入导出走文件与交换格式，围栏/Rally 下发内部自行编译），2026-10 删掉包装后由原生测试覆盖 |
| 其它 | 1 | `mg_vehicle_open`（连接实现路径） |

---

## 2. Core 模块（15 个）

| 模块 | 文件 | 职责 | 对外接口 |
|---|---|---|---|
| `transport` | 10 | 链路抽象与实现：串口（含 Android USB）、TCP、UDP、内存链路；连接状态机；链路配置与工厂；串口枚举 | `ILink`、`ConnectionManager`、`LinkSession`、`SerialPortDiscovery` |
| `mavlink` | 13 | MAVLink 适配器（拆为 command/dispatch/handlers/log/mission/params 六个实现文件）、命令队列与 ACK 匹配、消息路由、发送器、任务传输状态机、参数缓存 | `MavlinkAdapter`、`CommandQueue`、`MissionTransfer`、`ParamCache`、`MavlinkRouter` |
| `vehicle` | 3 | 车辆聚合（遥测缓存 + 子系统）、多机管理（共享链路 + 路由）、链路健康（心跳、通信丢失） | `Vehicle`、`MultiVehicleManager`、`VehicleLinkManager` |
| `mission` | 9 | 任务文档模型（CAD）、校验、编译、估算、命令目录、QGC `.plan` 与 `waypoints` 互换、Haversine 距离 | `MissionDocumentService`、`MissionDocument`、`MissionEntity`、`geo_math` |
| `calibration` | 7 | 校准服务（异步状态机 + 取消确认）、校准后校验、5 种策略（陀螺/加速度/罗盘/气压/遥控） | `CalibrationService`、`ICalibrationStrategy` |
| `firmware` | 2 | 固件识别与适配、ArduPilot 插件（模式表按机型、capability、引导语义、模式角色解析） | `FirmwareAdapter`、`IFirmwarePlugin`、`ArduPilotPlugin` |
| `config` | 1 | 参数描述符库（含中文翻译与语义角色）；参数文件内容变换与变更集已随导出退役删除 | `ParamDescriptors` |
| `telemetry` | 2 | 遥测环形缓冲、IMU 波形缓冲 | `TelemetryRingBuffer`、`WaveformBuffer` |
| `settings` | 2 | 飞行日志服务、MAVLink 原始日志写入（`FlightLogWriter` 无任何调用方，已删） | `FlightLoggingService`、`MavlinkLogWriter` |
| `ftp` | 1 | MAVLink FTP 异步客户端（单会话状态机） | `MavlinkFtpClient` |
| `log` | 1 | `.tlog` 离线回放 | `FlightLogReplayer` |
| `sensor` | 1 | 传感器设备 ID 解码（ArduPilot 位段 → 型号名） | `SensorIdDecoder` |
| `flight` | 1 | 飞行模式管理（模式表、角色解析、状态跟踪） | `FlightModeManager` |
| `video` | 1 | 本地视频解码器（FFmpeg LGPL，可选编译） | `VideoDecoder` |
| `platform` | 3 | 文件系统抽象（Android / POSIX / Win32）、互斥与条件变量 | `platform::*` |


---

## 3. 前端（Flutter）

### 3.1 顶层导航

**顶栏**（`widgets/mg_top_status_bar.dart`）：机型图标/车辆选择、批量控制面板、飞控消息、解锁/上锁（长按 500ms）、急停（长按）、飞行模式菜单、设置。

**设置菜单**（`MgSettingsDestination`）→ 目标：

| 菜单项 | 目标 |
|---|---|
| 连接飞控 | `ConnectionFloatingPanel` |
| 参数配置 | `ConfigurationWorkspace`（默认 `motors`） |
| 航线规划 | `MissionPlanningFloatingPanel` |
| 相机与云台 | 配置页 `payload` |
| 机载日志 | 配置页 `logs` |
| 飞行记录 | 配置页 `flightLog` |
| 机载文件 | 配置页 `files` |
| 地面站配置 | `GcsConfigurationWorkspace`（地图来源 / MAVLink 签名） |

**配置工作区**（`configuration/configuration_workspace.dart`，13 页）：

`system` 系统状态 · `calibration` 飞控校准 · `ports` 端口配置 · `motors` 电机输出 · `flightModes` 飞行模式 · `pid` PID 调参 · `battery` 电池配置 · `arming` 解锁检查 · `allParameters` 全部参数 · `payload` 相机与云台 · `logs` 机载日志 · `flightLog` 飞行记录 · `files` 机载文件

### 3.2 页面与面板（30）

| 区域 | 页面 |
|---|---|
| 主界面 | `main.dart`（地图主页）、`map/amap_map_view.dart`（地图视图与工具列） |
| 任务 | `mission/mission_planning_panel.dart`（规划浮层）、`mission/mission_transfer_section.dart`（传输动作条） |
| 连接 | `connection_panel.dart`（连接浮层 + 机队链路列表） |
| 日志 | `logging/onboard_log_page.dart`、`logging/flight_log_page.dart`、`logging/file_browser_page.dart` |
| 载荷 | `payload/camera_gimbal_page.dart`、`payload/video_preview.dart` |
| 配置页 | 上表 13 页（含 `configuration/calibration/` 下 3 个视图、`configuration/pid/` 下 2 页） |
| 消息 | `flight/flight_message_panel.dart`（STATUSTEXT） |
| GCS | `gcs_configuration_workspace.dart`（地图来源 / 签名） |
| 开发 | `demo/component_gallery_demo.dart`（画廊，不进 release） |

### 3.3 控制器（9）

`ConnectionController`（**核心**：连接/遥测/参数/校准/飞行命令状态机）、`SigningController`、`WaveformController`、`FleetController`、`FileController`、`LogController`、`TlogController`、`MissionDocumentController`、`PayloadController`

### 3.4 后端（17，Dart FFI 边界）

`NativeCore`（唯一持有 `DynamicLibrary` 与 `mg_core_t` 句柄，架构 A5）+
6 对「抽象接口 + FFI 实现」：`FleetBackend`、`FileBackend`、`LogBackend`、`TlogBackend`、`MissionDocumentBackend`、`PayloadBackend` +
4 个直接 FFI 后端：`CalibrationFfiBackend`、`FlightCommandFfiBackend`、`ParameterFfiBackend`、`TelemetryFfiBackend`

### 3.5 组件（20）

**通用**（`widgets/`，19）：确认类（`mg_command_confirmation`、`mg_confirm`、`mg_dialog`）、面板容器（`mg_floating_panel`、`mac_panel`、`app_workspace_shell`）、读数与仪表（`mg_telemetry_dashboard`、`mg_motor_output_bar`、`mg_animated_number`、`mg_compass`、`mg_mac_glass_attitude_panel`、`mg_top_status_bar`）、表单（`aligned_field_row`、`adaptive_popup_button`、`parameter_load_progress`）、其它（`mg_inline_notice`、`mg_map_action_menu`、`mg_settings_menu`、`mg_fleet_batch_panel`）

**波形**（`components/`，1）：`realtime_waveform.dart`

**配置页专用**（`configuration/**/widgets/`，19）：参数控件族、校准卡片族、电机输出单元格族、PID 卡片族、系统状态卡片族

---

## 4. 测试（41 个套件）

| 类别 | 数量 | 覆盖 |
|---|---|---|
| `core/tests/` | 32 | 单元 + 模拟（`test_sim_*` 用 `MockFlightController` 跑完整 FFI 路径） |
| `tests/` | 5 | 跨模块契约与全流程（`test_ffi_response_contract`、`test_mp_qgc_features`、`test_real_fc_dump`、`test_scan_connect`、`test_reconnect`） |
| `tests/manual/` | 4 | 真实飞控 / SITL 手工验证（默认不注册进 CTest） |

默认 `ctest` 注册 34 个（`offline` + `simulation` 标签）；`hardware`/`manual` 标签的 7 个
（4 个 manual + `test_real_fc_dump` + `test_scan_connect` + `test_reconnect`）需显式开启
`MGCORE_REGISTER_HARDWARE_TESTS` 才会跑，避免误连真机。

代表：`test_ffi_contract`（ABI 契约）、`test_bridge_vehicle_session`（多机会话）、`test_mission_document`（任务文档）、`test_mavlink_protocol`（协议层）、`test_sim_ui_paths`（前端调用路径）、`test_bridge_architecture_guards`（架构 A1/A2 文本扫描）

---

## 5. 门禁与工具（21）

**preflight 门禁链**（`tools/preflight.sh`，6 项）：

| 脚本 | 检查 |
|---|---|
| `check_api_change_set.py` | F1：改 `ffi_api*.h` 必须成套（头+实现+测试+目录+文档） |
| `check_ffi_contract.py` | 声明 ↔ Dart 绑定 ↔ 编译库导出三者一致；未绑定导出必须有定位 |
| `check_fake_assertions.py` | T2：禁止 `CHECK(true)` 之类假断言 |
| `check_firmware_literals.py` | S6：通用代码里不得出现固件专属参数名字面量 |
| `check_ui_tokens.dart` | UI 必须走设计 token，不得硬编码颜色/字号 |
| `ffi_bundle_symbols.dart` | 打包产物必须导出必需的 ABI 符号 |

**其它工具**：`generate-mavlink.sh`（生成 MAVLink 头）、`release.sh`、`sanitize.sh`（ASan/TSan）、`sitl-mission-scenario.sh`（SITL 端到端证据）、`run-hardware-test.ps1`、`check_linux_bundle.dart` / `check_windows_bundle.dart`、`docker-dev.sh`、`install-hooks.sh`

---

## 6. 盘点发现的死代码（3 处，已清理）

| # | 内容 | 规模 | 证据 |
|---|---|---|---|
| 1 | `bridge/ffi/ffi_api_environment.h` | 15 行 | **0 个导出**（SITL 删除后成空壳）。只因 `ffi_api.h` 的 include 与架构守卫测试的名单而存在；已不在 API 目录里（目录只列有导出的头） |
| 2 | `core/sitl/`（`SitlLauncher`） | 291 行 | 类从未被实例化；`mg_core_t::sitl` 成员**从未被访问**（`mg_sitl_*` 导出已删）。`test_sitl_mission_scenario.cc` 是从外部驱动 SITL，不用这个类 |
| 3 | `core/telemetry/adsb_tracker.{h,cc}`（`ADSBTracker`） | 193 行 | 类从未被实例化；MAVLink 层有 `on_adsb_vehicle` 回调（有测试），但没有任何代码把它接进 tracker |

**处置（第 4 轮）**：三处已删除（与前三轮口径一致）。同步改动：`ffi_api.h` 去掉 include、
`ffi_core_context.h` 去掉 `sitl` 成员与 include、`test_bridge_architecture_guards.cc`
的头文件名单、`CMakeLists.txt` 的两条源文件、API 能力表里 `ffi_api_environment.h` 的
陈规定位。**注意**：这三处不产出 FFI 符号（空头 + 两个从未实例化的内部类），
所以公开导出数量不变（仍 154）。

### 6.1 第 4 轮复查（删除后的孤儿代码）

前几轮删掉导出后，一批**只服务那些导出的实现**失去调用方却留了下来。本轮按
「函数体级可达性分析 + 编译器 `-Wunused` 告警」两条路交叉确认并清除：

| 位置 | 内容 |
|---|---|
| `ffi_commands.cc` | `export_connected_plan_json`（原 `mg_fence/rally_get_plan` 用） |
| `ffi_config.cc` | `infer_param_type`、`other_parameter_group`、`param_desc_json`、`config_read_group_json`、`kOtherParameterGroup`（原参数分组接口用） |
| `ffi_params.cc` | `parse_change_set`、`change_set_json`、`disposition_name`（原变更集接口用） |
| `ffi_helpers.h` | `cal_result_json`、`cal_history_json`、`cal_retry_json` |
| `ffi_internal.h` | `get_active_ring_buffer`、`ffi_set_param`、`ffi_fmt_param` |
| `ffi_serial.cc` | `serial_port_name_from_param` + 3 张串口参数表（原 `mg_serial_get_configs` 用） |
| `mission_json_serializers.h` | `mission_progress_json` |
| `mavlink_adapter.h` | `MissionCurrentInfo` 结构、`command_outcome_name` |
| 整文件删除 | `mission_json_parser.h`、`sensor_decode_json_serializers.h`、`config_descriptors.h` |

同时补回 4 处**功能仍在、测试却没了**的覆盖缺口（详见 `CHANGELOG.md`）：
`mg_telemetry_get_messages`（前端消息面板的唯一数据源，此前零覆盖）、
`mg_error_string` 的 F3 所有权契约、校准 MGCAL1 落盘的 wall-clock 时间戳、
硬件工具里退化成恒真的健康就绪门。

**一处方法论提醒**：可达性分析若只匹配 `name(`，会漏掉**函数指针与模板实参**
（如 `TelemetryGroup<..., gps_json>` 里没有括号）。本轮的 JSON 序列化器就是
这样被误判成死代码的——务必用裸名 `name` 复核。

### 6.2 第 5 轮复查（2026-10-12，删除后的孤儿代码）

第 4 轮之后又删了 108 个导出，包括测绘、地形、围栏独立模块等。
事件转发链（`e760aac`）与参数文件 I/O（`4c311f3`），连带留下一批「只服务那些能力」
的实现。本轮按裸名全仓计数（**注释与字符串先剥离**，并单独复核函数指针/模板实参）逐条确认后清除：

| 位置 | 内容 |
|---|---|
| 整类删除 | `FlightLogWriter`（`core/settings/flight_log_writer.{h,cc}`，11 个方法）——唯一「使用」是 `mg_core_t` 里一个从没被调用的成员 |
| 整文件改名 | `mission_item_factory.{h,cc}` → `geo_math.{h,cc}`：四个静态 builder 全部无调用方，只剩 `haversine_distance`，遂去掉那个名不副实的工厂结构体 |
| `mission` | `MissionItemFactory::create_waypoint/create_rtl/create_speed_change/create_camera_trigger` |
| `flight` | `FlightModeManager::is_armed/decode_mode_name/set_takeoff_altitude/takeoff_altitude/set_loiter_radius/loiter_radius` + 两个只被它们读的字段 |
| `mavlink` | `on_distance_sensor`、`on_video_stream_info`、`param_refresh_status`、`camera_information`、`gimbal_manager_information`、`rally_get_plan`、`fence_get_plan`、`video_request_stream_info`、`cmd_roi_stop`、`StatustextInfoCallback` |
| `transport` | `SerialPortDiscovery` 的 `refresh`/`best_candidate`/`is_port_available`/`filter_ports` + `PortFilter` 枚举；`link_config.h` 的 `high_latency` |
| `vehicle` | `MultiVehicleManager::init` 与两个回调 setter（连带删掉 4 处只会调用空回调的派发逻辑）、`VehicleInfo` 的 5 个传感器名字段、`flight_time_s()` 及其累加 |
| `settings`／`ftp`／`log` | `FlightLoggingService::set_write_callback`、`MavlinkFtpClient::download_path_`、`FlightLogReplayer::replay_all` |
| `ffi` | `mg_core_t` 的 `core_mtx`/`log_events`/`kMaxLogEvents`/`log_writer` 四个死成员；`ffi_config_extra.cc` 残留的函数横幅；`ffi_api_flight.h` 里指向已删导出的四段悬空注释 |
| 测试内 | `test_mission_document.cc` 三个未用 helper、`test_sitl_mission_scenario.cc` 的 `ok_data`、两个硬件工具里未用的 `call_json_int`、`test_param_descriptors.cc` 三个未用局部变量、`mock_flight_controller.h` 的 `kFtpPayload` |
| 工具 | `tools/audit_status.sh`（**已删除**；功能已被 `check_ffi_contract.py --write-catalog` 完整取代）；`firmware_literals_baseline.json` 中指向已删 `param_file_io.cc` 的 3 条僵尸记录 |

同时修掉两处**编译能过但语义错**的地方：`test_ffi_comprehensive.cc` 用
`state_string(...) == "disconnected"` 比较 `const char*`（比的是指针，靠字面量合并
碰巧通过——正是 T2 说的假断言），以及 10 处重定义 `CJSON_NESTING_LIMIT` 的无效
`#define`（cJSON 是独立编译单元，这些定义从来没生效过，其中 2 处还触发重定义告警）。

**刻意保留**：`event_codes.h` 里暂时无人触发的事件码——该头文件写明数值会出现在
事件 JSON 里，属 append-only 契约，删中间项会让后续码位整体前移；同理保留
`mavlink_ftp.h` 的协议操作码/结果码表、`calibration_types.h` 的校准命令常量、
`ModeResult`/`CoordFrame`/`NavCommand` 等域枚举——枚举成员未被使用不等于它是垃圾，
删掉它们只是把领域事实换成裸数字。

---

## 7. 已知偏差台账（`known-deviations.md`）

当前登记的活跃条目（按性质）：

| 类别 | 条目 |
|---|---|
| UI 约束 | KD-36（画廊豁免 U3）、KD-40（不做 golden 截图） |
| 架构建议 | KD-27（**采用轮询**，事件推送 ABI 已删）、KD-47（`JsonMap` typedef 层次倒挂，接受） |
| 门禁范围 | KD-45（FFI 契约守卫，存量已分类）、KD-50（S6 只拦参数名，数值型固件值待立项） |
| 合规 | KD-58（枚举标签暂留，待决定） |

---

## 8. 与前次盘点的差异

| 项 | 变化 |
|---|---|
| 公开导出 | 280 → **154**（-126） |
| 测绘 / 围栏独立模块 / 地形 | 全部删除 |
| FFI 事件推送 | 删除（采用轮询，KD-27 已同步） |
| 空壳源文件 | 删除 9 个（`ffi_actuator` / `ffi_health` / `ffi_logging` / `ffi_rc_config` / `ffi_sensor_decode` / `ffi_sitl_commands` / `ffi_vehicle_info` / `ffi_preflight_commands` / `ffi_flight_mode_state`） |
| API 目录 | 新增「公开类型」段（此前 typedef 是盲区） |
| 第 4 轮复查新增清理 | 见 §6.1 |
| 累计 | 231 文件变更，+903 / −19040 行 |
