#ifndef MGCORE_FFI_API_FLIGHT_H
#define MGCORE_FFI_API_FLIGHT_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

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

// Nonblocking explicit-target commands. Submit returns {success,ticket}; poll
// returns {success,ticket,state:"pending"} or state:"complete",result:{...}.
// A complete poll consumes the ticket. Core destruction drains all workers
// before releasing state.
//
// 并发规则（对标 QGC MavCommandQueue.cc:289-296 的 isPending(compId, command)）：
// 同一架飞机的**不同**命令可以并发下发，各有各的 ticket；只有"同一条命令尚未
// 收到 ACK 又重复下发"才返回 MG_ERROR_BUSY（两条相同命令无法分辨 ACK 归属）。
// 队列上限 32，全局共享。
MGCORE_API char* mg_command_submit_for_vehicle(mg_core_t* core, int vehicle_id,
                                               const char* command_json);
MGCORE_API char* mg_command_poll(mg_core_t* core, uint64_t ticket);

// 以下同步命令会发送并等待飞控的 COMMAND_ACK（safety.md 的 S2），最长阻塞约 5 s，
// 期间调用线程被占用。界面线程不得直接调用——请改用非阻塞的
// mg_command_submit_for_vehicle + mg_command_poll。无 COMMAND_ACK 的引导命令
// （改变高度/引导速度/引导暂停/环绕/ROI/改航向/设原点）发送后立即返回。
MGCORE_API char* mg_command_arm(mg_core_t* core, int force);
MGCORE_API char* mg_command_disarm(mg_core_t* core, int force);
MGCORE_API char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode);
// 飞行模式操作台接口。模式表按机型给（Copter/Plane/Rover/Sub/Tracker 各自的
// 枚举号），与 MP 的 CMB_modes、QGC 的 flightMode 菜单同源。
// mg_flight_get_modes 返回 available / current_mode / armed / modes[{id,name,settable}]，
// 以及 heartbeat_fresh / heartbeat_age_ms：heartbeat_fresh 为 false 时 armed 与
// current_mode 来自最后一帧，调用方必须按“未知”处理，不能当成当前值（S5）。
// mg_flight_set_mode 按名字切换，内部转成 base_mode + custom_mode，并等待
// COMMAND_ACK 后才返回成功（safety.md 的 S2）。它是同步调用，最长等待约 5 s，
// 界面线程应改用 mg_command_submit_for_vehicle + mg_command_poll。
MGCORE_API char* mg_flight_get_modes(mg_core_t* core);
MGCORE_API char* mg_flight_set_mode(mg_core_t* core, const char* mode_name);
// 遥控通道 → 飞行模式的映射（对标 MP 的 ConfigFlightModes 页面）：读取
// FLTMODE1..6、通道参数 FLTMODE_CH|MODE_CH，以及 Copter/Rover 的
// SIMPLE / SUPER_SIMPLE 位掩码。写回复用 mg_config_write_params。
MGCORE_API char* mg_flight_get_mode_mapping(mg_core_t* core);
MGCORE_API char* mg_command_rtl(mg_core_t* core);
MGCORE_API char* mg_command_takeoff(mg_core_t* core, float altitude_m);
MGCORE_API char* mg_command_land(mg_core_t* core);
// 终止飞行：下发 MAV_CMD_DO_FLIGHTTERMINATION（param1=1）。飞控立即切断动力，
// 空中执行等于坠机，不可撤销。
//
// 它**不是**"上锁/紧急停桨"（那是 COMPONENT_ARM_DISARM + param2=21196，可恢复）。
// 界面上的任何入口都必须按"终止飞行"如实标注并做二次确认。当前决定：
// core 保留能力、界面不暴露（QGC 全仓不引用该命令；MP 在 Actions 下拉里以
// Terminate Flight 暴露并要求一次 Yes/No 确认）。
MGCORE_API char* mg_command_terminate_flight(mg_core_t* core);
// 注意：SET_POSITION_TARGET_* / DO_REPOSITION 之外的引导类命令（改变高度、
// 引导速度、引导暂停、环绕、ROI、改变航向、设置原点）在 MAVLink 上**没有
// COMMAND_ACK**。这些入口返回的 success 只表示“已交给发送层”，不代表飞控已
// 执行；界面不要把它展示成执行成功。有 ACK 的命令见 mg_command_* 的 ACK 队列。
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
// 发 MAV_CMD_DO_SET_GLOBAL_ORIGIN（COMMAND_INT + MAV_FRAME_GLOBAL），
// 只有飞控回 COMMAND_ACK=ACCEPTED 才返回 success（S2）；拒绝与无 ACK 超时如实失败。
// 最长阻塞约 5 s。
MGCORE_API char* mg_command_set_global_origin(mg_core_t* core, double lat,
                                              double lon, float alt_m);
// 收放起落架。发 MAV_CMD_AIRFRAME_CONFIGURATION，同样等 COMMAND_ACK；
// 只有确认才返回 success（S2）。最长阻塞约 5 s。
MGCORE_API char* mg_command_landing_gear(mg_core_t* core, bool deploy);

MGCORE_API char* mg_flight_get_available_modes(mg_core_t* core);
/// 当前模式/解锁/是否在飞，附 heartbeat_fresh 与 heartbeat_age_ms；
/// heartbeat_fresh 为 false 时这些值已过期，界面显示为未知而不是当前值（S5）。
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
