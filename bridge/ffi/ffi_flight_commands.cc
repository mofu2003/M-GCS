#define MGCORE_EXPORTS
/**
 * ffi_flight_commands.cc - Flight command implementations
 *
 * 飞行指令：解锁/上锁、起降、模式切换、RTL、引导模式、紧急停止。
 *
 * arm, disarm, takeoff, land, set_mode, RTL, terminate_flight, guided_*
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstring>
#include <chrono>
#include <thread>

// cJSON
#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

namespace {

/// MAV_RESULT (rejection) -> symbolic code used in the FFI error object.
const char* ack_result_code(int result) {
    switch (result) {
        case 1: return "temporarily_rejected";
        case 2: return "denied";
        case 3: return "unsupported";
        case 4: return "failed";
        case 5: return "in_progress";
        default: return "unknown";
    }
}

}  // namespace

/// Flight controller rejected the command: canonical error object plus the
/// ACK detail the UI needs to explain why.
static char* ack_rejection_json(const mg::core::mavlink::CommandAckInfo& ack) {
    const char* code = ack_result_code(static_cast<int>(ack.result));
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    write_error(jb, MG_ERROR_REJECTED,
                std::string("flight controller rejected the command: ") + code);
    jb.field("command", static_cast<int>(ack.command));
    jb.field("result", static_cast<int>(ack.result));
    jb.field("progress", static_cast<int>(ack.progress));
    jb.field("result_str", code);
    jb.field("result_text", code);
    jb.end_object();
    return to_cstr(jb.str());
}

/// 命令队列辅助函数：发送命令并阻塞等待 ACK
/// 使用 send_command_queued() + wait_for_command()，支持超时重发
char* ffi_cmd_result_queued(mg_core_t* core, mg::core::mavlink::MavlinkAdapter* mav,
                            uint16_t cmd_id, uint8_t sys, uint8_t comp,
                            float p1, float p2, float p3, float p4,
                            float p5, float p6, float p7, int max_wait_ms) {
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    auto ticket = mav->send_command_queued(sys, comp, cmd_id,
        p1, p2, p3, p4, p5, p6, p7);
    auto ack = mav->wait_for_command(ticket, max_wait_ms);
    if (ack.has_value()) {
        if (ack->result_text.find("timeout") != std::string::npos) {
            return fail(MG_ERROR_TIMEOUT, ack->result_text.c_str());
        }
        if (ack->result == 0) return ok();  // MAV_RESULT_ACCEPTED
        if (ack->result == 5) {  // MAV_RESULT_IN_PROGRESS: still running, not an error
            return to_cstr(mg::core::telemetry::command_ack_json(ack.value()));
        }
        return ack_rejection_json(ack.value());
    }
    mav->cancel_command(ticket);  // Do not send a delayed retry after reporting failure.
    return fail(MG_ERROR_TIMEOUT, "command ack timeout");
}

/// COMMAND_INT 版本：ArduPilot 的 DO_REPOSITION 不接受 COMMAND_LONG。
static char* cmd_int_result_queued(
        mg_core_t* core, mg::core::mavlink::MavlinkAdapter* mav,
        uint16_t cmd_id, uint8_t sys, uint8_t comp, uint8_t frame,
        float p1, float p2, float p3, float p4,
        int32_t x, int32_t y, float z, int max_wait_ms = 5000) {
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    auto ticket = mav->send_command_int_queued(
        sys, comp, cmd_id, frame, p1, p2, p3, p4, x, y, z);
    auto ack = mav->wait_for_command(ticket, max_wait_ms);
    if (ack.has_value()) {
        if (ack->result_text.find("timeout") != std::string::npos) {
            return fail(MG_ERROR_TIMEOUT, ack->result_text.c_str());
        }
        if (ack->result == 0) return ok();  // MAV_RESULT_ACCEPTED
        if (ack->result == 5) {  // MAV_RESULT_IN_PROGRESS: still running, not an error
            return to_cstr(mg::core::telemetry::command_ack_json(ack.value()));
        }
        return ack_rejection_json(ack.value());
    }
    mav->cancel_command(ticket);  // Do not send a delayed retry after reporting failure.
    return fail(MG_ERROR_TIMEOUT, "command ack timeout");
}

MGCORE_API char* mg_command_arm(mg_core_t* core, int force) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    float force_param = force ? 21196.0f : 0.0f;
    auto* result = ffi_cmd_result_queued(core, mav, MAV_CMD_COMPONENT_ARM_DISARM,
        active_target_sys(core), active_target_comp(core),
        1.0f, force_param, 0, 0, 0, 0, 0);
    get_active_flight_log(core).on_command_result("arm", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_disarm(mg_core_t* core, int force) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    float force_param = force ? 21196.0f : 0.0f;
    auto* result = ffi_cmd_result_queued(core, mav, MAV_CMD_COMPONENT_ARM_DISARM,
        active_target_sys(core), active_target_comp(core),
        0.0f, force_param, 0, 0, 0, 0, 0);
    get_active_flight_log(core).on_command_result("disarm", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return ffi_cmd_result_queued(core, mav, MAV_CMD_DO_SET_MODE,
        active_target_sys(core), active_target_comp(core),
        static_cast<float>(base_mode), static_cast<float>(custom_mode),
        0, 0, 0, 0, 0);
}

MGCORE_API char* mg_command_rtl(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    auto* result = ffi_cmd_result_queued(core, mav, MAV_CMD_NAV_RETURN_TO_LAUNCH,
        active_target_sys(core), active_target_comp(core),
        0, 0, 0, 0, 0, 0, 0);
    get_active_flight_log(core).on_command_result("rtl", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

// 起飞。PX4 与 ArduPilot 的高度语义不同：
//   - ArduPilot：`NAV_TAKEOFF` 的 param7 是**相对 Home 的高度**（QGC APM 同）。
//   - PX4：QGC `PX4FirmwarePlugin::guidedModeTakeoff`（:322-350）把界面给的相对
//     高度加上当前 MSL 高度后写进 param7（AMSL），并在拿不到 MSL 高度时直接拒绝。
MGCORE_API char* mg_command_takeoff(mg_core_t* core, float altitude_m) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    float param7 = altitude_m;
    if (ffi_is_px4(core)) {
        auto* cache = get_active_cache(core);
        if (!cache) return fail(MG_ERROR_INVALID_STATE, "no telemetry cache");
        const auto gp_snap = cache->snapshot_global_pos();
        const auto& gp = gp_snap.data;
        // GLOBAL_POSITION_INT 从未收到时缓存里是全零，与 global_position JSON 的
        // valid 判定保持一致：没有有效位置就拒绝，而不是发一个假的 AMSL 高度。
        if (gp.lat == 0 && gp.lon == 0) {
            return fail(MG_ERROR_INVALID_STATE,
                        "PX4 takeoff requires a known MSL altitude");
        }
        param7 = altitude_m + static_cast<float>(gp.alt / 1000.0);
    }
    return ffi_cmd_result_queued(core, mav, MAV_CMD_NAV_TAKEOFF,
        active_target_sys(core), active_target_comp(core),
        0, 0, 0, 0, 0, 0, param7);
}

MGCORE_API char* mg_command_land(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return ffi_cmd_result_queued(core, mav, MAV_CMD_NAV_LAND,
        active_target_sys(core), active_target_comp(core),
        0, 0, 0, 0, 0, 0, 0);
}

MGCORE_API char* mg_command_terminate_flight(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return ffi_cmd_result_queued(core, mav, MAV_CMD_DO_FLIGHTTERMINATION,
        active_target_sys(core), active_target_comp(core),
        1.0f, 0, 0, 0, 0, 0, 0);
}

// ================================================================
// GUIDED mode commands
// ================================================================

MGCORE_API char* mg_command_guided_point(mg_core_t* core,
                                         double lat, double lon, float alt,
                                         float yaw, float speed) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    float p1 = speed > 0 ? speed : -1.0f;
    float p4 = (yaw >= 0) ? yaw : NAN;
    const int32_t lat_e7 = static_cast<int32_t>(lat * 1e7);
    const int32_t lon_e7 = static_cast<int32_t>(lon * 1e7);
    // QGC APMFirmwarePlugin::guidedModeGotoLocation uses COMMAND_INT with
    // MAV_FRAME_GLOBAL and an AMSL altitude. ArduPilot rejects the same
    // command over COMMAND_LONG because COMMAND_LONG carries no frame.
    auto* result = cmd_int_result_queued(
        core, mav, MAV_CMD_DO_REPOSITION,
        active_target_sys(core), active_target_comp(core),
        MAV_FRAME_GLOBAL, p1, 1.0f, 0, p4,
        lat_e7, lon_e7, alt);
    get_active_flight_log(core).on_command_result("guided_point", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_change_altitude(mg_core_t* core, float altitude_change_m) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (isnan(altitude_change_m)) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid altitude change");
    }
    auto* mav = get_active_mavlink(core);
    const bool sent = mav->cmd_change_altitude(active_target_sys(core), active_target_comp(core),
                                               altitude_change_m);
    get_active_flight_log(core).on_command_result("change_altitude", sent, sent ? "sent" : "send_failed");
    // SET_POSITION_TARGET_LOCAL_NED 没有 COMMAND_ACK，发出即算成功。
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "change altitude send failed");
}

MGCORE_API char* mg_command_guided_velocity(mg_core_t* core,
                                            float vx, float vy, float vz) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    bool sent = mav->cmd_guided_velocity(active_target_sys(core), active_target_comp(core),
                                          vx, vy, vz);
    get_active_flight_log(core).on_command_result("guided_velocity", sent, sent ? "sent" : "send_failed");
    // SET_POSITION_TARGET_GLOBAL_INT doesn't have a COMMAND_ACK, return ok if sent
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "send failed");
}

// 设置 home 点。参数与 QGC 的 doSetHome / MP 的 setHomeHere 一致：
// param1 = 0（用给定位置，而不是当前位置）、param2..4 = 0、
// param5 = 纬度（度）、param6 = 经度（度）、param7 = 高度（米）。
// QGC 在发送前会先弹确认框，因为这会改变 RTL 基准。
MGCORE_API char* mg_command_set_home(mg_core_t* core,
                                     double lat, double lon, float alt_m) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!(lat >= -90.0 && lat <= 90.0) || !(lon >= -180.0 && lon <= 180.0)) {
        return fail(MG_ERROR_INVALID_PARAM, "home latitude or longitude out of range");
    }
    auto* mav = get_active_mavlink(core);
    auto* result = ffi_cmd_result_queued(core, mav, MAV_CMD_DO_SET_HOME,
        active_target_sys(core), active_target_comp(core),
        0.0f, 0.0f, 0.0f, 0.0f,
        static_cast<float>(lat), static_cast<float>(lon), alt_m);
    get_active_flight_log(core).on_command_result(
        "set_home", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_guided_pause(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    if (ffi_is_px4(core)) {
        // QGC `PX4FirmwarePlugin::pauseVehicle`（:276-288）：不是零速度
        // SET_POSITION_TARGET（那条会过期），而是 DO_REPOSITION +
        // MAV_DO_REPOSITION_FLAGS_CHANGE_MODE，让飞控切进 Auto Loiter 并停在原地。
        // param1 = -1 表示沿用当前速度，param3 = 0，param5/6/7 允许 NaN，因为
        // CHANGE_MODE 分支下飞控不会用这些位置参数。
        auto* result = ffi_cmd_result_queued(
            core, mav, MAV_CMD_DO_REPOSITION,
            active_target_sys(core), active_target_comp(core),
            -1.0f, static_cast<float>(MAV_DO_REPOSITION_FLAGS_CHANGE_MODE),
            0.0f, 0.0f,
            static_cast<float>(NAN), static_cast<float>(NAN),
            static_cast<float>(NAN));
        get_active_flight_log(core).on_command_result(
            "guided_pause", result && strstr(result, "\"success\":true"), "queued");
        return result;
    }
    bool sent = mav->cmd_guided_pause(active_target_sys(core), active_target_comp(core));
    get_active_flight_log(core).on_command_result("guided_pause", sent, sent ? "sent" : "send_failed");
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "send failed");
}

// ================================================================
// Extended flight commands
// ================================================================

MGCORE_API char* mg_command_orbit(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!json_config) return fail(MG_ERROR_INVALID_PARAM, "null config");
    cJSON* root = mg::core::json::parse(json_config);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    double lat = 0, lon = 0; float radius = 50, velocity = 5, alt = 50;
    cJSON* p;
    if ((p = cJSON_GetObjectItem(root, "lat")) && cJSON_IsNumber(p)) lat = p->valuedouble;
    if ((p = cJSON_GetObjectItem(root, "lon")) && cJSON_IsNumber(p)) lon = p->valuedouble;
    if ((p = cJSON_GetObjectItem(root, "radius")) && cJSON_IsNumber(p)) radius = (float)p->valuedouble;
    if ((p = cJSON_GetObjectItem(root, "velocity")) && cJSON_IsNumber(p)) velocity = (float)p->valuedouble;
    if ((p = cJSON_GetObjectItem(root, "altitude")) && cJSON_IsNumber(p)) alt = (float)p->valuedouble;
    cJSON_Delete(root);
    auto* mav = get_active_mavlink(core);
    if (mav->cmd_orbit(active_target_sys(core), active_target_comp(core), lat, lon, radius, velocity, alt))
        return ok();
    return fail(MG_ERROR_SEND_FAILED, "orbit command send failed");
}

MGCORE_API char* mg_command_set_roi(mg_core_t* core, double lat, double lon, float alt) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (get_active_mavlink(core)->cmd_set_roi(active_target_sys(core), active_target_comp(core), lat, lon, alt))
        return ok();
    return fail(MG_ERROR_SEND_FAILED, "ROI command send failed");
}

MGCORE_API char* mg_command_stop_roi(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (get_active_mavlink(core)->cmd_roi_stop(active_target_sys(core), active_target_comp(core)))
        return ok();
    return fail(MG_ERROR_SEND_FAILED, "ROI stop send failed");
}

MGCORE_API char* mg_command_change_heading(mg_core_t* core, float heading_deg,
                                           float yaw_rate_dps, int direction) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    const int8_t dir = direction < 0 ? -1 : 1;
    const bool sent = get_active_mavlink(core)->cmd_change_heading(
        active_target_sys(core), active_target_comp(core),
        heading_deg, yaw_rate_dps, dir, true /* relative */);
    get_active_flight_log(core).on_command_result("change_heading", sent, sent ? "sent" : "send_failed");
    if (!sent) return fail(MG_ERROR_SEND_FAILED, "change heading send failed");
    return ok();
}

