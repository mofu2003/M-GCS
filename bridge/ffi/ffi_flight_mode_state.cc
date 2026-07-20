#define MGCORE_EXPORTS
/**
 * Flight mode state and tuning FFI entrypoints.
 */
#include "ffi_internal.h"

#include <optional>

namespace {

void refresh_flight_mode_from_latest_heartbeat(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    auto hb_info = mav ? mav->latest_heartbeat() : std::nullopt;
    if (!hb_info) return;

    get_active_flight_modes(core).update_from_heartbeat(
        hb_info->autopilot_type,
        hb_info->vehicle_type,
        hb_info->base_mode,
        hb_info->custom_mode,
        hb_info->armed,
        hb_info->system_status);
}

}  // namespace

MGCORE_API char* mg_flight_get_available_modes(mg_core_t* core) {
    CHECK_CORE(core);

    refresh_flight_mode_from_latest_heartbeat(core);

    auto modes = get_active_flight_modes(core).available_modes();

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& mode : modes) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", mode);
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_get_mode_status(mg_core_t* core) {
    CHECK_CORE(core);

    refresh_flight_mode_from_latest_heartbeat(core);

    auto& mgr = get_active_flight_modes(core);
    auto status = mgr.get_status();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("current_mode", status.current_mode);
    jb.field("armed", status.armed);
    jb.field("in_flight", status.in_flight);
    jb.field("base_mode", static_cast<int>(status.base_mode));
    jb.field("custom_mode", static_cast<int>(status.custom_mode));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_get_mode_history(mg_core_t* core) {
    CHECK_CORE(core);

    auto& mgr = get_active_flight_modes(core);
    auto history = mgr.get_history();

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& entry : history) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("old_mode", entry.old_mode);
        item.field("new_mode", entry.new_mode);
        item.field("result", static_cast<int>(entry.result));
        if (!entry.error.empty()) item.field("error", entry.error);
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_get_takeoff_altitude(mg_core_t* core) {
    CHECK_CORE(core);

    auto& mgr = get_active_flight_modes(core);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("altitude_m", mgr.takeoff_altitude());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_set_takeoff_altitude(mg_core_t* core, float altitude_m) {
    CHECK_CORE(core);
    if (altitude_m < 1.0f || altitude_m > 200.0f) {
        return fail(MG_ERROR_INVALID_PARAM, "altitude must be 1-200m");
    }

    auto& mgr = get_active_flight_modes(core);
    mgr.set_takeoff_altitude(altitude_m);
    return ok();
}

MGCORE_API char* mg_flight_get_loiter_radius(mg_core_t* core) {
    CHECK_CORE(core);

    auto& mgr = get_active_flight_modes(core);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("radius_m", mgr.loiter_radius());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_set_loiter_radius(mg_core_t* core, float radius_m) {
    CHECK_CORE(core);
    if (radius_m < 5.0f || radius_m > 500.0f) {
        return fail(MG_ERROR_INVALID_PARAM, "radius must be 5-500m");
    }

    auto& mgr = get_active_flight_modes(core);
    mgr.set_loiter_radius(radius_m);
    return ok();
}
