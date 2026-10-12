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
        {"preflight_check", mg_preflight_check(nullptr)},
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

// F4 + 参数文件内容变换：成功路径必须回解析结果，失败路径必须回统一 error 对象。
// 旧实现里 mg_param_file_import_csv 恒回 success:true 且丢掉解析数组，导出函数在
// 非法输入时回空串（不是合法 JSON）。
static void test_param_file_transform_contract() {
    fprintf(stderr, "[Group] Parameter file transform contract\n");
    auto* core = mg_create();

    // CSV 导入：返回解析出的参数数组，字段名与同族的 .param 导入一致
    // （items 数组 + count + 统一 error 对象）。
    {
        char* r = mg_param_file_import_csv(
            core, "name,value,type\nINS_GYROFFS_X,0.5,9\nINS_GYROFFS_Y,-0.25,9\n");
        check(contains(r, "\"success\":true"), "csv import: success true");
        check(contains(r, "\"count\":2"), "csv import: count is 2");
        check(contains(r, "\"items\":["), "csv import: items array present");
        check(contains(r, "\"name\":\"INS_GYROFFS_X\""), "csv import: name field");
        check(contains(r, "\"value\":0.5"), "csv import: value field");
        check(contains(r, "\"type\":9"), "csv import: type field");
        mg_free_string(r);
    }

    // 垃圾输入：表头之后没有一条能解析 → success:false + 统一 error 对象。
    {
        char* r = mg_param_file_import_csv(core, "not a csv\nstill not a csv\n");
        check(contains(r, "\"success\":false"), "csv import garbage: success false");
        check(contains(r, "\"count\":0"), "csv import garbage: count 0");
        check(contains(r, "\"error\":{"), "csv import garbage: error is an object");
        check(contains(r, "\"code\":-4"), "csv import garbage: numeric code");
        check(contains(r, "\"name\":\""), "csv import garbage: symbolic name");
        check(contains(r, "\"message\":\""), "csv import garbage: message");
        check(!contains(r, "\"error\":\""), "csv import garbage: no bare error string");
        mg_free_string(r);
    }

    // 空输入：同样是带 error 对象的失败响应。
    {
        char* r = mg_param_file_import_csv(core, "");
        check(contains(r, "\"success\":false"), "csv import empty: success false");
        check(contains(r, "\"error\":{"), "csv import empty: error is an object");
        mg_free_string(r);
    }

    // 导出：非法输入回合法 JSON（error 对象），不再回空串。
    {
        char* r = mg_param_file_export(core, "");
        check(r != nullptr && r[0] != '\0',
              "param export empty input: not an empty string");
        check(contains(r, "\"success\":false"), "param export empty input: success false");
        check(contains(r, "\"error\":{"), "param export empty input: error object");
        mg_free_string(r);
    }
    {
        char* r = mg_param_file_export_csv(core, "{\"not\":\"an array\"}");
        check(r != nullptr && r[0] != '\0',
              "csv export invalid json: not an empty string");
        check(contains(r, "\"success\":false"), "csv export invalid json: success false");
        check(contains(r, "\"error\":{"), "csv export invalid json: error object");
        mg_free_string(r);
    }

    // F4：未知 operation_id 的 success:false 必须带 error 对象（原先只有 reason）。
    {
        char* r = mg_operations_cancel(core, "no-such-operation");
        check(contains(r, "\"success\":false"), "operations_cancel unknown id: success false");
        check(contains(r, "\"reason\":\"unknown operation\""),
              "operations_cancel unknown id: reason kept for the UI");
        check(contains(r, "\"error\":{"), "operations_cancel unknown id: error object");
        check(contains(r, "\"code\":-4"), "operations_cancel unknown id: numeric code");
        check(!contains(r, "\"error\":\""), "operations_cancel unknown id: no bare error string");
        mg_free_string(r);
    }

    mg_destroy(core);
}

int main() {
    fprintf(stderr, "=== FFI Telemetry Response Contract Tests ===\n\n");

    test_failure_envelope_shape();
    test_param_file_transform_contract();

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