/// MAV_CMD_DO_SET_GLOBAL_ORIGIN = 48（旧名 MAV_CMD_SET_GPS_GLOBAL_ORIGIN）。
/// 仓库里的 MAVLink 方言快照早于该命令改名，所以这里有字面量；命令语义按
/// MAVLink common.xml 固定，不需要按固件区分。
constexpr uint16_t kSetGlobalOriginCommand = 48;

MGCORE_API char* mg_command_set_global_origin(mg_core_t* core, double lat,
                                              double lon, float alt_m) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    // 与 QGC `Vehicle::setEstimatorOrigin` 一致：先做经纬度合理性检查，
    // 越界直接本地拒绝，不发无效坐标给飞控。
    if (!(lat >= -90.0 && lat <= 90.0) || !(lon >= -180.0 && lon <= 180.0) ||
        (lat == 0.0 && lon == 0.0)) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid origin lat/lon");
    }

    // QGC 首选带 COMMAND_ACK 的 MAV_CMD_DO_SET_GLOBAL_ORIGIN（COMMAND_INT +
    // MAV_FRAME_GLOBAL，经纬度用 degE7），ArduPilot（GCS_Common.cpp:5329、5712）
    // 与 PX4（Commander.cpp:1709-1710）都实现并回 ACK。只有飞控确认才算成功（S2）：
    // 原来发完已废弃的 SET_GPS_GLOBAL_ORIGIN 报文就回 success，界面把"已发出"
    // 显示成了成功。
    char* result = cmd_int_result_queued(
        core, get_active_mavlink(core), kSetGlobalOriginCommand,
        active_target_sys(core), active_target_comp(core), MAV_FRAME_GLOBAL,
        0.0f, 0.0f, 0.0f, 0.0f,
        static_cast<int32_t>(std::llround(lat * 1e7)),
        static_cast<int32_t>(std::llround(lon * 1e7)),
        alt_m);
    const bool confirmed =
        result != nullptr && std::strstr(result, "\"success\":true") != nullptr;
    get_active_flight_log(core).on_command_result(
        "set_global_origin", confirmed, confirmed ? "acknowledged" : "not_confirmed");
    return result;
}

