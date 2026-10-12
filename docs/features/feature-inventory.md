# 功能清单与交付分类（Feature Inventory）

状态：核对日期 2026-10-11，对应 `v0.2.1`（main）。
来源：`docs/plans/mgcs-development-plan-2026-10-10.md` 阶段 0 的交付口径；`docs/capabilities.md` 的模块状态表；`bridge/ffi/API_CATALOG.md` 的导出/绑定明细。

本文件的用途是把「有导出」「有原生实现」「有 UI 入口」「用户能用」这几件事分开讲清楚，回答三类问题：
**已经能用的功能有哪些、还有什么没做、哪些 FFI 导出没接前端以及为什么。** 它不是需求文档，也不替代 `docs/features/` 下的方向简报。

口径（与 `docs/capabilities.md` 一致）：

- **已交付**：主要工作流有实际 UI 入口，成功/失败/超时/断链都有定义，且有对应测试。
- **部分接入**：模块内一部分能力已进 UI，其余仍是内部接口或仅测试覆盖。
- **待接入**：能力已在 FFI 暴露且有原生实现，但尚未提供 UI 入口。
- **延期至 0.3.0 之后**：保留原生实现与 ABI，当前版本明确不接入 UI。
- **内部能力**：只服务开发、离线分析或原生测试，刻意不进入业务 UI。
- **退役候选**：暂保留 ABI，尚待确认仓库外兼容性与独立用途后再决定是否删除。
- **已删除（历史遗留）**：被更新接口取代、且确认无未来用途的历史导出，已从源码与本轮 ABI 删除，并登记进 `RETIRED_SYMBOLS`（当前无此类导出，见 §4.1）。

底层统计（`python3 tools/check_ffi_contract.py`）：公开导出 **280** 个，前端已绑定 **136** 个，未绑定 **144** 个；其中内部能力 **17** 个、0.3.0+ 延期 **10** 个、退役候选 **6** 个、仅测试引用 **111** 个，当前没有未定性债务。0.2.1 已删除 34 个无调用方、无未来用途的历史导出，见 §4.1 与 `CHANGELOG.md`。

## 1. 已实现功能（已交付，用户可见）

按前端工作区/面板分组，入口文件见括号。

### 1.1 连接与链路（`flutter_shell/lib/connection_panel.dart`）

- 串口（枚举本机 UART 名）、UDP、TCP 连接与断开。
- 本机端口枚举（`mg_connection_list_ports` + `mg_serial_get_uart_names`）。
- 会话状态、链路状态栏、心跳新鲜度展示。
- 本地回环校验入口。

### 1.2 多机与机队（`flutter_shell/lib/fleet/`）

- 车辆列表、活动车辆选择与切换（`mg_vehicle_list`/`mg_vehicle_select`/`mg_vehicle_get_active`）。
- 每车会话状态与任务快照（`mg_vehicle_get_session_status`/`mg_vehicle_get_session_mission`）。
- 批量操作面板（`widgets/mg_fleet_batch_panel.dart`）。

### 1.3 飞行操作与指令（`flutter_shell/lib/main.dart`、`connection/flight_command_ffi_backend.dart`）

- 解锁/上锁、起飞、返航（RTL）、降落、模式切换、引导类指令等，统一走
  `mg_command_submit_for_vehicle`（提交）+ `mg_command_poll`（轮询结果），无 ACK 命令不当作成功（KD-01/KD-02）。
- 飞行模式表读取（`mg_flight_get_modes`）与模式映射。
- 危险操作前有确认对话框（`widgets/mg_command_confirmation.dart`）。

### 1.4 遥测与仪表（`widgets/mg_telemetry_dashboard.dart`、`widgets/mg_top_status_bar.dart`）

