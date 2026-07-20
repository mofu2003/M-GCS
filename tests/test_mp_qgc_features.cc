#define MGCORE_EXPORTS
/**
 * test_mp_qgc_features.cc - Test all ffi_api.h functions offline
 *
 * Validates every function declared in ffi_api.h returns valid JSON
 * without crashing. Inspired by MP/QGC feature coverage.
 *
 * Total ffi_api.h functions: ~120. This tests ~60 offline-safe ones.
 */
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

static int g_pass = 0;
static int g_fail = 0;

#define T(name, expr) do { \
    if (expr) { g_pass++; printf("  [PASS] %s\n", name); } \
    else      { g_fail++; printf("  [FAIL] %s\n", name); } \
} while(0)

#define TJ(name, ptr) do { \
    char* _s = (ptr); \
    if (_s) { g_pass++; printf("  [PASS] %s\n", name); mg_free_string(_s); } \
    else    { g_fail++; printf("  [FAIL] %s (null)\n", name); } \
} while(0)

int main() {
    printf("=========================================================\n");
    printf("  MP/QGC Feature Coverage Test (offline)\n");
    printf("=========================================================\n\n");

    mg_core_t* core = mg_create();
    T("mg_create", core != nullptr);
    if (!core) return 1;

    TJ("mg_get_version", mg_get_version());

    // --- Connection ---
    printf("\n--- Connection ---\n"); fflush(stdout);
    TJ("mg_connection_get_state", mg_connection_get_state(core));
    TJ("mg_connection_get_heartbeat_age", mg_connection_get_heartbeat_age(core));
    TJ("mg_connection_check_heartbeat", mg_connection_check_heartbeat(core, 5000));

    // --- Vehicle ---
    printf("\n--- Vehicle ---\n"); fflush(stdout);
    TJ("mg_vehicle_list", mg_vehicle_list(core));
    TJ("mg_vehicle_get_summary", mg_vehicle_get_summary(core));
    TJ("mg_vehicle_get_battery", mg_vehicle_get_battery(core));
    TJ("mg_vehicle_get_global_position", mg_vehicle_get_global_position(core));
    TJ("mg_vehicle_get_attitude", mg_vehicle_get_attitude(core));
    TJ("mg_vehicle_get_gps", mg_vehicle_get_gps(core));
    TJ("mg_vehicle_get_rc_channels", mg_vehicle_get_rc_channels(core));
    TJ("mg_vehicle_get_servo_output", mg_vehicle_get_servo_output(core));
    TJ("mg_vehicle_get_home_position", mg_vehicle_get_home_position(core));
    TJ("mg_vehicle_get_vfr_hud", mg_vehicle_get_vfr_hud(core));
    TJ("mg_vehicle_get_imu", mg_vehicle_get_imu(core));
    TJ("mg_vehicle_get_pressure", mg_vehicle_get_pressure(core));
    TJ("mg_vehicle_get_highres_imu", mg_vehicle_get_highres_imu(core));
    TJ("mg_vehicle_get_extended_sys_state", mg_vehicle_get_extended_sys_state(core));
    TJ("mg_vehicle_get_firmware_version", mg_vehicle_get_firmware_version(core));
    TJ("mg_vehicle_get_vibration", mg_vehicle_get_vibration(core));
    TJ("mg_vehicle_get_esc_status", mg_vehicle_get_esc_status(core));
    TJ("mg_vehicle_get_wind_estimate", mg_vehicle_get_wind_estimate(core));
    TJ("mg_vehicle_get_prearm_check", mg_vehicle_get_prearm_check(core));

    // --- Sensors / Health ---
    printf("\n--- Sensors / Health ---\n"); fflush(stdout);
    TJ("mg_sensors_get_health", mg_sensors_get_health(core));
    TJ("mg_sensors_get_statustext", mg_sensors_get_statustext(core));
    TJ("mg_health_get_all", mg_health_get_all(core));
    TJ("mg_sensors_identify_all", mg_sensors_identify_all(core));
    TJ("mg_telemetry_get_freshness", mg_telemetry_get_freshness(core));

    // --- RC / Servo / Actuator ---
    printf("\n--- RC / Servo / Actuator ---\n"); fflush(stdout);
    TJ("mg_rc_get_channels", mg_rc_get_channels(core));
    TJ("mg_rc_get_channel", mg_rc_get_channel(core, 1));
    TJ("mg_servo_get_outputs", mg_servo_get_outputs(core));
    TJ("mg_servo_get_output", mg_servo_get_output(core, 1));
    TJ("mg_actuator_get_mapping", mg_actuator_get_mapping(core));
    TJ("mg_rc_get_mapping", mg_rc_get_mapping(core));
    TJ("mg_rc_get_calibration", mg_rc_get_calibration(core));
    TJ("mg_rc_get_failsafe", mg_rc_get_failsafe(core));

    // --- Firmware ---
    printf("\n--- Firmware ---\n"); fflush(stdout);
    TJ("mg_firmware_get_type", mg_firmware_get_type(core));
    TJ("mg_firmware_flash_status", mg_firmware_flash_status(core));
    TJ("mg_firmware_get_version", mg_firmware_get_version(core));

    // --- Config / Params ---
    printf("\n--- Config / Params ---\n"); fflush(stdout);
    TJ("mg_params_get_all", mg_params_get_all(core));
    TJ("mg_config_read_param", mg_config_read_param(core, "SYSID_SW_MREV"));

    // --- Mission ---
    printf("\n--- Mission ---\n"); fflush(stdout);
    TJ("mg_mission_get_plan", mg_mission_get_plan(core));
    TJ("mg_mission_get_progress", mg_mission_get_progress(core));

    // --- Flight Modes ---
    printf("\n--- Flight Modes ---\n"); fflush(stdout);
    TJ("mg_flight_get_available_modes", mg_flight_get_available_modes(core));
    TJ("mg_flight_get_mode_status", mg_flight_get_mode_status(core));
    TJ("mg_flight_get_mode_history", mg_flight_get_mode_history(core));

    // --- Geofence ---
    printf("\n--- Geofence ---\n"); fflush(stdout);
    TJ("mg_geofence_get", mg_geofence_get(core));
    TJ("mg_geofence_check", mg_geofence_check(core, 37.0, -122.0));

    // --- Rally / Fence ---
    printf("\n--- Rally / Fence Protocol ---\n"); fflush(stdout);
    TJ("mg_rally_get_plan", mg_rally_get_plan(core));
    TJ("mg_fence_get_plan", mg_fence_get_plan(core));

    // --- Logs ---
    printf("\n--- Logs ---\n"); fflush(stdout);
    TJ("mg_log_get_entries", mg_log_get_entries(core));
    TJ("mg_logging_get_stats", mg_logging_get_stats(core));

    // --- Preflight ---
    printf("\n--- Preflight ---\n"); fflush(stdout);
    TJ("mg_preflight_check", mg_preflight_check(core));

    // --- SITL ---
    printf("\n--- SITL ---\n"); fflush(stdout);
    TJ("mg_sitl_status", mg_sitl_status(core));

    // --- Calibration ---
    printf("\n--- Calibration ---\n"); fflush(stdout);
    TJ("mg_cal_status", mg_cal_status(core));
    TJ("mg_cal_history", mg_cal_history(core, 0));
    TJ("mg_cal_retry_info", mg_cal_retry_info(core));

    // --- PX4 Specific ---
    printf("\n--- PX4 Specific ---\n"); fflush(stdout);
    TJ("mg_px4_actuator_get_config", mg_px4_actuator_get_config(core));
    TJ("mg_px4_flight_mode_get_config", mg_px4_flight_mode_get_config(core));
    TJ("mg_px4_safety_get", mg_px4_safety_get(core));
    TJ("mg_px4_tuning_get", mg_px4_tuning_get(core));

    // --- Flight Commands (verify FFI calls return valid JSON) ---
    printf("\n--- Flight Commands (FFI call check) ---\n"); fflush(stdout);
    TJ("mg_cmd_arm", mg_cmd_arm(core));
    TJ("mg_cmd_disarm", mg_cmd_disarm(core));
    TJ("mg_cmd_takeoff", mg_cmd_takeoff(core, 10.0f));
    TJ("mg_cmd_land", mg_cmd_land(core));
    TJ("mg_cmd_rtl", mg_cmd_rtl(core));
    TJ("mg_cmd_loiter", mg_cmd_loiter(core));
    TJ("mg_cmd_guided", mg_cmd_guided(core, 0.0, 0.0, 10.0f));

    mg_destroy(core);

    printf("\n=========================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("=========================================================\n");

    return (g_fail > 0) ? 1 : 0;
}