MGCORE_API char* mg_command_landing_gear(mg_core_t* core, bool deploy) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    // MAV_CMD_AIRFRAME_CONFIGURATION 有 COMMAND_ACK，必须等飞控确认（S2）。
    return ffi_cmd_result_queued(core, get_active_mavlink(core),
        MAV_CMD_AIRFRAME_CONFIGURATION,
        active_target_sys(core), active_target_comp(core),
        deploy ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

namespace {
// MAV_CMD_MISSION_START + COMMAND_ACK 等待。同步命令，最长阻塞约 5 s，只在
// 内部工作流（启动任务、批量启动）里使用；界面线程走
// mg_command_submit_for_vehicle({"command":"mission_start"}) + poll。
char* mission_start_and_wait(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    return ffi_cmd_result_queued(core, get_active_mavlink(core), MAV_CMD_MISSION_START,
        active_target_sys(core), active_target_comp(core), 0, 0, 0, 0, 0, 0, 0);
}

bool response_succeeded(const char* result) {
    if (!result) return false;
    cJSON* object = mg::core::json::parse(result);
    const bool success = object && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object, "success"));
    cJSON_Delete(object);
    return success;
}

bool wait_for_vehicle_state(mg_core_t* core, std::optional<uint32_t> mode,
                            std::optional<bool> armed, int timeout_ms = 3500) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    do {
        const auto session = mg::bridge::ffi::resolve_vehicle_session(core);
        if (!session.connected || !session.mavlink) return false;
        const auto hb = session.mavlink->latest_heartbeat();
        if (hb && (!mode || hb->custom_mode == *mode) && (!armed || hb->armed == *armed)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

char* set_mode_and_validate(mg_core_t* core, uint32_t mode) {
    if (wait_for_vehicle_state(core, mode, {}, 0)) return ok();
    char* result = mg_command_set_mode(core, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, mode);
    if (!response_succeeded(result)) return result;
    mg_free_string(result);
    return wait_for_vehicle_state(core, mode, {}) ? ok()
        : fail(MG_ERROR_TIMEOUT, "flight mode was not confirmed by heartbeat");
}

char* arm_and_validate(mg_core_t* core) {
    if (wait_for_vehicle_state(core, {}, true, 0)) return ok();
    char* result = mg_command_arm(core, 0);
    if (!response_succeeded(result)) return result;
    mg_free_string(result);
    return wait_for_vehicle_state(core, {}, true) ? ok()
        : fail(MG_ERROR_TIMEOUT, "arming was not confirmed by heartbeat");
}

// Shared workflow for single/batch starts. QGC APMFirmwarePlugin::startMission
// uses AUTO before arming both Plane and VTOL (tilt-rotor safety); MP retains
// GUIDED before arming Copter. PX4FirmwarePlugin selects AUTO_MISSION.
char* start_mission_and_validate(mg_core_t* core, bool arm_if_needed) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    const auto hb = mav->latest_heartbeat();
    if (!hb) return fail(MG_ERROR_INVALID_STATE, "vehicle identity is not available");
    if (!arm_if_needed && !hb->armed)
        return fail(MG_ERROR_INVALID_STATE, "vehicle must already be armed for batch mission start");
    if (hb->autopilot_type != MAV_AUTOPILOT_ARDUPILOTMEGA && hb->autopilot_type != MAV_AUTOPILOT_PX4)
        return fail(MG_ERROR_NOT_SUPPORTED, "mission start workflow unsupported for this autopilot");
    const bool plane = hb->vehicle_type == MAV_TYPE_FIXED_WING ||
        (hb->vehicle_type >= MAV_TYPE_VTOL_TAILSITTER_DUOROTOR && hb->vehicle_type <= MAV_TYPE_VTOL_RESERVED5);
    const auto landed = mav->latest_extended_sys_state();
    const auto position = mav->latest_global_position();
    const bool flying = hb->armed && ((landed && landed->landed_state == MAV_LANDED_STATE_IN_AIR) ||
        (position && position->relative_alt > 2000));
    const uint32_t auto_mode = hb->autopilot_type == MAV_AUTOPILOT_PX4 ? 0x04040000U : plane ? 10U : 3U;
    if (hb->autopilot_type == MAV_AUTOPILOT_PX4 || plane || flying) {
        char* result = set_mode_and_validate(core, auto_mode);
        if (!response_succeeded(result)) return result;
        mg_free_string(result);
        return arm_and_validate(core);
    }
    if (!hb->armed) {
        char* result = set_mode_and_validate(core, 4U);
        if (!response_succeeded(result)) return result;
        mg_free_string(result);
        result = arm_and_validate(core);
        if (!response_succeeded(result)) return result;
        mg_free_string(result);
    }
    char* result = mission_start_and_wait(core);
    if (!response_succeeded(result)) return result;
    mg_free_string(result);
    return wait_for_vehicle_state(core, auto_mode, true) ? ok()
        : fail(MG_ERROR_TIMEOUT, "mission mode was not confirmed by heartbeat");
}
} // namespace

