/**
 * test_v2_contract.cc - Contract tests for Bridge API v2 (no hardware)
 *
 * Validates:
 *   - Envelope shape (success/error) for all 19 v2 functions
 *   - Normalized fields presence
 *   - Error path when core is null
 */
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int tests_run = 0;
static int tests_passed = 0;

static bool contains(const char* json, const char* needle) {
    return json && strstr(json, needle) != nullptr;
}

static void check(bool cond, const char* name) {
    tests_run++;
    if (cond) {
        tests_passed++;
        fprintf(stderr, "  [PASS] %s\n", name);
    } else {
        fprintf(stderr, "  [FAIL] %s\n", name);
    }
}

// Helper: test null-core returns error envelope
static void test_null(const char* label, char* r) {
    char buf[128];
    snprintf(buf, sizeof(buf), "%s null -> success=false", label);
    check(contains(r, "\"success\":false"), buf);
    mg_free_string(r);
}

int main() {
    fprintf(stderr, "=== Bridge API v2 Contract Tests ===\n\n");

    // ---- Null core tests (error envelope) ----
    {
        fprintf(stderr, "[Group] Null core error handling\n");
        test_null("connection_get_state",     mg_connection_get_state(nullptr));
        test_null("vehicle_get_summary",      mg_vehicle_get_summary(nullptr));
        test_null("vehicle_get_battery",      mg_vehicle_get_battery(nullptr));
        test_null("vehicle_get_global_pos",   mg_vehicle_get_global_position(nullptr));
        test_null("vehicle_get_attitude",     mg_vehicle_get_attitude(nullptr));
        test_null("vehicle_get_gps",          mg_vehicle_get_gps(nullptr));
        test_null("vehicle_get_rc_channels",  mg_vehicle_get_rc_channels(nullptr));
        test_null("vehicle_get_servo_output", mg_vehicle_get_servo_output(nullptr));
        test_null("vehicle_get_home",         mg_vehicle_get_home_position(nullptr));
        test_null("vehicle_get_vfr_hud",      mg_vehicle_get_vfr_hud(nullptr));
        test_null("vehicle_get_imu",          mg_vehicle_get_imu(nullptr));
        test_null("vehicle_get_pressure",     mg_vehicle_get_pressure(nullptr));
        test_null("vehicle_get_highres_imu",  mg_vehicle_get_highres_imu(nullptr));
        test_null("vehicle_get_ext_sys",      mg_vehicle_get_extended_sys_state(nullptr));
        test_null("vehicle_get_firmware_ver", mg_vehicle_get_firmware_version(nullptr));
        test_null("vehicle_get_vibration",    mg_vehicle_get_vibration(nullptr));
        test_null("vehicle_get_esc_status",   mg_vehicle_get_esc_status(nullptr));
        test_null("vehicle_get_wind",         mg_vehicle_get_wind_estimate(nullptr));
        test_null("vehicle_get_prearm",       mg_vehicle_get_prearm_check(nullptr));
    }

    // ---- Disconnected state tests ----
    {
        fprintf(stderr, "\n[Group] Disconnected state (valid core, no connection)\n");
        auto* core = mg_create();
        check(core != nullptr, "mg_create returns non-null");

        auto test_ok = [&](const char* label, char* r, const char* field) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s envelope ok", label);
            check(contains(r, "\"success\":true"), buf);
            snprintf(buf, sizeof(buf), "%s has %s", label, field);
            check(contains(r, field), buf);
            mg_free_string(r);
        };

        // 1. Connection state
        char* cs = mg_connection_get_state(core);
        check(contains(cs, "\"success\":true"), "connection_get_state envelope ok");
        check(contains(cs, "\"connected\":false"), "connected=false when idle");
        check(contains(cs, "\"state\":"), "has state field");
        check(contains(cs, "\"heartbeat_alive\":"), "has heartbeat_alive");
        check(contains(cs, "\"heartbeat_age_ms\":"), "has heartbeat_age_ms");
        mg_free_string(cs);

        // 2. Vehicle summary
        test_ok("vehicle_summary", mg_vehicle_get_summary(core), "\"flight_mode\":");

        // 3. Battery
        test_ok("battery", mg_vehicle_get_battery(core), "\"voltage_v\":");

        // 4. Global position
        test_ok("global_pos", mg_vehicle_get_global_position(core), "\"lat_deg\":");

        // 5. Attitude
        test_ok("attitude", mg_vehicle_get_attitude(core), "\"roll_deg\":");

        // 6. GPS
        test_ok("gps", mg_vehicle_get_gps(core), "\"fix_type\":");

        // 7. RC Channels
        test_ok("rc_channels", mg_vehicle_get_rc_channels(core), "\"channel_count\":");

        // 8. Servo Output
        test_ok("servo_output", mg_vehicle_get_servo_output(core), "\"servos\":");

        // 9. Home Position
        test_ok("home_pos", mg_vehicle_get_home_position(core), "\"lat_deg\":");

        // 10. VFR HUD
        test_ok("vfr_hud", mg_vehicle_get_vfr_hud(core), "\"airspeed_m_s\":");

        // 11. IMU
        test_ok("imu", mg_vehicle_get_imu(core), "\"acc_mg\":");

        // 12. Pressure
        test_ok("pressure", mg_vehicle_get_pressure(core), "\"abs_pressure_hpa\":");

        // 13. HIGHRES IMU
        test_ok("highres_imu", mg_vehicle_get_highres_imu(core), "\"fields_updated\":");

        // 14. Extended sys state
        test_ok("ext_sys", mg_vehicle_get_extended_sys_state(core), "\"landed_state\":");

        // 15. Firmware version
        test_ok("firmware_ver", mg_vehicle_get_firmware_version(core), "\"received\":");

        // 16. Vibration
        test_ok("vibration", mg_vehicle_get_vibration(core), "\"vibration_x\":");

        // 17. ESC Status
        test_ok("esc_status", mg_vehicle_get_esc_status(core), "\"esc\":");

        // 18. Wind Estimate
        test_ok("wind_estimate", mg_vehicle_get_wind_estimate(core), "\"wind_speed_m_s\":");

        // 19. Prearm Check
        test_ok("prearm_check", mg_vehicle_get_prearm_check(core), "\"ready\":");

        mg_destroy(core);
    }

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