- 姿态、GPS、电池、高度、地速、航向等实时量与顶栏状态。
- 聚合读数：`mg_telemetry_get_dashboard_snapshot`（姿态 / GPS / 电池 / 位置 / VFR / 传感器健康）、`mg_vehicle_get_summary`（心跳与机载信息）、`mg_vehicle_get_{{attitude,global_position,home_position,vfr_hud,extended_sys_state,firmware_version}}`。
- RC 与舵机：`mg_rc_get_channels`、`mg_servo_get_outputs`；传感器健康：`mg_sensors_get_health`。
- 实时波形（`components/realtime_waveform.dart` + `mg_waveform_*`）。
- **未接线（诚实标注）**：ADS-B 与距离传感器。MAVLink 层有 `ADSB_VEHICLE` 处理与 `on_adsb_vehicle` 回调，但 `telemetry/adsb_tracker.{h,cc}` 的 `ADSBTracker` **在仓库内从未被实例化**（FFI 导出已删除），也没有距离传感器的 FFI 入口。

### 1.5 地图（`flutter_shell/lib/map/amap_map_view.dart`）

- 多种底图源：标准、地形、高德标准/卫星/卫星+路网，以及自定义 XYZ 瓦片（带校验，KD-37）。
- GCJ-02 坐标换算、飞行器标绘、任务/围栏叠加、引导点标记（命令成功后才更新）。
- 地图偏好持久化（缩放、中心、底图）与数据来源署名。

### 1.6 任务规划与交换（`flutter_shell/lib/mission/`）

- 文档模型（`mg_mission_doc_*`）：创建、编辑、校验、估算、上传、回读、比对（verify）。
  撤销/重做在 core、FFI 与 Dart 控制器（`MissionDocumentController.undo/redo`）三层都已就绪，
  **界面尚未接线**（见 §3.1）。
- 航点编辑、地图绘制、上传/清除带统一确认（KD-08）。
- 文件互操作：`waypoints`（MP/QGC 的 QGC WPL 110 文本）与 `plan`（QGC JSON）导入导出。
- 任务命令目录、执行进度（`mg_mission_get_command_catalog`、`mg_mission_get_execution_progress`）。

### 1.7 围栏与 Rally（`mission/mission_planning_panel.dart`）

- 经任务文档区段下发/回读/比对围栏与 Rally（`mg_mission_doc_*_fence/rally`、`mg_fence_download`、`mg_rally_download`）。

### 1.8 参数（`flutter_shell/lib/configuration/parameters/all_parameters_page.dart`）

- 参数全量请求、按名读取、写入与写进度（`mg_params_request_all`/`get_all`/`get_by_name`/`get_progress`/`get_write_status`）。
- 按语义角色取值（`mg_config_get_param_roles`，safety.md S6），供 PID、校准、飞行模式、解锁检查等页取固件参数名。

### 1.9 配置工作区其余页

- 飞控校准（`configuration/calibration/`）：加速度计、磁罗盘、RC 校准；取消要等飞控 ACK（S8）。
- PID 调参（`configuration/pid/`）：通用参数 + 语义角色。
- 端口配置（`configuration/ports/`）：走通用参数 `SERIALx_*`/`SER_TELx_*`。
- 电机输出（`configuration/motors/`）：输出只读展示。
- 飞行模式（`configuration/flight_modes/`）、电池配置（`configuration/battery/`）、解锁检查（`configuration/arming/`）、系统状态（`configuration/system/`）。

### 1.10 MAVLink 签名（`gcs_configuration_workspace.dart`、`connection/signing_controller.dart`）

- 本机密钥库、保存并应用密钥、下发 `SETUP_SIGNING`，均有确认（KD-11）。

### 1.11 日志与文件（`flutter_shell/lib/logging/`）

- 机载 DataFlash 日志：列表、下载、取消、进度、完整性校验、擦除（`onboard_log_page.dart`）。
- 链路 `.tlog` 录制与离线回放（`flight_log_page.dart`）。
- 机载文件：MAVLink FTP 浏览/下载/上传（`file_browser_page.dart`）。详情见 `docs/features/flight-logging-and-replay.md`。

### 1.12 相机 / 云台 / 视频（`flutter_shell/lib/payload/`）

