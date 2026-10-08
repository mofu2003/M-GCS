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
#include <cmath>
#include <limits>
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
    auto* result = cmd_result_queued(core, mav, MAV_CMD_DO_SET_HOME,
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

MGCORE_API char* mg_command_set_global_origin(mg_core_t* core, double lat,
                                              double lon, float alt_m) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    // 与 QGC `Vehicle::setEstimatorOrigin` 一致：先做经纬度合理性检查，
    // 越界直接本地拒绝，不发无效坐标给飞控。
    if (!(lat >= -90.0 && lat <= 90.0) || !(lon >= -180.0 && lon <= 180.0) ||
        (lat == 0.0 && lon == 0.0)) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid origin lat/lon");
    }
    const bool sent = get_active_mavlink(core)->cmd_set_global_origin(
        active_target_sys(core), active_target_comp(core),
        lat, lon, alt_m);
    get_active_flight_log(core).on_command_result("set_global_origin", sent, sent ? "sent" : "send_failed");
    if (!sent) return fail(MG_ERROR_SEND_FAILED, "set global origin send failed");
    return ok();
}

MGCORE_API char* mg_command_landing_gear(mg_core_t* core, bool deploy) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (get_active_mavlink(core)->cmd_landing_gear(active_target_sys(core), active_target_comp(core), deploy))
        return ok();
    return fail(MG_ERROR_SEND_FAILED, "landing gear command send failed");
}

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

    cJSON* root = cJSON_Parse(command_json);
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
    } else if (command == "mission_start") {
        result = mg_mission_start(core);
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