// ================================================================
// Explicit target command dispatch
// ================================================================
namespace {

bool json_number(cJSON* object, const char* key, double& result, bool required) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item) return !required;
    if (!cJSON_IsNumber(item)) return false;
    result = item->valuedouble;
    return std::isfinite(result) && std::abs(result) <= std::numeric_limits<float>::max();
}

bool json_int(cJSON* object, const char* key, int& result, bool required, int fallback = 0) {
    double value = static_cast<double>(fallback);
    if (!json_number(object, key, value, required)) return false;
    if (value < std::numeric_limits<int>::min() ||
        value > std::numeric_limits<int>::max() || std::trunc(value) != value) return false;
    result = static_cast<int>(value);
    return true;
}

char* invalid_args(const char* message) {
    return fail(MG_ERROR_INVALID_PARAM, message);
}

} // namespace

MGCORE_API char* mg_command_execute_for_vehicle(mg_core_t* core,
                                                int vehicle_id,
                                                const char* command_json) {
    CHECK_CORE(core);
    if (vehicle_id <= 0 || !command_json || !command_json[0]) {
        return invalid_args("invalid target or command");
    }
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "target vehicle not found");

    cJSON* root = mg::core::json::parse(command_json);
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return invalid_args("invalid command json");
    }
    cJSON* command_item = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (!cJSON_IsString(command_item) || !command_item->valuestring) {
        cJSON_Delete(root);
        return invalid_args("missing command name");
    }
    const std::string command(command_item->valuestring);
    cJSON* args = cJSON_GetObjectItemCaseSensitive(root, "args");
    if (args && !cJSON_IsObject(args)) {
        cJSON_Delete(root);
        return invalid_args("command args must be an object");
    }
    const bool owns_empty_args = args == nullptr;
    if (owns_empty_args) args = cJSON_CreateObject();

    mg::bridge::ffi::VehicleTargetScope target(core, vehicle.get());
    char* result = nullptr;
    double number = 0.0;
    int integer = 0;

    if (command == "rtl") {
        result = mg_command_rtl(core);
    } else if (command == "land") {
        result = mg_command_land(core);
    } else if (command == "takeoff") {
        if (!json_number(args, "altitude_m", number, true)) result = invalid_args("takeoff altitude required");
        else result = mg_command_takeoff(core, static_cast<float>(number));
    } else if (command == "arm") {
        if (!json_int(args, "force", integer, false, 0)) result = invalid_args("invalid arm force");
        else result = mg_command_arm(core, integer);
    } else if (command == "disarm") {
        if (!json_int(args, "force", integer, false, 0)) result = invalid_args("invalid disarm force");
        else result = mg_command_disarm(core, integer);
    } else if (command == "set_mode") {
        int base_mode = 1, custom_mode = 0;
        if (!json_int(args, "base_mode", base_mode, false, 1) ||
            !json_int(args, "custom_mode", custom_mode, true)) {
            result = invalid_args("invalid flight mode");
        } else {
            result = mg_command_set_mode(core, base_mode, custom_mode);
        }
    } else if (command == "start_mission") {
        int arm_if_needed = 1;
        if (!json_int(args, "arm_if_needed", arm_if_needed, false, 1) ||
            (arm_if_needed != 0 && arm_if_needed != 1)) result = invalid_args("invalid mission arming policy");
        else result = start_mission_and_validate(core, arm_if_needed != 0);
    } else if (command == "mission_start") {
        result = mission_start_and_wait(core);
    } else if (command == "guided_pause") {
        result = mg_command_guided_pause(core);
    } else if (command == "change_altitude") {
        if (!json_number(args, "altitude_change_m", number, true)) result = invalid_args("altitude change required");
        else result = mg_command_change_altitude(core, static_cast<float>(number));
    } else if (command == "set_home") {
        double lat = 0, lon = 0, alt = 0;
        if (!json_number(args, "lat", lat, true) ||
            !json_number(args, "lon", lon, true) ||
            !json_number(args, "alt", alt, false)) result = invalid_args("invalid home target");
        else result = mg_command_set_home(core, lat, lon, static_cast<float>(alt));
    } else if (command == "guided_point") {
        double lat = 0, lon = 0, alt = 0, yaw = -1, speed = -1;
        if (!json_number(args, "lat", lat, true) ||
            !json_number(args, "lon", lon, true) ||
            !json_number(args, "alt", alt, true) ||
            !json_number(args, "yaw", yaw, false) ||
            !json_number(args, "speed", speed, false)) result = invalid_args("invalid guided target");
        else result = mg_command_guided_point(core, lat, lon,
            static_cast<float>(alt), static_cast<float>(yaw), static_cast<float>(speed));
    } else if (command == "orbit") {
        char* orbit_json = cJSON_PrintUnformatted(args);
        if (!orbit_json) result = invalid_args("invalid orbit args");
        else {
            result = mg_command_orbit(core, orbit_json);
            cJSON_free(orbit_json);
        }
    } else if (command == "set_roi") {
        double lat = 0, lon = 0, alt = 0;
        if (!json_number(args, "lat", lat, true) ||
            !json_number(args, "lon", lon, true) ||
            !json_number(args, "alt", alt, false)) result = invalid_args("invalid ROI target");
        else result = mg_command_set_roi(core, lat, lon, static_cast<float>(alt));
    } else if (command == "change_heading") {
        double heading = 0, yaw_rate = 0;
        int direction = 1;
        if (!json_number(args, "heading_deg", heading, true) ||
            !json_number(args, "yaw_rate_dps", yaw_rate, false) ||
            !json_int(args, "direction", direction, true)) result = invalid_args("invalid heading command");
        else result = mg_command_change_heading(core, static_cast<float>(heading),
            static_cast<float>(yaw_rate), direction);
    } else if (command == "set_global_origin") {
        double lat = 0, lon = 0, alt = 0;
        if (!json_number(args, "lat", lat, true) ||
            !json_number(args, "lon", lon, true) ||
            !json_number(args, "alt", alt, false)) result = invalid_args("invalid global origin");
        else result = mg_command_set_global_origin(core, lat, lon, static_cast<float>(alt));
    } else {
        result = invalid_args("unknown target command");
    }

    cJSON_Delete(root);
    if (owns_empty_args) cJSON_Delete(args);
    return result;
}

