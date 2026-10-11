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

公开函数：314；Flutter 绑定：136。

## ffi_api_configuration.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_actuator_get_limits(mg_core_t* core, int channel);` | — |
| `char* mg_actuator_get_mapping(mg_core_t* core);` | — |
| `char* mg_actuator_motor_test(mg_core_t* core, int motor, float throttle_pct, int duration_ms);` | — |
| `char* mg_actuator_set_function(mg_core_t* core, int channel, int function_id);` | — |
| `char* mg_actuator_set_limits(mg_core_t* core, int channel, int min_pwm, int max_pwm, int trim_pwm);` | — |
| `char* mg_calibration_accel_confirm(mg_core_t* core, int position);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_cancel(mg_core_t* core);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_get_history(mg_core_t* core, int type);` | — |
| `char* mg_calibration_get_retry_info(mg_core_t* core);` | — |
| `char* mg_calibration_get_status(mg_core_t* core);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_start(mg_core_t* core, int type, int accel_mode);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart` |
| `char* mg_calibration_start_airspeed(mg_core_t* core);` | — |
| `char* mg_config_apply_preset(mg_core_t* core, const char* preset_name);` | — |
| `char* mg_config_get_group_descriptor(mg_core_t* core, const char* group_name);` | — |
| `char* mg_config_get_groups(mg_core_t* core);` | — |
| `char* mg_config_get_param_roles(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_config_get_presets(mg_core_t* core);` | — |
| `char* mg_config_read_group(mg_core_t* core, const char* group_name);` | — |
| `char* mg_config_validate_param(mg_core_t* core, const char* param_name, float value);` | — |
| `char* mg_px4_actuator_get_config(mg_core_t* core);` | — |
| `char* mg_px4_actuator_set_config(mg_core_t* core, const char* json_config);` | — |
| `char* mg_px4_flight_mode_get_config(mg_core_t* core);` | — |
| `char* mg_px4_safety_get(mg_core_t* core);` | — |
| `char* mg_px4_safety_set(mg_core_t* core, const char* json_config);` | — |
| `char* mg_px4_tuning_get(mg_core_t* core);` | — |
| `char* mg_px4_tuning_set(mg_core_t* core, const char* json_config);` | — |
| `char* mg_rc_calibrate_save(mg_core_t* core);` | — |
| `char* mg_rc_calibrate_start(mg_core_t* core);` | — |
| `char* mg_rc_calibrate_status(mg_core_t* core);` | — |
| `char* mg_rc_get_calibration(mg_core_t* core);` | — |
| `char* mg_rc_get_failsafe(mg_core_t* core);` | — |
| `char* mg_rc_get_mapping(mg_core_t* core);` | — |
| `char* mg_rc_set_failsafe(mg_core_t* core, int mode, int threshold);` | — |
| `char* mg_rc_set_mapping(mg_core_t* core, const char* function, int channel);` | — |
| `char* mg_serial_get_config(mg_core_t* core, const char* port_name);` | — |
| `char* mg_serial_get_configs(mg_core_t* core);` | — |
| `char* mg_serial_get_uart_names(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_serial_set_baud(mg_core_t* core, const char* port_name, int baud);` | — |
| `char* mg_serial_set_protocol(mg_core_t* core, const char* port_name, int protocol);` | — |