- 相机与云台设备清单、状态回读、拍照/录像/变焦/云台指向/回中。
- 视频源元数据、本机解码渲染、本地录制与截图落盘、容量上限清理（FFmpeg 仅 LGPL；无后端时诚实降级）。详情见 `docs/features/video-camera-gimbal.md`。



## 2. 部分实现（已交付但有明确边界）

| 功能 | 已做 | 未做 |
| --- | --- | --- |
| 固件 | 连接流程中的固件识别、版本回读 | 刷写与校验（导出已删除，待独立 ADR） |
| 参数 | 全量读取、按名读写、写进度、语义角色 | 参数文件导入导出（导出侧已删除，导入侧 `mg_mission_doc_import_format` 之外无入口） |
| 日志 | 机载日志、tlog 录制/回放、MAVLink FTP 收发 | 事件日志服务、会话文件日志（导出已删除）、`.bin` 解析与图表 |
| 相机/云台/视频 | 状态回读、命令、本机解码/录制/截图（macOS arm64 已验证） | 真实硬件回读、其他平台验证 |

## 3. 未实现 / 未接入前端

### 3.1 FFI 导出但未绑定前端（27 个，全部有正当定位）

见 §4「未绑定导出明细」。这里不重复列举；**导出层没有悬空项**——
前两轮清理已把所有「无前端绑定、无内部调用方、无未来用途」的导出一律删除。

注意区分两件事：导出层干净，不等于**功能层**都已接 UI。当前仍有已实现却无入口的能力，
即下面这些（都在本轮盘点中确认）：

| 能力 | 现状 | 缺什么 |
|---|---|---|
| 文档撤销/重做 | core + FFI + `MissionDocumentController.undo/redo` 全通，有原生测试 | 没有按钮或快捷键；`can_undo`/`can_redo` 也被 Dart 解码器丢掉 |
| 气压计校准 | 后端策略完整（`BaroCalibration`）、Dart `CalibrationType.barometer` 可用、有测试 | 「飞控校准」页第三个 Tab 是空占位，且标签写的是「空速计」，与后端 type 3 的语义不符 |
| 围栏/Rally 核对 | `verifyFence`/`verifyRally` 已实现并绑定，结果在 `fenceVerification`/`rallyVerification` | 零调用点，差异明细无处显示（任务侧同类结果有专门展示） |
| 参数下载重试 | `requestParameters(force: true)` 已实现 | 失败后自动路径不再触发，界面只有一行红字，没有重试按钮 |

### 3.2 完全未实现（QGC/MP 有、M-GCS 目前没有）

- **手柄/手动控制（Joystick / MANUAL_CONTROL）**：仓库内无相关代码。
- **离线地图发行与瓦片持久化**：仅支持在线底图与自定义 XYZ 模板，无离线包管理。
- **日志分析器**：DataFlash `.bin` 解析与图表（QGC LogViewer / MP LogBrowse 级别）；tlog 仅基本回放。
- **固件刷写**：导出与存根已删除（`mg_firmware_flash` 原为 `NOT_SUPPORTED` 占位）；待独立 ADR 后重做。
- **相机定义 XML 驱动的完整设置项、云台 on-screen 指向/ROI 向导、热成像/多路画面**。

## 4. 未接入前端的 FFI 导出明细

当前公开导出 **154** 个、前端已绑定 **127** 个，未绑定 **27** 个。逐符号定位见 `bridge/ffi/API_CATALOG.md`（含「公开类型」段）。这 27 个全部有正当用途，**没有待处置债务**。

处置方式（沿用 KD-45）：

- **绑定 UI**：真正需要用户入口的能力，进入待办（3.1）。
- **补测试引用**：有原生价值但无 UI 需求，用合约测试固定契约。
- **定性为内部能力**：只服务开发/离线/原生测试，登记进 `INTERNAL_SYMBOL_NOTES`。
- **延期至 0.3.0 之后**：保留实现与 ABI，登记进 `DEFERRED_SYMBOL_NOTES`；当前版本不承诺 UI 入口。
- **退役候选**：登记进 `RETIREMENT_CANDIDATE_NOTES`，本轮不删除 ABI；先确认仓库外兼容性和独立用途。
- **删除旧路径**：被更新接口取代、且确认无未来用途的历史导出，直接删除并登记进 `RETIRED_SYMBOLS`，防止误恢复。

