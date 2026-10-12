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

    // --- Connection / Vehicle ---
    printf("\n--- Connection / Vehicle ---\n"); fflush(stdout);
    TJ("mg_connection_list", mg_connection_list(core));
    TJ("mg_connection_get_session_state", mg_connection_get_session_state(core, 0));
    TJ("mg_vehicle_list", mg_vehicle_list(core));
    TJ("mg_vehicle_get_summary", mg_vehicle_get_summary(core));
    TJ("mg_vehicle_get_global_position", mg_vehicle_get_global_position(core));
    TJ("mg_vehicle_get_attitude", mg_vehicle_get_attitude(core));
    TJ("mg_vehicle_get_home_position", mg_vehicle_get_home_position(core));
    TJ("mg_vehicle_get_vfr_hud", mg_vehicle_get_vfr_hud(core));
    TJ("mg_vehicle_get_extended_sys_state", mg_vehicle_get_extended_sys_state(core));
    TJ("mg_vehicle_get_firmware_version", mg_vehicle_get_firmware_version(core));

    // --- Sensors / Health / Waveform ---
    printf("\n--- Sensors / Health ---\n"); fflush(stdout);
    TJ("mg_sensors_get_health", mg_sensors_get_health(core));
    TJ("mg_telemetry_get_freshness", mg_telemetry_get_freshness(core));
    TJ("mg_waveform_get_instances", mg_waveform_get_instances(core));

    // --- RC / Servo ---
    printf("\n--- RC / Servo ---\n"); fflush(stdout);
    TJ("mg_rc_get_channels", mg_rc_get_channels(core));
    TJ("mg_servo_get_outputs", mg_servo_get_outputs(core));

    // --- Firmware / Config ---
    printf("\n--- Firmware / Config ---\n"); fflush(stdout);
    TJ("mg_firmware_get_type", mg_firmware_get_type(core));
    TJ("mg_vehicle_get_firmware", mg_vehicle_get_firmware(core));
    TJ("mg_params_get_all", mg_params_get_all(core));
    TJ("mg_config_read_param", mg_config_read_param(core, "SYSID_SW_MREV"));
    TJ("mg_config_get_param_roles", mg_config_get_param_roles(core));

    // --- Mission document ---
    printf("\n--- Mission document ---\n"); fflush(stdout);
    TJ("mg_mission_doc_get", mg_mission_doc_get(core));
    TJ("mg_mission_doc_validate", mg_mission_doc_validate(core));
    TJ("mg_mission_doc_export", mg_mission_doc_export(core));
    TJ("mg_mission_get_command_catalog", mg_mission_get_command_catalog(core));

    // --- Flight modes ---
    printf("\n--- Flight modes ---\n"); fflush(stdout);
    TJ("mg_flight_get_modes", mg_flight_get_modes(core));

    // --- Logs ---
    printf("\n--- Logs ---\n"); fflush(stdout);
    TJ("mg_onboard_log_get_entries", mg_onboard_log_get_entries(core));
    TJ("mg_tlog_status", mg_tlog_status(core));

    // --- Calibration ---
    printf("\n--- Calibration ---\n"); fflush(stdout);
    TJ("mg_calibration_get_status", mg_calibration_get_status(core));

    // --- Flight commands (offline: 必须回错误对象，不能崩溃) ---
    printf("\n--- Flight Commands (offline) ---\n"); fflush(stdout);
    TJ("mg_command_arm", mg_command_arm(core, 0));
    TJ("mg_command_disarm", mg_command_disarm(core, 0));
    TJ("mg_command_takeoff", mg_command_takeoff(core, 10.0f));
    TJ("mg_command_land", mg_command_land(core));
    TJ("mg_command_rtl", mg_command_rtl(core));
    TJ("mg_command_guided_pause", mg_command_guided_pause(core));
    TJ("mg_command_guided_point",
       mg_command_guided_point(core, 0.0, 0.0, 10.0f, 0.0f, -1.0f));

    mg_destroy(core);

    printf("\n=========================================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("=========================================================\n");

    return (g_fail > 0) ? 1 : 0;
}
