# 功能清单与交付分类（Feature Inventory）

状态：核对日期 2026-10-11，对应 `main@4722bab`。
来源：`docs/plans/mgcs-development-plan-2026-10-10.md` 阶段 0 的交付口径；`docs/capabilities.md` 的模块状态表；`bridge/ffi/API_CATALOG.md` 的导出/绑定明细。

本文件的用途是把「有导出」「有原生实现」「有 UI 入口」「用户能用」这几件事分开讲清楚，回答三类问题：
**已经能用的功能有哪些、还有什么没做、哪些 FFI 导出没接前端以及为什么。** 它不是需求文档，也不替代 `docs/features/` 下的方向简报。

口径（与 `docs/capabilities.md` 一致）：

- **已交付**：主要工作流有实际 UI 入口，成功/失败/超时/断链都有定义，且有对应测试。
- **部分接入**：模块内一部分能力已进 UI，其余仍是内部接口或仅测试覆盖。
- **待接入**：能力已在 FFI 暴露且有原生实现，但尚未提供 UI 入口。
- **内部能力**：只服务开发、离线分析或原生测试，刻意不进入业务 UI。
- **被取代**：被更新的接口取代的历史路径，保留在 ABI 中但不再接 UI（退役候选）。

底层统计（`python3 tools/check_ffi_contract.py`）：公开导出 **314** 个，前端已绑定 **136** 个，未绑定 **178** 个；其中内部能力 **17** 个、被取代 **31** 个、仅测试引用 **110** 个、剩余未定性债务 **20** 个（`tools/ffi_unbound_baseline.json`）。

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
- RC 通道、健康位、振动、IMU、气压、距离传感器、风速估计、ESC 状态、ADS-B、预解锁定检查等（`mg_vehicle_get_*`、`mg_health_get_all`）。
- 实时波形（`components/realtime_waveform.dart` + `mg_waveform_*`）。

### 1.5 地图（`flutter_shell/lib/map/amap_map_view.dart`）

- 多种底图源：标准、地形、高德标准/卫星/卫星+路网，以及自定义 XYZ 瓦片（带校验，KD-37）。
- GCJ-02 坐标换算、飞行器标绘、任务/围栏叠加、引导点标记（命令成功后才更新）。
- 地图偏好持久化（缩放、中心、底图）与数据来源署名。

### 1.6 任务规划与交换（`flutter_shell/lib/mission/`）

- 文档模型（`mg_mission_doc_*`）：创建、编辑、撤销/重做、校验、估算、上传、回读、比对（verify）。
- 航点编辑、地图绘制、上传/清除带统一确认（KD-08）。
- 文件互操作：`waypoints`（MP/QGC 的 QGC WPL 110 文本）与 `plan`（QGC JSON）导入导出。
- 任务命令目录、执行进度（`mg_mission_get_command_catalog`、`mg_mission_get_execution_progress`）。

### 1.7 围栏与 Rally（`mission/mission_planning_panel.dart`）

- 经任务文档区段下发/回读/比对围栏与 Rally（`mg_mission_doc_*_fence/rally`、`mg_fence_download`、`mg_rally_download`）。

### 1.8 测绘规划（`flutter_shell/lib/survey/survey_planning_page.dart`）

- 矩形与走廊测绘的相机模型、GSD/重叠/线间距、结果生成与估算（`mg_survey_generate*`）。
- 结果导出复用时序/航点格式。详情见 `docs/features/survey-and-terrain.md`。

### 1.9 参数（`flutter_shell/lib/configuration/parameters/all_parameters_page.dart`）

- 参数全量请求、按名读取、写入与写进度（`mg_params_request_all`/`get_all`/`get_by_name`/`get_progress`/`get_write_status`）。
- 按语义角色取值（`mg_config_get_param_roles`，safety.md S6），供 PID、校准、飞行模式、解锁检查等页取固件参数名。

### 1.10 配置工作区其余页

- 飞控校准（`configuration/calibration/`）：加速度计、磁罗盘、RC 校准；取消要等飞控 ACK（S8）。
- PID 调参（`configuration/pid/`）：通用参数 + 语义角色。
- 端口配置（`configuration/ports/`）：走通用参数 `SERIALx_*`/`SER_TELx_*`。
- 电机输出（`configuration/motors/`）：输出只读展示。
- 飞行模式（`configuration/flight_modes/`）、电池配置（`configuration/battery/`）、解锁检查（`configuration/arming/`）、系统状态（`configuration/system/`）。

### 1.11 MAVLink 签名（`gcs_configuration_workspace.dart`、`connection/signing_controller.dart`）

