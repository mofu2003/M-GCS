/**
 * test_ffi_response_contract.cc - Telemetry response contract tests (no hardware)
 *
 * Validates:
 *   - Envelope shape (success/error) for telemetry snapshot functions
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

// F4: every failing response uses one error object, never a bare string.
static void test_failure_envelope_shape() {
    fprintf(stderr, "[Group] Failure envelope shape (F4)\n");
    const struct { const char* label; char* json; } cases[] = {
        {"firmware_get_type", mg_firmware_get_type(nullptr)},
        {"command_arm", mg_command_arm(nullptr, 0)},
        {"params_get_by_name", mg_params_get_by_name(nullptr, "X")},
        {"mission_doc_get", mg_mission_doc_get(nullptr)},
    };
    for (const auto& c : cases) {
        char buf[160];
        snprintf(buf, sizeof(buf), "%s: error is an object", c.label);
        check(contains(c.json, "\"error\":{"), buf);
        snprintf(buf, sizeof(buf), "%s: error has numeric code", c.label);
        check(contains(c.json, "\"code\":"), buf);
        snprintf(buf, sizeof(buf), "%s: error has symbolic name", c.label);
        check(contains(c.json, "\"name\":\""), buf);
        snprintf(buf, sizeof(buf), "%s: error has message", c.label);
        check(contains(c.json, "\"message\":\""), buf);
        snprintf(buf, sizeof(buf), "%s: no legacy flat error string", c.label);
        check(!contains(c.json, "\"error\":\""), buf);
        mg_free_string(c.json);
    }
}

int main() {
    fprintf(stderr, "=== FFI Telemetry Response Contract Tests ===\n\n");

    test_failure_envelope_shape();

    // ---- Null core tests (error envelope) ----
    {
        fprintf(stderr, "[Group] Null core error handling\n");
        test_null("vehicle_get_summary",      mg_vehicle_get_summary(nullptr));
        test_null("vehicle_get_global_pos",   mg_vehicle_get_global_position(nullptr));
        test_null("vehicle_get_attitude",     mg_vehicle_get_attitude(nullptr));
        test_null("rc_get_channels",          mg_rc_get_channels(nullptr));
        test_null("servo_get_outputs",        mg_servo_get_outputs(nullptr));
        test_null("vehicle_get_home",         mg_vehicle_get_home_position(nullptr));
        test_null("vehicle_get_vfr_hud",      mg_vehicle_get_vfr_hud(nullptr));
        test_null("vehicle_get_ext_sys",      mg_vehicle_get_extended_sys_state(nullptr));
        test_null("vehicle_get_firmware_ver", mg_vehicle_get_firmware_version(nullptr));
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

        // 1. Vehicle summary
        test_ok("vehicle_summary", mg_vehicle_get_summary(core), "\"flight_mode\":");

        // 2. Global position
        test_ok("global_pos", mg_vehicle_get_global_position(core), "\"lat_deg\":");

        // 3. Attitude
        test_ok("attitude", mg_vehicle_get_attitude(core), "\"roll_deg\":");

        // 4. RC Channels（这两个接口回裸载荷，不带 success 包封）
        {
            char* r = mg_rc_get_channels(core);
            check(contains(r, "\"channel_count\":"), "rc_channels has channel_count");
            mg_free_string(r);
        }

        // 5. Servo Outputs
        {
            char* r = mg_servo_get_outputs(core);
            check(contains(r, "\"servos\":"), "servo_outputs has servos");
            mg_free_string(r);
        }

        // 6. Home Position
        test_ok("home_pos", mg_vehicle_get_home_position(core), "\"lat_deg\":");

        // 7. VFR HUD
        test_ok("vfr_hud", mg_vehicle_get_vfr_hud(core), "\"airspeed_m_s\":");

        // 8. Extended sys state
        test_ok("ext_sys", mg_vehicle_get_extended_sys_state(core), "\"landed_state\":");

        // 9. Firmware version
        test_ok("firmware_ver", mg_vehicle_get_firmware_version(core), "\"received\":");

        mg_destroy(core);
    }

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}

