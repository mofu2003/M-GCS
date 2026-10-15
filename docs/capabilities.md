# 当前能力

更新时间：2026-10-14。

本文是能力参考，不是开发约束。状态以当前代码、FFI 契约和测试为准；“已绑定”只表示存在 Dart 绑定，不代表所有硬件和失败分支都已验证。

<!-- capabilities:start -->
| 模块 | 公开导出 | 已接入前端 | 状态 | 说明 |
| --- | --- | --- | --- | --- |
| `ffi_api_configuration.h` | 6 | 6 | 部分接入 | 连接与校准子集已进入配置页；参数组描述符（`mg_config_get_param_roles` 语义角色表）经 Dart 绑定进入 PID、校准、飞行模式与解锁检查等配置页，由前端按语义角色取固件参数名（safety.md S6）。执行器/RC 通道的只读清单由内部导出提供；写入统一走通用参数页（`SERVOx_*`/`RCMAP_*`）。 |
| `ffi_api_connection.h` | 19 | 17 | 已交付 | 串口/UDP/TCP 连接、断开、链路状态与本地回环由连接面板和状态栏消费。旧入口 `mg_connection_connect_tcp`、`mg_send_heartbeat`、`mg_stream_request` 已被 `mg_connection_open`/`mg_stream_set_interval` 取代并删除。 |
| `ffi_api_core.h` | 11 | 6 | 已交付 | 生命周期、版本、错误信息与整机快照由前端启动路径消费；`mg_error_string`（错误码→字符串）为 ABI 基础设施，供原生调用方与测试使用。 |
| `ffi_api_firmware.h` | 3 | 3 | 已交付 | 固件类型识别（含 capability 位与最小起飞高度）、固件版本回读与版本请求均由连接流程消费。固件刷写的导出与存根已删除（待独立 ADR 后重做），当前版本不提供该能力。 |
| `ffi_api_flight.h` | 18 | 3 | 部分接入 | 生产入口是 `mg_command_submit_for_vehicle` + `mg_command_poll`（带 ACK 校验）与 `mg_flight_get_modes`（按机型的模式表）；同文件里的同步 `mg_command_arm`/`takeoff`/`land`/`rtl`/`set_mode` 等由 submit 的 worker 内部调用，不作为前端入口。 |
| `ffi_api_logging.h` | 18 | 18 | 部分接入 | 机载 DataFlash 日志列表/下载/擦除（「机载日志」页）、链路 `.tlog` 录制/离线回放（「飞行记录」页）与 `mg_fs_*` 通用 MAVLink FTP 文件浏览/收发（「机载文件」页）均已接入；未接入的事件日志与会话文件日志导出已删除，见 docs/features/flight-logging-and-replay.md。 |
| `ffi_api_mission.h` | 33 | 29 | 已交付 | 任务规划、上传、围栏/Rally 下发与航点编辑走任务面板与地图；文档模型（`mg_mission_doc_*`）是生产路径，旧 `MissionManager` 增删改/导入导出接口（`mg_mission_add_item` 等、`mg_mission_start`、`mg_mission_get_templates`）已被取代并删除。 |
| `ffi_api_params.h` | 7 | 7 | 部分接入 | 参数全量请求、按名读取、写入与回读校验、下载与写入进度均接入配置页；语义角色表（`mg_config_get_param_roles`）供 PID、校准、飞行模式与解锁检查等页按角色取固件参数名（S6）。旧的批量/刷新/文件变换导出（`mg_params_set`、`mg_params_refresh`、`mg_param_file_*`、`mg_params_*_change_set`）已删除，见 docs/features/feature-inventory.md。 |
| `ffi_api_payload.h` | 23 | 23 | 部分接入 | 相机/云台/视频元数据接入配置工作区「相机与云台」页（`payload`，S2/S3/S4）：设备清单、拍照/录像/变焦/云台指向/回中与视频流元数据。本地视频解码与录制/截图（S3/S4，ADR-0008）已接入：`mg_video_renderer_status`/`mg_video_poll_frame` 渲染最新 RGBA 帧，`mg_video_start_recording`/`mg_video_stop_recording`/`mg_video_recording_status`/`mg_video_capture_frame` 把正在播放的码流复制到本地文件（Matroska 容器）并按容量上限清理最旧文件；没有编译进后端时 `available:false`/`backend:"none"` 并如实说明。S1 的读契约返回真实适配器缓存，无设备时 `success:true`+`available:false`，见 docs/features/video-camera-gimbal.md。 |
| `ffi_api_telemetry.h` | 16 | 15 | 部分接入 | 遥测快照、姿态、GPS、电池进入顶栏/仪表盘；波形与部分导航量仍为 FFI 内部能力。 |
| **合计** | **154** | **127** | — | 其余 27 个公开导出尚未被前端绑定。 |
<!-- capabilities:end -->

## 视频后端

默认构建可以不包含 FFmpeg，本地视频能力会返回不可用状态，而不是伪装成可用。macOS 本地 LGPL 构建已验证；其他平台是否可用必须按实际构建和设备验证。

## 重新确认能力

```bash
python3 tools/check_ffi_contract.py
```

FFI 完整签名和所有权规则见 `bridge/ffi/API_CATALOG.md`。历史实现过程见 `docs/features/`，不作为当前流程门槛。