- 本机密钥库、保存并应用密钥、下发 `SETUP_SIGNING`，均有确认（KD-11）。

### 1.12 日志与文件（`flutter_shell/lib/logging/`）

- 机载 DataFlash 日志：列表、下载、取消、进度、完整性校验、擦除（`onboard_log_page.dart`）。
- 链路 `.tlog` 录制与离线回放（`flight_log_page.dart`）。
- 机载文件：MAVLink FTP 浏览/下载/上传（`file_browser_page.dart`）。详情见 `docs/features/flight-logging-and-replay.md`。

### 1.13 相机 / 云台 / 视频（`flutter_shell/lib/payload/`）

- 相机与云台设备清单、状态回读、拍照/录像/变焦/云台指向/回中。
- 视频源元数据、本机解码渲染、本地录制与截图落盘、容量上限清理（FFmpeg 仅 LGPL；无后端时诚实降级）。详情见 `docs/features/video-camera-gimbal.md`。

### 1.14 PX4 支持（`docs/features/px4-parity.md`）

- 统一模式表、引导/起飞行为、PX4 参数元数据 profile、任务命令覆盖、PX4 行为与错误路径测试。

## 2. 部分实现（已交付但有明确边界）

| 功能 | 已做 | 未做 |
| --- | --- | --- |
| 固件 | 连接流程中的固件识别 | 刷写与校验（`mg_firmware_flash` 未接 UI） |
| 地形 | 前端 backend + 在线高程 provider（open-elevation）、剖面查询 | 页面入口（D7） |
| 参数 | 读写、写进度、语义角色 | 文件导入导出（`.param`/CSV）、变化项查询 |
| 日志 | 机载日志、tlog 录制/回放、MAVLink FTP | 事件日志服务、会话文件日志 |
| 相机/云台/视频 | 状态回读、命令、本机解码/录制/截图（macOS arm64 已验证） | 真实硬件回读、其他平台验证 |
| 测绘/地形 | 矩形/走廊测绘已进页 | 任意多边形、自定义相机库、DEM 瓦片渲染 |

## 3. 未实现 / 未接入前端

### 3.1 FFI 已有、但未接 UI（待接入）

| 能力 | 导出 | 实现 | 现状 |
| --- | --- | --- | --- |
| 参数文件导入导出 | `mg_param_file_export/import(_csv)` | `bridge/ffi/ffi_telemetry.cc` | 纯内容变换（JSON 数组 ↔ `.param`/CSV 文本），未接前端文件选择；这是参数文件功能的**规范实现**，磁盘读写交给 Dart |
| 参数预设 | `mg_config_get_presets`/`apply_preset` | `bridge/ffi/ffi_config.cc` | 无预设管理 UI |
| 空速校准 | `mg_calibration_start_airspeed` | `bridge/ffi/ffi_calibration.cc` | 校准页无该入口 |
| 设备发现 | `mg_discover_devices` | `bridge/ffi/ffi_vehicle_manager.cc` | 连接面板走端口枚举，未用此路径 |
| 事件日志状态 | `mg_event_log_get_status` | `bridge/ffi/ffi_logging.cc` | 事件日志服务未接 UI |
| 会话文件日志 | `mg_session_log_start/stop/status` | `bridge/ffi/ffi_telemetry.cc` | 未接 UI |
| 遥测 series 记录器 | `mg_telemetry_register_series` 等 6 个 | `bridge/ffi/ffi_telemetry.cc` | 前端波形走 `mg_waveform_*`，series 未用 |
| 任务模板 | `mg_mission_get_templates` | `bridge/ffi/ffi_mission_management.cc` | 无模板选择 UI |
| 跳转到航点 | `mg_mission_set_current` | `bridge/ffi/ffi_mission_commands.cc` | 无入口 |

### 3.2 完全未实现（QGC/MP 有、M-GCS 目前没有）

- **手柄/手动控制（Joystick / MANUAL_CONTROL）**：仓库内除 PX4 参数元数据外无相关代码。
- **离线地图发行与瓦片持久化**：仅支持在线底图与自定义 XYZ 模板，无离线包管理。
- **日志分析器**：DataFlash `.bin` 解析与图表（QGC LogViewer / MP LogBrowse 级别）；tlog 仅基本回放。
- **固件刷写**：无刷写 UI 与下载管理。
- **地形页面**：有 backend 与 provider，无页面。
- **相机定义 XML 驱动的完整设置项、云台 on-screen 指向/ROI 向导、热成像/多路画面**。
- **任意多边形/结构扫描测绘、影像拼接、DEM 瓦片渲染**。

## 4. 未接入前端的 FFI 导出明细