### 4.1 已删除的导出（三轮清理记录）

全部登记在 `tools/check_ffi_contract.py` 的 `RETIRED_SYMBOLS`（重新引入即门禁失败）。
公开导出从 **280 → 154**。详见 `CHANGELOG.md`。

**第一轮（0.2.1）——旧路径被新接口取代：**

| 簇 | 导出 | 取代者 |
| --- | --- | --- |
| 旧任务管理 | `mg_mission_add_item`/`update_item`/`remove_item`/`move_item`/`validate`/`get_stats`/`export_json`/`import_json`/`export_csv`/`import_csv`、`mg_mission_start` | `mg_mission_doc_apply`/`validate`/`export`/`import`/`export_format`/`import_format`；`mg_command_execute_for_vehicle`+轮询 |
| 旧围栏管理 | `mg_geofence_add_point`/`remove_point`/`get_points`/`validate`/`set_config`/`import_json`/`export_json`/`export_kml`、`mg_fence_clear` | `mg_mission_doc_apply`+`compile/upload/clear/verify_fence`、`mg_fence_download` |
| 旧 Rally | `mg_rally_clear` | `mg_mission_doc_clear_rally` |
| 旧连接 | `mg_connection_connect_tcp`、`mg_send_heartbeat`、`mg_stream_request` | `mg_connection_open`、`mg_stream_set_interval` |
| 旧串口读写 | `mg_serial_get_config`/`set_baud`/`set_protocol` | 通用参数页 `SERIALx_*`/`SER_TELx_*` |
| 旧 RC 校准状态 | `mg_rc_calibrate_status` | 通用 `mg_calibration_get_status`（type=radio） |
| 参数变化项 | `mg_params_get_changed` | 前端按描述符默认值比较 |
| 参数文件读写 | `mg_params_save_to_file`、`mg_params_load_from_file` | `mg_params_get_all` + Dart 侧文件读写 |

**第二轮——PX4 / 测绘 / 围栏 / 地形的功能级删除：**
`mg_geofence_check`、`mg_geofence_get_config`、`mg_fence_enable`、`mg_survey_generate*`、
`mg_terrain_*`、`mg_px4_*`（7 个）。

**第三轮——未绑定导出清理（无前端绑定、无内部调用方、无未来用途）：**
逐消息遥测 getter（`mg_vehicle_get_{{gps,imu,pressure,vibration,esc_status,wind_estimate,
servo_output,rc_channels,highres_imu,battery,flight_time,adsb_traffic,distance_sensors}}`）、
旧任务入口（`mg_mission_{{upload,get_plan,get_progress,get_current}}`、`mg_fence_get_plan`、
`mg_rally_get_plan`）、飞行模式辅助（`mg_flight_{{set_mode,get_mode_status,
get_available_modes,get_mode_history,get_loiter_radius,set_loiter_radius,
get_takeoff_altitude,set_takeoff_altitude}}`）、参数分组与批量
（`mg_config_{{get_groups,read_group,get_group_descriptor,metadata_coverage,write_params}}`、
`mg_params_{{set,refresh,get_refresh_status,prepare_change_set,commit_change_set}}`）、
连接辅助（`mg_connection_{{get_state,disconnect,connect,connect_udp,check_heartbeat,
get_heartbeat_age}}`）、`mg_serial_get_configs`、`mg_discover_devices`、SITL 启停
（`mg_sitl_*`）、固件刷写存根（`mg_firmware_flash*`）、事件日志（`mg_event_log_*`）、
会话日志（`mg_session_log_*`）、传感器解码（`mg_sensor_decode_*`、`mg_sensors_identify_all`、
`mg_sensors_get_statustext`）、参数文件变换（`mg_param_file_*`）、RC 与执行器配置
（`mg_rc_get_*`/`set_*`、`mg_actuator_*`）、校准历史与重试（`mg_calibration_get_history`、
`mg_calibration_get_retry_info`、`mg_calibration_start_airspeed`）、围栏/任务检查
（`mg_preflight_check`）、`mg_vehicle_count`、`mg_vehicle_close`、`mg_waveform_clear`、
`mg_operations_get_snapshot_for_vehicle`、`mg_set_event_callback`（含事件推送 ABI）。

