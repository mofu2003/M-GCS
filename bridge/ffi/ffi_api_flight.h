#ifndef MGCORE_FFI_API_FLIGHT_H
#define MGCORE_FFI_API_FLIGHT_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Compatibility commands with simplified signatures. New consumers should use
 * the mg_command_* family so force/base/custom values remain explicit. */
MGCORE_API char* mg_cmd_arm(mg_core_t* core);
MGCORE_API char* mg_cmd_disarm(mg_core_t* core);
MGCORE_API char* mg_cmd_set_mode(mg_core_t* core, const char* mode);
MGCORE_API char* mg_cmd_rtl(mg_core_t* core);
MGCORE_API char* mg_cmd_takeoff(mg_core_t* core, float altitude);
MGCORE_API char* mg_cmd_land(mg_core_t* core);
MGCORE_API char* mg_cmd_loiter(mg_core_t* core);
MGCORE_API char* mg_cmd_guided(mg_core_t* core, double lat, double lon, float alt);

// Explicit-target command dispatch. JSON example:
// {"command":"rtl"} or {"command":"takeoff","args":{"altitude_m":30}}.
// Commands: rtl, land, takeoff, arm, disarm, set_mode, mission_start,
// guided_pause, change_altitude, set_home, guided_point, orbit, set_roi,
// change_heading, set_global_origin. Args use the existing command units.
// Numeric args must be finite/representable; integer args must be integral.
// The override is core-local/thread-local and never changes UI active focus.
MGCORE_API char* mg_command_execute_for_vehicle(mg_core_t* core,
                                                int vehicle_id,
                                                const char* command_json);

MGCORE_API char* mg_command_arm(mg_core_t* core, int force);
MGCORE_API char* mg_command_disarm(mg_core_t* core, int force);
MGCORE_API char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode);
// 飞行模式操作台接口。模式表按机型给（ArduPilot 用 Copter/Plane/Rover/Sub/Tracker 各自
// 的枚举号，PX4 用 main/sub 打包），与 MP 的 CMB_modes、QGC 的 flightMode 菜单同源。
// mg_flight_get_modes 返回 available / current_mode / armed / modes[{id,name,settable}]；
// mg_flight_set_mode 按名字切换，内部转成 base_mode + custom_mode 再发 DO_SET_MODE。
MGCORE_API char* mg_flight_get_modes(mg_core_t* core);
MGCORE_API char* mg_flight_set_mode(mg_core_t* core, const char* mode_name);
// 遥控通道 → 飞行模式的映射（对标 MP 的 ConfigFlightModes 页面）：读取
// FLTMODE1..6 / COM_FLTMODE1..6、FLTMODE_CH|MODE_CH，以及 Copter/Rover 的
// SIMPLE / SUPER_SIMPLE 位掩码。写回复用 mg_config_write_params。
MGCORE_API char* mg_flight_get_mode_mapping(mg_core_t* core);
MGCORE_API char* mg_command_rtl(mg_core_t* core);
MGCORE_API char* mg_command_takeoff(mg_core_t* core, float altitude_m);
MGCORE_API char* mg_command_land(mg_core_t* core);
MGCORE_API char* mg_command_emergency_stop(mg_core_t* core);
MGCORE_API char* mg_command_guided_point(mg_core_t* core,
                                         double lat, double lon, float alt,
                                         float yaw, float speed);
MGCORE_API char* mg_command_guided_velocity(mg_core_t* core, float vx, float vy, float vz);
MGCORE_API char* mg_command_guided_pause(mg_core_t* core);
// 在当前位置改变高度（QGC `APMFirmwarePlugin::guidedModeChangeAltitude`）。
// altitude_change_m 为正表示上升；底层用 LOCAL_OFFSET_NED 只偏移 z。
MGCORE_API char* mg_command_change_altitude(mg_core_t* core, float altitude_change_m);
// 设置飞控 home 点（MAV_CMD_DO_SET_HOME）。
//
// 会改变返航（RTL）基准，属于安全敏感操作：调用方应先向用户确认，
// 文案参考 QGC 的 “Set vehicle home as the specified location. This will
// affect Return to Home position”。
MGCORE_API char* mg_command_set_home(mg_core_t* core,
                                     double lat, double lon, float alt_m);
// 环绕飞行（QGC `Vehicle::guidedModeOrbit`）。json_config 字段：
//   lat/lon=环绕中心，radius=半径(带符号表示方向)，velocity<=0 表示用飞控默认，
//   altitude=**AMSL 高度**（QGC 用 home AMSL + 相对高度）。
MGCORE_API char* mg_command_orbit(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_command_set_roi(mg_core_t* core, double lat, double lon, float alt);
MGCORE_API char* mg_command_stop_roi(mg_core_t* core);
// 机头转向点击方向（QGC 地图菜单的 “Set Heading”）。
//
// heading_deg 为相对当前航向的转角（度），direction 为 ±1，yaw_rate_dps 为
// 最大偏航速率；与 QGC `APMFirmwarePlugin::guidedModeChangeHeading` 发出的
// MAV_CMD_CONDITION_YAW 参数一致。仅多旋翼/潜航器适用。
MGCORE_API char* mg_command_change_heading(mg_core_t* core, float heading_deg,
                                           float yaw_rate_dps, int direction);
// 设置 EKF 全局原点（QGC 地图菜单的 “Set Estimator Origin”）。
//
// 无 GPS 的室内/视觉定位场景用它指定本地坐标原点的经纬高。
MGCORE_API char* mg_command_set_global_origin(mg_core_t* core, double lat,
                                              double lon, float alt_m);
MGCORE_API char* mg_command_landing_gear(mg_core_t* core, bool deploy);

MGCORE_API char* mg_flight_get_available_modes(mg_core_t* core);
MGCORE_API char* mg_flight_get_mode_status(mg_core_t* core);
MGCORE_API char* mg_flight_get_mode_history(mg_core_t* core);
MGCORE_API char* mg_flight_get_takeoff_altitude(mg_core_t* core);
MGCORE_API char* mg_flight_set_takeoff_altitude(mg_core_t* core, float altitude_m);
MGCORE_API char* mg_flight_get_loiter_radius(mg_core_t* core);
MGCORE_API char* mg_flight_set_loiter_radius(mg_core_t* core, float radius_m);

MGCORE_API char* mg_preflight_check(mg_core_t* core);
MGCORE_API char* mg_battery_failsafe_configure(mg_core_t* core,
                                                int low_voltage_action,
                                                float low_voltage_threshold,
                                                float critical_voltage_threshold);
MGCORE_API char* mg_failsafe_get_status(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_FLIGHT_H */