下表按「簇」列出 178 个未绑定导出中需要解释的部分；**已定性的导出（内部能力 17 + 被取代 31）** 的逐符号定位见 `bridge/ffi/API_CATALOG.md` 的「已定性的公开导出」一节（由 `tools/check_ffi_contract.py` 生成）。仅测试覆盖的 110 个不逐一列出，它们通过测试保持契约。

处置方式（沿用 KD-45，四选一，**不删除导出**）：

- **绑定 UI**：真正需要用户入口的能力，进入待办（3.1）。
- **补测试引用**：有原生价值但无 UI 需求，用合约测试固定契约。
- **定性为内部能力**：只服务开发/离线/原生测试，登记进 `INTERNAL_SYMBOL_NOTES`。
- **登记退役**：被更新接口取代的旧路径，登记进 `SUPERSEDED_SYMBOL_NOTES`。

### 4.1 被取代的历史路径（31 个，已登记退役候选）

| 簇 | 导出 | 取代者 |
| --- | --- | --- |
| 旧任务管理 | `mg_mission_add_item`/`update_item`/`remove_item`/`move_item`/`validate`/`get_stats`/`export_json`/`import_json`/`export_csv`/`import_csv`、`mg_mission_start` | `mg_mission_doc_apply`/`validate`/`export`/`import`/`export_format`/`import_format`；`mg_command_execute_for_vehicle`+轮询 |
| 旧围栏管理 | `mg_geofence_add_point`/`remove_point`/`get_points`/`validate`/`set_config`/`import_json`/`export_json`/`export_kml`、`mg_fence_clear` | `mg_mission_doc_apply`+`compile/upload/clear/verify_fence`、`mg_fence_download` |
| 旧 Rally | `mg_rally_clear` | `mg_mission_doc_clear_rally` |
| 旧连接 | `mg_connection_connect_tcp`、`mg_send_heartbeat`、`mg_stream_request` | `mg_connection_open`、`mg_stream_set_interval` |
| 旧串口读写 | `mg_serial_get_config`/`set_baud`/`set_protocol` | 通用参数页 `SERIALx_*`/`SER_TELx_*` |
| 旧 RC 校准状态 | `mg_rc_calibrate_status` | 通用 `mg_calibration_get_status`（type=radio） |
| 参数变化项 | `mg_params_get_changed` | 前端按描述符默认值比较 |
| 参数文件读写 | `mg_params_save_to_file`、`mg_params_load_from_file` | `mg_params_get_all` + `mg_param_file_export` + Dart 文件写入；`mg_param_file_import` + `mg_params_commit_change_set` |

> 旧任务管理路径的文件头注释（`bridge/ffi/ffi_mission_management.cc`）明确写了并发调用会 lost-update、调用方必须串行——这正是文档模型 `mg_mission_doc_*` 取代它的原因。

### 4.2 内部能力（17 个，无 UI 计划）

- **PX4 便捷封装（7）**：`mg_px4_actuator_*`/`flight_mode_get_config`/`safety_*`/`tuning_*`；通用参数页配合 `px4` 描述符 profile 已覆盖同一批参数。
- **SITL（3）**：`mg_sitl_start/status/stop`；只服务开发与离线分析。
- **执行器/RC 通道写入（5）**：`mg_actuator_get_limits`/`set_function`/`set_limits`、`mg_rc_set_mapping`/`set_failsafe`；通用参数页覆盖 `SERVOx_*`/`RCMAP_*`。
- **参数预校验（1）**：`mg_config_validate_param`；前端按描述符范围/枚举本地校验。
- **ABI 基础设施（1）**：`mg_error_string`（错误码→字符串常量）。

### 4.3 剩余未定性债务（20 个）

`tools/ffi_unbound_baseline.json` 当前固化 20 个「无绑定、无定性、无测试引用」的导出，主要是 3.1 里的待接入能力（参数文件内容变换、事件/会话日志、遥测 series、任务模板/跳转、空速校准、预设、设备发现）。门禁只允许基线缩小：新增未定性导出会失败，被绑定/被测试引用/被定性后仍留在基线的符号也会失败。

## 5. 维护规则

- 改 `CAPABILITY_METADATA`、`INTERNAL_SYMBOL_NOTES` 或 `SUPERSEDED_SYMBOL_NOTES` 后，跑
  `python3 tools/check_ffi_contract.py --write-catalog` 与 `--write-baseline` 再提交。
- 新增公开导出必须同时给出绑定、测试引用或定性，否则 `tools/preflight.sh` 会失败。
- 本文件是人工维护的功能视图；模块级的机器校验以 `docs/capabilities.md` 与 `bridge/ffi/API_CATALOG.md` 为准。
