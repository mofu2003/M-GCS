# FFI API 目录

由 `python3 tools/check_ffi_contract.py --write-catalog` 从公开头文件与 Flutter 绑定生成。
运行 `python3 tools/check_ffi_contract.py --library build/libmgcore.dylib` 可核对编译库。
Linux 使用 `build/libmgcore.so`，Windows 使用 `build/mgcore.dll`。

`char*` 结果使用 `mg_free_string` 释放，`mg_error_string` 返回的常量除外。
`success:false` 一律返回 {'success':false,'error':{'code':int,'name':str,'message':str}}。
有 COMMAND_ACK 的命令只有 ACK 接受才回 success；SET_POSITION_TARGET_* 等无 ACK 命令只表示已发出。
飞行指令的生产入口是 submit/poll；所有公开名称必须对应当前规范接口。
任务上传的即时响应表示传输启动，最终结果需读取 operation 状态。
符号检查不等于完整的参数类型或行为验证；真实交互另由 FFI smoke 和合约测试覆盖。

公开函数：154；Flutter 绑定：130。
公开类型：2。

## ffi_api_configuration.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_calibration_accel_confirm(mg_core_t* core, int position);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_cancel(mg_core_t* core);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_get_status(mg_core_t* core);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_start(mg_core_t* core, int type, int accel_mode);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_config_get_param_roles(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_serial_get_uart_names(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |

## ffi_api_connection.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_connection_close(mg_core_t* core, int link_id);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_connection_close_route(mg_core_t* core, int route_id);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_connection_get_diagnostics(mg_core_t* core);` | — |
| `char* mg_connection_get_session_state(mg_core_t* core, int link_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_connection_list(mg_core_t* core);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_connection_list_ports(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `int mg_connection_open(mg_core_t* core, const char* endpoint_uri);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_stream_set_interval(mg_core_t* core, int msg_id, int interval_us);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_clear_signing_candidates(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_clear_signing_key(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `int mg_vehicle_get_active(mg_core_t* core);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_vehicle_get_session_mission(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_vehicle_get_signing_status(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_list(mg_core_t* core);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `int mg_vehicle_open(mg_core_t* core, const char* endpoint);` | — |
| `char* mg_vehicle_select(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_vehicle_send_setup_signing(mg_core_t* core, int vehicle_id, const char* key_hex, uint64_t initial_timestamp);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_set_signing_candidates(mg_core_t* core, int vehicle_id, const char* candidates_json);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_set_signing_key(mg_core_t* core, int vehicle_id, const char* key_hex, int sign_outgoing, uint64_t persisted_timestamp);` | `flutter_shell/lib/connection/native_core.dart` |

## ffi_api_core.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `void mg_clear_last_error(void);` | — |
| `mg_core_t* mg_create(void);` | `flutter_shell/lib/connection/native_core.dart` |
| `void mg_destroy(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `const char* mg_error_string(mg_error_t error);` | — |
| `void mg_free_string(char* str);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart`<br>`flutter_shell/lib/connection/flight_command_ffi_backend.dart`<br>`flutter_shell/lib/connection/native_core.dart`<br>`flutter_shell/lib/connection/parameter_ffi_backend.dart`<br>`flutter_shell/lib/connection/telemetry_ffi_backend.dart`<br>`flutter_shell/lib/fleet/fleet_ffi_backend.dart`<br>`flutter_shell/lib/logging/file_ffi_backend.dart`<br>`flutter_shell/lib/logging/log_ffi_backend.dart`<br>`flutter_shell/lib/logging/tlog_ffi_backend.dart`<br>`flutter_shell/lib/mission/mission_document_ffi_backend.dart`<br>`flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `mg_error_t mg_get_last_error(void);` | — |
| `char* mg_operations_cancel(mg_core_t* core, const char* operation_id);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_operations_cancel_for_vehicle( mg_core_t* core, int vehicle_id, const char* operation_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_operations_get_snapshot(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `void mg_set_last_error(mg_error_t error);` | — |
| `char* mg_system_get_diagnostics(mg_core_t* core);` | — |

## ffi_api_firmware.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_firmware_get_type(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_firmware(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_request_firmware_version(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |

## ffi_api_flight.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_command_arm(mg_core_t* core, int force);` | — |
| `char* mg_command_change_altitude(mg_core_t* core, float altitude_change_m);` | — |
| `char* mg_command_change_heading(mg_core_t* core, float heading_deg, float yaw_rate_dps, int direction);` | — |
| `char* mg_command_disarm(mg_core_t* core, int force);` | — |
| `char* mg_command_execute_for_vehicle(mg_core_t* core, int vehicle_id, const char* command_json);` | — |
| `char* mg_command_guided_pause(mg_core_t* core);` | — |
| `char* mg_command_guided_point(mg_core_t* core, double lat, double lon, float alt, float yaw, float speed);` | — |
| `char* mg_command_land(mg_core_t* core);` | — |
| `char* mg_command_orbit(mg_core_t* core, const char* json_config);` | — |
| `char* mg_command_poll(mg_core_t* core, uint64_t ticket);` | `flutter_shell/lib/connection/flight_command_ffi_backend.dart` |
| `char* mg_command_rtl(mg_core_t* core);` | — |
| `char* mg_command_set_global_origin(mg_core_t* core, double lat, double lon, float alt_m);` | — |
| `char* mg_command_set_home(mg_core_t* core, double lat, double lon, float alt_m);` | — |
| `char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode);` | — |
| `char* mg_command_set_roi(mg_core_t* core, double lat, double lon, float alt);` | — |
| `char* mg_command_submit_for_vehicle(mg_core_t* core, int vehicle_id, const char* command_json);` | `flutter_shell/lib/connection/flight_command_ffi_backend.dart` |
| `char* mg_command_takeoff(mg_core_t* core, float altitude_m);` | — |
| `char* mg_flight_get_modes(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |

## ffi_api_logging.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_fs_cancel(mg_core_t* core);` | `flutter_shell/lib/logging/file_ffi_backend.dart` |
| `char* mg_fs_download_file(mg_core_t* core, const char* remote_path, const char* local_path);` | `flutter_shell/lib/logging/file_ffi_backend.dart` |
| `char* mg_fs_list_directory(mg_core_t* core, const char* remote_path);` | `flutter_shell/lib/logging/file_ffi_backend.dart` |
| `char* mg_fs_transfer_status(mg_core_t* core);` | `flutter_shell/lib/logging/file_ffi_backend.dart` |
| `char* mg_fs_upload_file(mg_core_t* core, const char* local_path, const char* remote_path);` | `flutter_shell/lib/logging/file_ffi_backend.dart` |
| `char* mg_onboard_log_download(mg_core_t* core, int log_id, const char* filepath);` | `flutter_shell/lib/logging/log_ffi_backend.dart` |
| `char* mg_onboard_log_download_cancel(mg_core_t* core);` | `flutter_shell/lib/logging/log_ffi_backend.dart` |
| `char* mg_onboard_log_download_status(mg_core_t* core);` | `flutter_shell/lib/logging/log_ffi_backend.dart` |
| `char* mg_onboard_log_erase(mg_core_t* core);` | `flutter_shell/lib/logging/log_ffi_backend.dart` |
| `char* mg_onboard_log_get_entries(mg_core_t* core);` | `flutter_shell/lib/logging/log_ffi_backend.dart` |
| `char* mg_onboard_log_request_list(mg_core_t* core);` | `flutter_shell/lib/logging/log_ffi_backend.dart` |
| `char* mg_tlog_replay_close(mg_core_t* core);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |
| `char* mg_tlog_replay_open(mg_core_t* core, const char* filepath);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |
| `char* mg_tlog_replay_seek(mg_core_t* core, double percent);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |
| `char* mg_tlog_replay_step(mg_core_t* core, int max_messages);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |
| `char* mg_tlog_start(mg_core_t* core, const char* filepath);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |
| `char* mg_tlog_status(mg_core_t* core);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |
| `char* mg_tlog_stop(mg_core_t* core);` | `flutter_shell/lib/logging/tlog_ffi_backend.dart` |

## ffi_api_mission.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_fence_download(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_clear(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_apply(mg_core_t* core, const char* json_command);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_clear_fence(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_clear_rally(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_compile(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_compile_fence(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_compile_rally(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_create(mg_core_t* core, const char* json_config);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_estimate(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_export(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_export_format( mg_core_t* core, const char* format);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_get(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_import(mg_core_t* core, const char* json_document);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_import_fence_from_vehicle(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_import_format( mg_core_t* core, const char* format, const char* text);` | — |
| `char* mg_mission_doc_import_from_vehicle(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_import_rally_from_vehicle(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_load(mg_core_t* core, const char* filepath);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_redo(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_save(mg_core_t* core, const char* filepath);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_undo(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_upload( mg_core_t* core, unsigned long long expected_revision);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_upload_fence( mg_core_t* core, unsigned long long expected_revision);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_upload_rally( mg_core_t* core, unsigned long long expected_revision);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_validate(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_verify(mg_core_t* core, double tolerance_m);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_verify_fence(mg_core_t* core, double tolerance_m);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_doc_verify_rally(mg_core_t* core, double tolerance_m);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_download(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_get_command_catalog(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_get_execution_progress(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_rally_download(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |

## ffi_api_params.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_config_read_param(mg_core_t* core, const char* param_name);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_config_write_param(mg_core_t* core, const char* param_name, float value);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_all(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_by_name(mg_core_t* core, const char* param_name);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_progress(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_write_status(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_request_all(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |

## ffi_api_payload.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_camera_get_status(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_request_info(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_set_mode(mg_core_t* core, int camera_id, int mode);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_set_zoom(mg_core_t* core, int camera_id, float zoom_level);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_start_video(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_stop_capture(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_stop_video(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_take_photo(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_command_gimbal_control(mg_core_t* core, float pitch_deg, float yaw_deg);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_command_gimbal_recenter(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_gimbal_get_attitude(mg_core_t* core, int gimbal_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_gimbal_get_status(mg_core_t* core, int gimbal_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_gimbal_request_info(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_capture_frame(mg_core_t* core, const char* path);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_get_streams(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `int mg_video_poll_frame(mg_core_t* core, unsigned char* dst, int dst_capacity, int* out_width, int* out_height, long long* out_pts_us, unsigned long long* out_sequence);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_recording_status(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_renderer_status(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_start(mg_core_t* core, const char* uri);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_start_recording(mg_core_t* core, const char* path);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_status(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_stop(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_video_stop_recording(mg_core_t* core);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |

## ffi_api_telemetry.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_rc_get_channels(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_sensors_get_health(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_servo_get_outputs(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_telemetry_get_dashboard_snapshot(mg_core_t* core);` | — |
| `char* mg_telemetry_get_freshness(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_telemetry_get_messages(mg_core_t* core, unsigned long long since_seq);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_get_attitude(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_extended_sys_state(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_firmware_version(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_global_position(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_home_position(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_session_status(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart`<br>`flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_vehicle_get_summary(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_vfr_hud(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_waveform_get_instances(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `int mg_waveform_poll(mg_core_t* core, float* out_points, int out_max_points, int target_points, int window_ms, int instance);` | `flutter_shell/lib/connection/native_core.dart` |

## 公开类型

前端不直接绑定这些类型；它们与函数一样属于公开 ABI，删除或改签名是破坏性变更。

| 类型 | 头文件 | 定义 |
| --- | --- | --- |
| `mg_core_t` | `ffi_api_core.h` | `typedef struct mg_core_t mg_core_t;` |
| `mg_error_t` | `ffi_api_core.h` | `typedef enum { MG_ERROR_NONE = 0, MG_ERROR_NULL_CORE = -1, MG_ERROR_NOT_CONNECTED = -2, MG_ERROR_SEND_FAILED = -3, MG_ERROR_INVALID_PARAM = -4, MG_ERROR_INVALID_STATE = -5, MG_ERROR_TIMEOUT = -6, MG_ERROR_BUSY = -7, MG_ERROR_NOT_SUPPORTED = -8, MG_ERROR_INTERNAL = -9, MG_ERROR_BUFFER_TOO_SMALL = -10, MG_ERROR_INVALID_MESSAGE = -11, MG_ERROR_CHECKSUM_FAIL = -12, MG_ERROR_PROTOCOL = -13, MG_ERROR_NOT_FOUND = -14, MG_ERROR_CALIBRATION = -15, MG_ERROR_REJECTED = -16, } mg_error_t;` |

## 已定性的公开导出

这些符号保留在 ABI 中且定位已经明确：**内部能力**只服务开发、离线分析或原生测试；
**0.3.0 之后延期**是当前版本明确不接 UI 的功能接口；**退役候选**仍保留导出，
待兼容性确认后再删除；**被取代**是已从源码删除并登记进 `RETIRED_SYMBOLS` 的历史路径。
分类符号若被 Flutter 绑定，`tools/check_ffi_contract.py` 会要求重新评估定位。

| 符号 | 类别 | 定位 |
| --- | --- | --- |
| `mg_error_string` | 内部能力 | ABI 基础设施：错误码→字符串常量，供原生调用方与测试使用，无 UI |
