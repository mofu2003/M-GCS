#define MGCORE_EXPORTS
/**
 * ffi_flight_commands.cc - Flight command implementations
 *
 * 飞行指令：解锁/上锁、起降、模式切换、RTL、引导模式、紧急停止。
 *
 * arm, disarm, takeoff, land, set_mode, RTL, emergency_stop, guided_*
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include "telemetry_bindings.h"

#include <algorithm>
#include <cstring>

// Cross-platform case-insensitive string compare
#ifdef _WIN32
  #define mg_stricmp _stricmp
#else
  #include <strings.h>
  #define mg_stricmp strcasecmp
#endif

// cJSON
#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

/// 命令队列辅助函数：发送命令并阻塞等待 ACK
/// 使用 send_command_queued() + wait_for_command()，支持超时重发
static char* cmd_result_queued(mg_core_t* core, mg::core::mavlink::MavlinkAdapter* mav,
                               uint16_t cmd_id, uint8_t sys, uint8_t comp,
                               float p1, float p2, float p3, float p4,
                               float p5, float p6, float p7, int max_wait_ms = 5000) {
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    auto ticket = mav->send_command_queued(sys, comp, cmd_id,
        p1, p2, p3, p4, p5, p6, p7);
    auto ack = mav->wait_for_command(ticket, max_wait_ms);
    if (ack.has_value()) {
        if (ack->result_text.find("timeout") != std::string::npos) {
            return fail(MG_ERROR_TIMEOUT, ack->result_text.c_str());
        }
        if (ack->result == 0) return ok();  // MAV_RESULT_ACCEPTED
        mg_set_last_error(MG_ERROR_SEND_FAILED);
        return to_cstr(mg::bridge::command_ack_json(ack.value()));
    }
    return fail(MG_ERROR_TIMEOUT, "command ack timeout");
}

MGCORE_API char* mg_command_arm(mg_core_t* core, int force) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    float force_param = force ? 21196.0f : 0.0f;
    auto* result = cmd_result_queued(core, mav, MAV_CMD_COMPONENT_ARM_DISARM,
        active_target_sys(core), active_target_comp(core),
        1.0f, force_param, 0, 0, 0, 0, 0);
    get_active_flight_log(core).on_command_result("arm", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_disarm(mg_core_t* core, int force) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    float force_param = force ? 21196.0f : 0.0f;
    auto* result = cmd_result_queued(core, mav, MAV_CMD_COMPONENT_ARM_DISARM,
        active_target_sys(core), active_target_comp(core),
        0.0f, force_param, 0, 0, 0, 0, 0);
    get_active_flight_log(core).on_command_result("disarm", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return cmd_result_queued(core, mav, MAV_CMD_DO_SET_MODE,
        active_target_sys(core), active_target_comp(core),
        static_cast<float>(base_mode), static_cast<float>(custom_mode),
        0, 0, 0, 0, 0);
}

MGCORE_API char* mg_command_rtl(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    auto* result = cmd_result_queued(core, mav, MAV_CMD_NAV_RETURN_TO_LAUNCH,
        active_target_sys(core), active_target_comp(core),
        0, 0, 0, 0, 0, 0, 0);
    get_active_flight_log(core).on_command_result("rtl", result && strstr(result, "\"success\":true"), "queued");
    return result;
}

MGCORE_API char* mg_command_takeoff(mg_core_t* core, float altitude_m) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return cmd_result_queued(core, mav, MAV_CMD_NAV_TAKEOFF,
        active_target_sys(core), active_target_comp(core),
        0, 0, 0, 0, 0, 0, altitude_m);
}

MGCORE_API char* mg_command_land(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return cmd_result_queued(core, mav, MAV_CMD_NAV_LAND,
        active_target_sys(core), active_target_comp(core),
        0, 0, 0, 0, 0, 0, 0);
}

MGCORE_API char* mg_command_emergency_stop(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return cmd_result_queued(core, mav, MAV_CMD_DO_FLIGHTTERMINATION,
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
    // 这里走的是命令队列（COMMAND_LONG），MAV_CMD_DO_REPOSITION 在 COMMAND_LONG
    // 下的 param5/param6 是「度」，不是度×1e7。飞控收到后会自己做 ×1e7 转换，
    // 传 lat_e7 会让坐标溢出成垃圾值。需要 1e7 精度的路径见
    // MavlinkAdapter::cmd_guided_point（COMMAND_INT）。
    float p1 = speed > 0 ? speed : -1.0f;
    float p4 = (yaw >= 0) ? yaw : NAN;
    auto* result = cmd_result_queued(core, mav, 192,  // MAV_CMD_DO_REPOSITION
        active_target_sys(core), active_target_comp(core),
        p1, 1.0f, 0, p4,
        static_cast<float>(lat), static_cast<float>(lon), alt);
    get_active_flight_log(core).on_command_result("guided_point", result && strstr(result, "\"success\":true"), "queued");
    return result;
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

MGCORE_API char* mg_command_guided_pause(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    bool sent = mav->cmd_guided_pause(active_target_sys(core), active_target_comp(core));
    get_active_flight_log(core).on_command_result("guided_pause", sent, sent ? "sent" : "send_failed");
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "send failed");
}

// ================================================================
// ffi_api.h aliases (simplified signatures)
// ================================================================

MGCORE_API char* mg_cmd_arm(mg_core_t* core) {
    return mg_command_arm(core, 0);
}

MGCORE_API char* mg_cmd_disarm(mg_core_t* core) {
    return mg_command_disarm(core, 0);
}

MGCORE_API char* mg_cmd_rtl(mg_core_t* core) {
    return mg_command_rtl(core);
}

MGCORE_API char* mg_cmd_takeoff(mg_core_t* core, float altitude) {
    return mg_command_takeoff(core, altitude);
}

MGCORE_API char* mg_cmd_land(mg_core_t* core) {
    return mg_command_land(core);
}

MGCORE_API char* mg_cmd_loiter(mg_core_t* core) {
    return mg_command_guided_pause(core);
}

MGCORE_API char* mg_cmd_guided(mg_core_t* core, double lat, double lon, float alt) {
    return mg_command_guided_point(core, lat, lon, alt, 0.0f, -1.0f);
}

MGCORE_API char* mg_cmd_set_mode(mg_core_t* core, const char* mode) {
    if (!mode || mode[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null mode");
    static const struct { const char* name; int custom_mode; } ap_copter_modes[] = {
        {"Stabilize", 0}, {"Acro", 1}, {"AltHold", 2}, {"Auto", 3},
        {"Guided", 4}, {"Loiter", 5}, {"RTL", 6}, {"Circle", 7},
        {"Land", 9}, {"Drift", 11}, {"Sport", 13}, {"Flip", 14},
        {"AutoTune", 15}, {"PosHold", 16}, {"Brake", 17}, {"Throw", 18},
        {"Avoid_ADSB", 19}, {"Guided_NoGPS", 20}, {"Smart_RTL", 21},
        {"FlowHold", 22}, {"Follow", 23}, {"Zigzag", 24}, {"SystemID", 25},
        {"Heli_Autorotate", 26}, {"Auto RTL", 27}, {"Turtle", 28},
        {nullptr, -1}
    };
    for (int i = 0; ap_copter_modes[i].name; i++) {
        if (mg_stricmp(mode, ap_copter_modes[i].name) == 0) {
            return mg_command_set_mode(core, 0, ap_copter_modes[i].custom_mode);
        }
    }
    char* endptr = nullptr;
    long val = strtol(mode, &endptr, 10);
    if (endptr != mode && *endptr == '\0' && val >= 0 && val <= 255) {
        return mg_command_set_mode(core, 0, static_cast<int>(val));
    }
    return fail(MG_ERROR_INVALID_PARAM, "unknown mode");
}

// ================================================================
// Extended flight commands
// ================================================================

MGCORE_API char* mg_command_orbit(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!json_config) return fail(MG_ERROR_INVALID_PARAM, "null config");
    cJSON* root = cJSON_Parse(json_config);
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

MGCORE_API char* mg_command_landing_gear(mg_core_t* core, bool deploy) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (get_active_mavlink(core)->cmd_landing_gear(active_target_sys(core), active_target_comp(core), deploy))
        return ok();
    return fail(MG_ERROR_SEND_FAILED, "landing gear command send failed");
}