## ffi_api_connection.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_connection_check_heartbeat(mg_core_t* core, int timeout_ms);` | — |
| `char* mg_connection_close(mg_core_t* core, int link_id);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_connection_close_route(mg_core_t* core, int route_id);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_connection_connect(mg_core_t* core, const char* port, int baud);` | — |
| `char* mg_connection_connect_tcp(mg_core_t* core, const char* endpoint);` | — |
| `char* mg_connection_connect_udp(mg_core_t* core, const char* endpoint);` | — |
| `char* mg_connection_disconnect(mg_core_t* core);` | — |
| `char* mg_connection_get_diagnostics(mg_core_t* core);` | — |
| `char* mg_connection_get_heartbeat_age(mg_core_t* core);` | — |
| `char* mg_connection_get_session_state(mg_core_t* core, int link_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_connection_get_state(mg_core_t* core);` | — |
| `char* mg_connection_list(mg_core_t* core);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_connection_list_ports(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `int mg_connection_open(mg_core_t* core, const char* endpoint_uri);` | `flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_discover_devices(mg_core_t* core, const char* transport_type);` | — |
| `char* mg_send_heartbeat(mg_core_t* core);` | — |
| `char* mg_stream_request(mg_core_t* core, int stream_id, int rate_hz);` | — |
| `char* mg_stream_set_interval(mg_core_t* core, int msg_id, int interval_us);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_clear_signing_candidates(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_clear_signing_key(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_vehicle_close(mg_core_t* core, int vehicle_id);` | — |
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
| `void mg_free_string(char* str);` | `flutter_shell/lib/connection/calibration_ffi_backend.dart`<br>`flutter_shell/lib/connection/flight_command_ffi_backend.dart`<br>`flutter_shell/lib/connection/native_core.dart`<br>`flutter_shell/lib/connection/parameter_ffi_backend.dart`<br>`flutter_shell/lib/connection/telemetry_ffi_backend.dart`<br>`flutter_shell/lib/fleet/fleet_ffi_backend.dart`<br>`flutter_shell/lib/logging/file_ffi_backend.dart`<br>`flutter_shell/lib/logging/log_ffi_backend.dart`<br>`flutter_shell/lib/logging/tlog_ffi_backend.dart`<br>`flutter_shell/lib/mission/mission_document_ffi_backend.dart`<br>`flutter_shell/lib/payload/payload_ffi_backend.dart`<br>`flutter_shell/lib/survey/survey_ffi_backend.dart`<br>`flutter_shell/lib/terrain/terrain_ffi_backend.dart` |
| `mg_error_t mg_get_last_error(void);` | — |
| `char* mg_get_version(void);` | — |
| `char* mg_operations_cancel(mg_core_t* core, const char* operation_id);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_operations_cancel_for_vehicle( mg_core_t* core, int vehicle_id, const char* operation_id);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_operations_get_snapshot(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_operations_get_snapshot_for_vehicle( mg_core_t* core, int vehicle_id);` | — |
| `void mg_set_event_callback(mg_core_t* core, mg_event_callback_t cb, void* user_data);` | — |
| `void mg_set_last_error(mg_error_t error);` | — |
| `char* mg_system_get_diagnostics(mg_core_t* core);` | — |

## ffi_api_environment.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_sitl_start(mg_core_t* core, const char* json_config);` | — |
| `char* mg_sitl_status(mg_core_t* core);` | — |
| `char* mg_sitl_stop(mg_core_t* core);` | — |
| `char* mg_terrain_cache_clear(mg_core_t* core);` | `flutter_shell/lib/terrain/terrain_ffi_backend.dart` |
| `char* mg_terrain_cache_update(mg_core_t* core, const char* json_config);` | `flutter_shell/lib/terrain/terrain_ffi_backend.dart` |
| `char* mg_terrain_query(mg_core_t* core, double lat, double lon);` | `flutter_shell/lib/terrain/terrain_ffi_backend.dart` |
| `char* mg_terrain_query_line(mg_core_t* core, double lat1, double lon1, double lat2, double lon2, int samples);` | `flutter_shell/lib/terrain/terrain_ffi_backend.dart` |

## ffi_api_firmware.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_firmware_flash(mg_core_t* core, const char* firmware_path);` | — |
| `char* mg_firmware_flash_status(mg_core_t* core);` | — |
| `char* mg_firmware_get_type(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_firmware(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_request_firmware_version(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |

## ffi_api_flight.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_battery_failsafe_configure(mg_core_t* core, int low_voltage_action, float low_voltage_threshold, float critical_voltage_threshold);` | — |
| `char* mg_command_arm(mg_core_t* core, int force);` | — |
| `char* mg_command_change_altitude(mg_core_t* core, float altitude_change_m);` | — |
| `char* mg_command_change_heading(mg_core_t* core, float heading_deg, float yaw_rate_dps, int direction);` | — |
| `char* mg_command_disarm(mg_core_t* core, int force);` | — |
| `char* mg_command_execute_for_vehicle(mg_core_t* core, int vehicle_id, const char* command_json);` | — |
| `char* mg_command_guided_pause(mg_core_t* core);` | — |
| `char* mg_command_guided_point(mg_core_t* core, double lat, double lon, float alt, float yaw, float speed);` | — |
| `char* mg_command_guided_velocity(mg_core_t* core, float vx, float vy, float vz);` | — |
| `char* mg_command_land(mg_core_t* core);` | — |
| `char* mg_command_landing_gear(mg_core_t* core, bool deploy);` | — |
| `char* mg_command_orbit(mg_core_t* core, const char* json_config);` | — |
| `char* mg_command_poll(mg_core_t* core, uint64_t ticket);` | `flutter_shell/lib/connection/flight_command_ffi_backend.dart` |
| `char* mg_command_rtl(mg_core_t* core);` | — |
| `char* mg_command_set_global_origin(mg_core_t* core, double lat, double lon, float alt_m);` | — |
| `char* mg_command_set_home(mg_core_t* core, double lat, double lon, float alt_m);` | — |
| `char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode);` | — |
| `char* mg_command_set_roi(mg_core_t* core, double lat, double lon, float alt);` | — |
| `char* mg_command_stop_roi(mg_core_t* core);` | — |
| `char* mg_command_submit_for_vehicle(mg_core_t* core, int vehicle_id, const char* command_json);` | `flutter_shell/lib/connection/flight_command_ffi_backend.dart` |
| `char* mg_command_takeoff(mg_core_t* core, float altitude_m);` | — |
| `char* mg_command_terminate_flight(mg_core_t* core);` | — |
| `char* mg_failsafe_get_status(mg_core_t* core);` | — |
| `char* mg_flight_get_available_modes(mg_core_t* core);` | — |
| `char* mg_flight_get_loiter_radius(mg_core_t* core);` | — |
| `char* mg_flight_get_mode_history(mg_core_t* core);` | — |
| `char* mg_flight_get_mode_mapping(mg_core_t* core);` | — |
| `char* mg_flight_get_mode_status(mg_core_t* core);` | — |
| `char* mg_flight_get_modes(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_flight_get_takeoff_altitude(mg_core_t* core);` | — |
| `char* mg_flight_set_loiter_radius(mg_core_t* core, float radius_m);` | — |
| `char* mg_flight_set_mode(mg_core_t* core, const char* mode_name);` | — |
| `char* mg_flight_set_takeoff_altitude(mg_core_t* core, float altitude_m);` | — |
| `char* mg_preflight_check(mg_core_t* core);` | — |

## ffi_api_geofence.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_fence_clear(mg_core_t* core);` | — |
| `char* mg_fence_download(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_fence_enable(mg_core_t* core, int enable);` | — |
| `char* mg_fence_get_plan(mg_core_t* core);` | — |
| `char* mg_geofence_add_point(mg_core_t* core, double lat, double lon, float alt);` | — |
| `char* mg_geofence_check(mg_core_t* core, double lat, double lon);` | — |
| `char* mg_geofence_export_json(mg_core_t* core);` | — |
| `char* mg_geofence_export_kml(mg_core_t* core, const char* name);` | — |
| `char* mg_geofence_get_config(mg_core_t* core);` | — |
| `char* mg_geofence_get_points(mg_core_t* core);` | — |
| `char* mg_geofence_import_json(mg_core_t* core, const char* json);` | — |
| `char* mg_geofence_remove_point(mg_core_t* core, int index);` | — |
| `char* mg_geofence_set_config(mg_core_t* core, const char* json_config);` | — |
| `char* mg_geofence_validate(mg_core_t* core);` | — |

## ffi_api_logging.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_event_log_get_events(mg_core_t* core);` | — |
| `char* mg_event_log_get_status(mg_core_t* core);` | — |
| `char* mg_event_log_start(mg_core_t* core);` | — |
| `char* mg_event_log_stop(mg_core_t* core);` | — |
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
| `char* mg_session_log_start(mg_core_t* core, const char* session_name);` | — |
| `char* mg_session_log_status(mg_core_t* core);` | — |
| `char* mg_session_log_stop(mg_core_t* core);` | — |
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
| `char* mg_mission_add_item(mg_core_t* core, const char* json);` | — |
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
| `char* mg_mission_export_csv(mg_core_t* core);` | — |
| `char* mg_mission_export_json(mg_core_t* core);` | — |
| `char* mg_mission_get_command_catalog(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_get_current(mg_core_t* core);` | — |
| `char* mg_mission_get_execution_progress(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_mission_get_plan(mg_core_t* core);` | — |
| `char* mg_mission_get_progress(mg_core_t* core);` | — |
| `char* mg_mission_get_stats(mg_core_t* core);` | — |
| `char* mg_mission_get_templates(mg_core_t* core);` | — |
| `char* mg_mission_import_csv(mg_core_t* core, const char* csv);` | — |
| `char* mg_mission_import_json(mg_core_t* core, const char* json);` | — |
| `char* mg_mission_move_item(mg_core_t* core, int from_index, int to_index);` | — |
| `char* mg_mission_remove_item(mg_core_t* core, int index);` | — |
| `char* mg_mission_set_current(mg_core_t* core, int seq);` | — |
| `char* mg_mission_start(mg_core_t* core);` | — |
| `char* mg_mission_update_item(mg_core_t* core, const char* json);` | — |
| `char* mg_mission_upload(mg_core_t* core, const char* json_wps);` | — |
| `char* mg_mission_validate(mg_core_t* core);` | — |
| `char* mg_rally_clear(mg_core_t* core);` | — |
| `char* mg_rally_download(mg_core_t* core);` | `flutter_shell/lib/mission/mission_document_ffi_backend.dart` |
| `char* mg_rally_get_plan(mg_core_t* core);` | — |
| `char* mg_survey_generate(mg_core_t* core, const char* json_config);` | `flutter_shell/lib/survey/survey_ffi_backend.dart` |
| `char* mg_survey_generate_corridor(mg_core_t* core, const char* json_config);` | `flutter_shell/lib/survey/survey_ffi_backend.dart` |

## ffi_api_params.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_config_get_metadata_coverage(mg_core_t* core);` | — |
| `char* mg_config_read_param(mg_core_t* core, const char* param_name);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_config_write_param(mg_core_t* core, const char* param_name, float value);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_config_write_params(mg_core_t* core, const char* json_params);` | — |
| `char* mg_param_file_export(mg_core_t* core, const char* json);` | — |
| `char* mg_param_file_export_csv(mg_core_t* core, const char* json);` | — |
| `char* mg_param_file_import(mg_core_t* core, const char* content);` | — |
| `char* mg_param_file_import_csv(mg_core_t* core, const char* content);` | — |
| `char* mg_params_commit_change_set(mg_core_t* core, const char* json_params);` | — |
| `char* mg_params_get_all(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_by_name(mg_core_t* core, const char* param_name);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_changed(mg_core_t* core);` | — |
| `char* mg_params_get_progress(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_get_refresh_status(mg_core_t* core);` | — |
| `char* mg_params_get_write_status(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_load_from_file(mg_core_t* core, const char* filepath);` | — |
| `char* mg_params_prepare_change_set(mg_core_t* core, const char* json_params);` | — |
| `char* mg_params_refresh(mg_core_t* core, const char* json_names);` | — |
| `char* mg_params_request_all(mg_core_t* core);` | `flutter_shell/lib/connection/parameter_ffi_backend.dart` |
| `char* mg_params_save_to_file(mg_core_t* core, const char* filepath);` | — |
| `char* mg_params_set(mg_core_t* core, const char* param_name, float param_value, int param_type);` | — |

## ffi_api_payload.h

| 声明 | Flutter 绑定位置 |
| --- | --- |
| `char* mg_camera_get_status(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_request_info(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_set_focus(mg_core_t* core, int camera_id, int focus_type, float focus_value);` | — |
| `char* mg_camera_set_mode(mg_core_t* core, int camera_id, int mode);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_set_zoom(mg_core_t* core, int camera_id, float zoom_level);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_start_video(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_stop_capture(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_stop_video(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_camera_take_photo(mg_core_t* core, int camera_id);` | `flutter_shell/lib/payload/payload_ffi_backend.dart` |
| `char* mg_command_gimbal_configure(mg_core_t* core, int gimbal_device_id, int primary_sys, int primary_comp, int secondary_sys, int secondary_comp);` | — |
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
| `char* mg_health_get_all(mg_core_t* core);` | — |
| `char* mg_rc_get_channel(mg_core_t* core, int channel);` | — |
| `char* mg_rc_get_channels(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_sensor_decode_device_id(mg_core_t* core, int device_id, const char* category);` | — |
| `char* mg_sensor_decode_px4_id(mg_core_t* core, int device_id, const char* category);` | — |
| `char* mg_sensor_decode_px4_type(mg_core_t* core, int type_value, const char* category);` | — |
| `char* mg_sensor_decode_type_param(mg_core_t* core, int type_value, const char* category);` | — |
| `char* mg_sensors_get_health(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_sensors_get_statustext(mg_core_t* core);` | — |
| `char* mg_sensors_identify_all(mg_core_t* core);` | — |
| `char* mg_servo_get_output(mg_core_t* core, int channel);` | — |
| `char* mg_servo_get_outputs(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_telemetry_append(mg_core_t* core, const char* name, float value);` | — |
| `char* mg_telemetry_clear(mg_core_t* core);` | — |
| `char* mg_telemetry_get_dashboard_snapshot(mg_core_t* core);` | — |
| `char* mg_telemetry_get_freshness(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_telemetry_get_messages(mg_core_t* core, unsigned long long since_seq);` | `flutter_shell/lib/connection/native_core.dart` |
| `char* mg_telemetry_get_series(mg_core_t* core, const char* name);` | — |
| `char* mg_telemetry_get_series_names(mg_core_t* core);` | — |
| `char* mg_telemetry_get_stats(mg_core_t* core, const char* name);` | — |
| `char* mg_telemetry_register_series(mg_core_t* core, const char* name, const char* unit);` | — |
| `char* mg_vehicle_count(mg_core_t* core);` | — |
| `char* mg_vehicle_get_adsb_traffic(mg_core_t* core);` | — |
| `char* mg_vehicle_get_attitude(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_battery(mg_core_t* core);` | — |
| `char* mg_vehicle_get_distance_sensors(mg_core_t* core);` | — |
| `char* mg_vehicle_get_esc_status(mg_core_t* core);` | — |
| `char* mg_vehicle_get_extended_sys_state(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_firmware_version(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_flight_time(mg_core_t* core);` | — |
| `char* mg_vehicle_get_global_position(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_gps(mg_core_t* core);` | — |
| `char* mg_vehicle_get_highres_imu(mg_core_t* core);` | — |
| `char* mg_vehicle_get_home_position(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_imu(mg_core_t* core);` | — |
| `char* mg_vehicle_get_prearm_check(mg_core_t* core);` | — |
| `char* mg_vehicle_get_pressure(mg_core_t* core);` | — |
| `char* mg_vehicle_get_rc_channels(mg_core_t* core);` | — |
| `char* mg_vehicle_get_servo_output(mg_core_t* core);` | — |
| `char* mg_vehicle_get_session_status(mg_core_t* core, int vehicle_id);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart`<br>`flutter_shell/lib/fleet/fleet_ffi_backend.dart` |
| `char* mg_vehicle_get_summary(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_vfr_hud(mg_core_t* core);` | `flutter_shell/lib/connection/telemetry_ffi_backend.dart` |
| `char* mg_vehicle_get_vibration(mg_core_t* core);` | — |
| `char* mg_vehicle_get_wind_estimate(mg_core_t* core);` | — |
| `char* mg_waveform_clear(mg_core_t* core);` | — |
| `char* mg_waveform_get_instances(mg_core_t* core);` | `flutter_shell/lib/connection/native_core.dart` |
| `int mg_waveform_poll(mg_core_t* core, float* out_points, int out_max_points, int target_points, int window_ms, int instance);` | `flutter_shell/lib/connection/native_core.dart` |

## 已定性的公开导出

这些符号保留在 ABI 中，但定位已经明确，没有前端 UI 计划：**内部能力**只服务
开发、离线分析或原生测试；**被取代**是被更新的接口取代的历史路径（退役候选，不删除）。
`tools/check_ffi_contract.py` 会在它们被 Flutter 绑定后报错，要求同步定位
（见脚本的 `INTERNAL_SYMBOL_NOTES` 与 `SUPERSEDED_SYMBOL_NOTES`）。

| 符号 | 类别 | 定位 |
| --- | --- | --- |
| `mg_actuator_get_limits` | 内部能力 | 电调/舵机通道限位；内部能力（通用参数页覆盖 SERVOx_MIN/MAX/TRIM/REVERSED） |
| `mg_actuator_set_function` | 内部能力 | 电调/舵机通道功能映射；内部能力（通用参数页覆盖 SERVOx_FUNCTION / ACTUATORx_FUNCTION） |
| `mg_actuator_set_limits` | 内部能力 | 电调/舵机通道限位写入；内部能力（通用参数页覆盖 SERVOx_MIN/MAX/TRIM） |
| `mg_config_validate_param` | 内部能力 | 参数值预校验；内部能力（前端按描述符范围/枚举本地校验，不再单独调用） |
| `mg_connection_connect_tcp` | 被取代 | 旧 TCP 连接入口（内部还会 mg_vehicle_open 抢焦点）；被 mg_connection_open 取代 |
| `mg_error_string` | 内部能力 | ABI 基础设施：错误码→字符串常量，供原生调用方与测试使用，无 UI |
| `mg_fence_clear` | 被取代 | 旧围栏清除；被 mg_mission_doc_clear_fence 取代 |
| `mg_geofence_add_point` | 被取代 | 旧 GeofenceManager 直接编辑；被 mg_mission_doc_apply + mg_mission_doc_compile_fence/upload_fence 取代 |
| `mg_geofence_export_json` | 被取代 | 旧围栏导出；被 mg_mission_doc_export / mg_mission_doc_export_format 取代 |
| `mg_geofence_export_kml` | 被取代 | 旧围栏 KML 导出；被 mg_mission_doc_export_format 取代（无前端入口） |
| `mg_geofence_get_points` | 被取代 | 旧 GeofenceManager 读路径；被 mg_fence_download + mg_mission_doc_import_fence_from_vehicle 取代 |
| `mg_geofence_import_json` | 被取代 | 旧围栏导入；被 mg_mission_doc_import / mg_mission_doc_import_format 取代 |
| `mg_geofence_remove_point` | 被取代 | 旧 GeofenceManager 直接编辑；被 mg_mission_doc_apply 取代 |
| `mg_geofence_set_config` | 被取代 | 旧围栏配置写入；走通用参数页 FENCE_* / 任务文档 |
| `mg_geofence_validate` | 被取代 | 旧围栏校验；被 mg_mission_doc_validate 取代 |
| `mg_mission_add_item` | 被取代 | 旧 MissionManager 读-改-传路径；被 mg_mission_doc_apply 取代（并发调用有 lost-update 风险，见 ffi_mission_management.cc 文件头） |
| `mg_mission_export_csv` | 被取代 | 旧任务导出；被 mg_mission_doc_export_format("waypoints") 取代 |
| `mg_mission_export_json` | 被取代 | 旧任务导出；被 mg_mission_doc_export / mg_mission_doc_export_format 取代 |
| `mg_mission_get_stats` | 被取代 | 旧任务统计；被 mg_mission_doc_get / mg_mission_doc_estimate 取代 |
| `mg_mission_import_csv` | 被取代 | 旧任务导入；被 mg_mission_doc_import_format("waypoints") 取代 |
| `mg_mission_import_json` | 被取代 | 旧任务导入；被 mg_mission_doc_import / mg_mission_doc_import_format 取代 |
| `mg_mission_move_item` | 被取代 | 旧 MissionManager 读-改-传路径；被 mg_mission_doc_apply 取代 |
| `mg_mission_remove_item` | 被取代 | 旧 MissionManager 读-改-传路径；被 mg_mission_doc_apply 取代 |
| `mg_mission_start` | 被取代 | 同步命令（最长阻塞约 5 s）；界面线程改用 mg_command_execute_for_vehicle + 轮询 |
| `mg_mission_update_item` | 被取代 | 旧 MissionManager 读-改-传路径；被 mg_mission_doc_apply 取代 |
| `mg_mission_validate` | 被取代 | 旧任务校验；被 mg_mission_doc_validate 取代 |
| `mg_params_get_changed` | 被取代 | 实现与命名不符（返回全部参数而非变化项）；被前端按描述符默认值比较取代 |
| `mg_params_load_from_file` | 被取代 | 旧一体化导入（FFI 读磁盘 + 逐项下发）；被 mg_param_file_import + mg_params_commit_change_set 取代 |
| `mg_params_save_to_file` | 被取代 | 旧一体化导出（读活动机参数 + FFI 直接写磁盘）；被 mg_params_get_all + mg_param_file_export + 前端 Dart 文件写入取代（FFI 层不做阻塞磁盘 I/O） |
| `mg_px4_actuator_get_config` | 内部能力 | PX4 执行器映射；内部能力（通用参数页覆盖 PWM_MAIN_FUNC1..16） |
| `mg_px4_actuator_set_config` | 内部能力 | PX4 执行器映射；内部能力（通用参数页覆盖 PWM_MAIN_FUNC1..16） |
| `mg_px4_flight_mode_get_config` | 内部能力 | PX4 模式槽只读视图（COM_FLTMODE1..6）；写入走通用参数页，避免与 RC 模式通道脱节 |
| `mg_px4_safety_get` | 内部能力 | PX4 安全断路器；内部能力（按飞控实际 CBRK_* 缓存报告，不伪造固定字段） |
| `mg_px4_safety_set` | 内部能力 | PX4 安全断路器；内部能力（只写飞控实际存在的 CBRK_*） |
| `mg_px4_tuning_get` | 内部能力 | PX4 姿态/角速度 PID；内部能力（通用参数页覆盖 MC_* 参数） |
| `mg_px4_tuning_set` | 内部能力 | PX4 姿态/角速度 PID；内部能力（通用参数页覆盖 MC_* 参数） |
| `mg_rally_clear` | 被取代 | 旧 Rally 清除；被 mg_mission_doc_clear_rally 取代 |
| `mg_rc_calibrate_status` | 被取代 | 旧 RC 专用校准状态；被通用 mg_calibration_get_status（type=radio）取代 |
| `mg_rc_set_failsafe` | 内部能力 | RC 失联保护写入；内部能力（通用参数页覆盖 RC_FS_* / *FS_* 参数） |
| `mg_rc_set_mapping` | 内部能力 | RC 通道映射写入；内部能力（通用参数页覆盖 RCMAP_* / RC_MAP_*） |
| `mg_send_heartbeat` | 被取代 | 旧手动心跳；被 mg_connection_open 的连接状态模型取代 |
| `mg_serial_get_config` | 被取代 | 旧串口读接口；被通用参数页 SERIALx_* / SER_TELx_* 取代（mg_serial_get_configs/get_uart_names 仍在用） |
| `mg_serial_set_baud` | 被取代 | 旧串口写接口；被通用参数页 SERIALx_BAUD 取代 |
| `mg_serial_set_protocol` | 被取代 | 旧串口写接口；被通用参数页 SERIALx_PROTOCOL 取代 |
| `mg_sitl_start` | 内部能力 | SITL 启停只服务开发与离线分析（native 测试与手工脚本）；生产 UI 不提供仿真启停入口 |
| `mg_sitl_status` | 内部能力 | SITL 启停只服务开发与离线分析（native 测试与手工脚本）；生产 UI 不提供仿真启停入口 |
| `mg_sitl_stop` | 内部能力 | SITL 启停只服务开发与离线分析（native 测试与手工脚本）；生产 UI 不提供仿真启停入口 |
| `mg_stream_request` | 被取代 | 旧数据流请求；被 mg_stream_set_interval 取代 |