**第四轮——FFI 事件转发链**（与第三轮同批落地，独立成一条记录）：
`mg_event_type_t`、`mg_event_callback_t`、`mg_core_t::events`（`EventDispatcher`）、
`post_event`、连接时的事件转发 lambda。前端从未注册回调（KD-27 采用轮询），
这条链已在空转。

### 4.2 未绑定导出的定位（27 个）

**a. 命令族（15 个）——由 submit/poll worker 内部调用**
`mg_command_arm`/`disarm`/`takeoff`/`land`/`rtl`/`set_mode`/`set_home`/`set_global_origin`/
`orbit`/`set_roi`/`guided_pause`/`guided_point`/`change_altitude`/`change_heading`/
`execute_for_vehicle`。
前端走 `mg_command_submit_for_vehicle` + `mg_command_poll`（异步、带 ACK 校验）；这些同步入口
是 worker 解析命令 JSON 后实际下发所调用的函数，**不是废弃路径**。

**b. ABI 错误基础设施（4 个）**
`mg_error_string`（错误码→常量字符串）、`mg_get_last_error`、`mg_set_last_error`、
`mg_clear_last_error`。供原生调用方与测试使用，前端读统一响应对象里的 `error` 字段，不需要它们。

**c. 诊断与仪表盘聚合（3 个）——打包校验必需**
`mg_connection_get_diagnostics`、`mg_system_get_diagnostics`、
`mg_telemetry_get_dashboard_snapshot`。`tools/ffi_bundle_symbols.dart` 要求它们在产物里可解析，
用来验证打包后的动态库确实导出了完整 ABI。

**d. 任务文档的 JSON 与围栏/Rally 编译（4 个）**
`mg_mission_doc_export`、`mg_mission_doc_import`、`mg_mission_doc_compile_fence`、
`mg_mission_doc_compile_rally`。
前端曾有对应的 Dart 包装（`MissionDocumentController.exportJson`/`importJson`、
`MissionDocumentBackend.compileFencePlan`/`compileRallyPlan`），但那些方法全仓零调用——
任务导入导出实际走文件路径（`save`/`load`）与交换格式（`exportInterchange`，`waypoints`/`plan`），
围栏/Rally 下发走 `uploadFence`/`uploadRally`（内部自行编译）。2026-10 删掉这层无人使用的
Dart 包装后，四个导出保留在 ABI 中，由 `core/tests/test_mission_document.cc` 覆盖。

**e. 其它（1 个）**
- `mg_vehicle_open`：`mg_connection_open` 的内部实现路径，并供硬件诊断工具直接开单机会话。
- `mg_mission_doc_import_format`：任务文件导入入口。前端 `MissionDocumentFfiBackend`
  已绑定，由航线规划的「导入文件」弹层调用（读盘后交给 `importInterchange`）。

## 5. 维护规则

- 改 `CAPABILITY_METADATA`、`INTERNAL_SYMBOL_NOTES`、`SUPERSEDED_SYMBOL_NOTES`、
  `DEFERRED_SYMBOL_NOTES`、`RETIREMENT_CANDIDATE_NOTES` 或 `RETIRED_SYMBOLS` 后，跑
  `python3 tools/check_ffi_contract.py --write-catalog --write-baseline` 再提交。
- 新增公开导出必须同时给出绑定、测试引用或定性，否则 `tools/preflight.sh` 会失败。
- 本文件是人工维护的功能视图；模块级的机器校验以 `docs/capabilities.md` 与 `bridge/ffi/API_CATALOG.md` 为准。