MGCORE_API char* mg_command_submit_for_vehicle(mg_core_t* core, int vehicle_id,
                                               const char* command_json) {
    CHECK_CORE(core);
    if (vehicle_id <= 0 || !command_json || !*command_json || std::strlen(command_json) > 16384)
        return invalid_args("invalid command submission");
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "target vehicle not found");
    const auto connection = vehicle->command_connection();
    const auto adapter = connection ? connection->shared_mavlink() : nullptr;
    if (!connection || !adapter || !connection->is_connected())
        return fail(MG_ERROR_NOT_CONNECTED, "target vehicle disconnected");
    const std::string json(command_json);

    // 并发判定：只拦"同一架飞机的同一条命令"重复下发。
    // QGC 的规则是 isPending(compId, command) 才算重复，不同命令本来就是并发的
    // （MavCommandQueue.cc:289-296）；旧实现按"该飞机已有任何命令在跑"一律拒绝，
    // 于是等引导到位时点返航会被硬拒。QGC 另有一张可重复白名单
    // （_canBeDuplicated：DO_MOTOR_TEST、SET_MESSAGE_INTERVAL），M-GCS 目前可提交的
    // 命令里没有对应项，故未保留空名单；将来加入时按同一位置补。
    std::string command_key;
    {
        cJSON* job_root = mg::core::json::parse(command_json);
        if (job_root && cJSON_IsObject(job_root)) {
            cJSON* item = cJSON_GetObjectItemCaseSensitive(job_root, "command");
            if (cJSON_IsString(item) && item->valuestring) command_key = item->valuestring;
        }
        if (job_root) cJSON_Delete(job_root);
        if (command_key.empty()) command_key = json;  // 名字缺失时按原始载荷判重
    }

    uint64_t ticket = 0;
    {
        mg::core::platform::LockGuard lk(core->command_jobs_mtx);
        if (core->command_jobs_stopping) return fail(MG_ERROR_INVALID_STATE, "core is stopping");
        if (core->command_jobs.size() >= 32) return fail(MG_ERROR_BUSY, "command job queue full");
        for (auto& [id, job] : core->command_jobs) {
            (void)id;
            if (job.vehicle_id == vehicle_id && job.command_key == command_key &&
                job.result.wait_for(std::chrono::milliseconds(0)) !=
                    std::future_status::ready) {
                return fail(MG_ERROR_BUSY, "waiting on previous response to same command");
            }
        }
        ticket = core->next_command_job++;
        try {
            core->command_jobs.emplace(ticket, mg_core_t::CommandJob{vehicle_id, command_key,
                std::async(std::launch::async, [core, vehicle, connection, adapter, json]() {
                    try {
                        mg::bridge::ffi::VehicleTargetScope scope(core, vehicle.get(), connection);
                        char* raw = mg_command_execute_for_vehicle(core, vehicle->vehicle_id(), json.c_str());
                        std::string result = raw ? raw : "{\"success\":false,\"error\":\"empty native response\"}";
                        mg_free_string(raw);
                        return result;
                    } catch (...) {
                        return std::string("{\"success\":false,\"error\":\"native command worker failed\"}");
                    }
                })});
        } catch (...) {
            return fail(MG_ERROR_INTERNAL, "unable to start command worker");
        }
    }
    mg::core::telemetry::JsonBuilder response;
    response.begin_object(); response.field("success", true);
    response.field("ticket", static_cast<int64_t>(ticket)); response.end_object();
    return to_cstr(response.str());
}

MGCORE_API char* mg_command_poll(mg_core_t* core, uint64_t ticket) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder response;
    response.begin_object(); response.field("success", true);
    response.field("ticket", static_cast<int64_t>(ticket));
    {
        mg::core::platform::LockGuard lk(core->command_jobs_mtx);
        const auto it = core->command_jobs.find(ticket);
        if (it == core->command_jobs.end()) return fail(MG_ERROR_NOT_FOUND, "command ticket not found");
        if (it->second.result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            response.field("state", "complete");
            response.raw_field("result", it->second.result.get());
            core->command_jobs.erase(it);
        } else {
            response.field("state", "pending");
        }
    }
    response.end_object(); return to_cstr(response.str());
}
