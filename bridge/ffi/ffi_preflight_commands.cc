#define MGCORE_EXPORTS
/**
 * Preflight and failsafe inspection/configuration FFI entrypoints.
 */
#include "ffi_internal.h"
#include "json_builder.h"

namespace {

bool append_preflight_check(mg::bridge::JsonBuilder& checks,
                            const char* name,
                            bool ok_value,
                            const char* message) {
    checks.begin_object();
    checks.field("name", name);
    checks.field("ok", ok_value);
    checks.field("message", message);
    checks.end_object();
    return ok_value;
}

}  // namespace

MGCORE_API char* mg_battery_failsafe_configure(mg_core_t* core,
                                                 int low_voltage_action,
                                                 float low_voltage_threshold,
                                                 float critical_voltage_threshold) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");

    bool ok1 = mav->set_param(active_target_sys(core), active_target_comp(core),
                               "BATT_LOW_VOLT", low_voltage_threshold, 9);
    bool ok2 = mav->set_param(active_target_sys(core), active_target_comp(core),
                               "BATT_CRT_VOLT", critical_voltage_threshold, 9);

    if (ok1 && ok2) {
        mg::bridge::JsonBuilder jb;
        jb.begin_object();
        jb.field("success", true);
        jb.field("low_voltage_threshold", static_cast<double>(low_voltage_threshold));
        jb.field("critical_voltage_threshold", static_cast<double>(critical_voltage_threshold));
        jb.field("low_voltage_action", low_voltage_action);
        jb.end_object();
        return to_cstr(jb.str());
    }
    return fail(MG_ERROR_SEND_FAILED, "battery failsafe param set failed");
}

MGCORE_API char* mg_failsafe_get_status(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");

    mg::bridge::JsonBuilder jb;
    jb.begin_object();

    auto bat = mav->latest_battery();
    bool battery_ok = !bat || bat->battery_remaining < 0 || bat->battery_remaining > 20;
    jb.field("battery_ok", battery_ok);
    jb.field("battery_voltage", bat ? static_cast<double>(bat->voltage_v) : 0.0);
    jb.field("battery_remaining", bat ? static_cast<int>(bat->battery_remaining) : -1);

    auto gps = mav->latest_gps();
    bool gps_ok = gps && gps->fix_type >= 3;
    jb.field("gps_ok", gps_ok);
    jb.field("gps_fix", gps ? static_cast<int>(gps->fix_type) : 0);
    jb.field("gps_satellites", gps ? static_cast<int>(gps->satellites_visible) : 0);

    auto& geofence = get_active_geofence(core);
    auto fence_cfg = geofence.get_config();
    jb.field("fence_enabled", fence_cfg.enabled);
    jb.field("all_ok", battery_ok && gps_ok);

    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_preflight_check(mg_core_t* core) {
    CHECK_CORE(core);

    auto* pfcache = get_active_cache(core);
    if (!pfcache) return fail(MG_ERROR_NOT_CONNECTED, "no cache");

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    bool all_ok = true;

    mg::bridge::JsonBuilder checks_arr;
    checks_arr.begin_array();

    bool connected = is_active_connected(core);
    all_ok &= append_preflight_check(
        checks_arr, "connection", connected, connected ? "connected" : "not connected");

    bool hb_alive = get_active_conn(core).heartbeat_alive(5000);
    all_ok &= append_preflight_check(
        checks_arr, "heartbeat", hb_alive, hb_alive ? "alive" : "timeout");

    auto gps = pfcache->snapshot_gps().data;
    bool gps_ok = gps.fix_type >= 3;
    checks_arr.begin_object();
    checks_arr.field("name", "gps");
    checks_arr.field("ok", gps_ok);
    checks_arr.field("fix_type", static_cast<int>(gps.fix_type));
    checks_arr.field("satellites", static_cast<int>(gps.satellites_visible));
    checks_arr.field("message", gps_ok ? "3D fix" : "no 3D fix");
    checks_arr.end_object();
    all_ok &= gps_ok;

    auto sys = pfcache->snapshot_sys_status().data;
    bool batt_ok = sys.battery_remaining < 0 || sys.battery_remaining > 20;
    checks_arr.begin_object();
    checks_arr.field("name", "battery");
    checks_arr.field("ok", batt_ok);
    checks_arr.field("remaining", static_cast<int>(sys.battery_remaining));
    checks_arr.field("voltage", static_cast<int>(sys.battery_voltage));
    checks_arr.field("message", batt_ok ? "ok" : "low battery");
    checks_arr.end_object();
    all_ok &= batt_ok;

    bool sensors_ok = (sys.onboard_control_sensors_health & sys.onboard_control_sensors_enabled)
                       == sys.onboard_control_sensors_enabled;
    all_ok &= append_preflight_check(
        checks_arr, "sensors", sensors_ok, sensors_ok ? "all healthy" : "sensor fault");

    auto rc_snap = pfcache->snapshot_rc();
    bool rc_ok = (rc_snap.data.chan[0] > 800 && rc_snap.data.chan[0] < 2200);
    all_ok &= append_preflight_check(
        checks_arr, "rc", rc_ok, rc_ok ? "rc ok" : "no valid RC signal");

    bool armed_ok = !get_active_flight_modes(core).is_armed();
    all_ok &= append_preflight_check(
        checks_arr, "disarmed", armed_ok, armed_ok ? "disarmed" : "already armed");

    checks_arr.end_array();
    jb.raw_field("checks", checks_arr.str());
    jb.field("ready", all_ok);
    jb.field("ready_message", all_ok ? "all checks passed" : "some checks failed");
    jb.end_object();
    return to_cstr(jb.str());
}
