/**
 * test_ffi_contract.cc - FFI bridge contract tests
 *
 * Tests: create/destroy lifecycle, null safety, subsystem routing,
 *        error handling, version string, free_string
 */
#include "ffi_api.h"
#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

// Helper: check JSON contains a key
static bool json_has(const char* json, const char* key) {
    if (!json) return false;
    return strstr(json, key) != nullptr;
}

// Helper: check JSON starts with { or [
static bool json_valid_envelope(const char* json) {
    if (!json) return false;
    return json[0] == '{' || json[0] == '[';
}

int main() {
    fprintf(stderr, "=== FFI Contract Tests ===\n\n");

    // 1. Lifecycle
    fprintf(stderr, "[Group] Lifecycle\n");
    {
        mg_core_t* core = mg_create();
        CHECK(core != nullptr, "mg_create returns non-null");
        mg_destroy(core);
    }

    // 2. Null safety - all functions should not crash with null core
    fprintf(stderr, "[Group] Null safety\n");
    {
        const char* r;
        r = mg_connection_get_state(nullptr);
        CHECK(r != nullptr, "get_state null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_list(nullptr);
        CHECK(r != nullptr, "vehicle_list null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_get_summary(nullptr);
        CHECK(r != nullptr, "v2_vehicle_get_summary null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_get_battery(nullptr);
        CHECK(r != nullptr, "v2_vehicle_get_battery null core returns non-null");
        mg_free_string((char*)r);

        r = mg_vehicle_get_global_position(nullptr);
        CHECK(r != nullptr, "v2_vehicle_get_global_position null core returns non-null");
        mg_free_string((char*)r);

        // disconnect should not crash
        r = mg_connection_disconnect(nullptr);
        CHECK(r == nullptr || json_has(r, "error"), "disconnect null core returns null/error");
        if (r) mg_free_string((char*)r);
    }

    // 3. Version string
    fprintf(stderr, "[Group] Version\n");
    {
        char* v = mg_get_version();
        CHECK(v != nullptr, "version string non-null");
        CHECK(strlen(v) > 0, "version string non-empty");
        fprintf(stderr, "    version: %s\n", v);
        mg_free_string(v);
    }

    // 4. Error system
    fprintf(stderr, "[Group] Error system\n");
    {
        mg_clear_last_error();
        CHECK(mg_get_last_error() == MG_ERROR_NONE, "clear_last_error resets to NONE");

        // Trigger an error
        char* r = mg_connection_connect(nullptr, "COM99", 115200);
        CHECK(json_has(r, "error"), "connect invalid port returns error JSON");
        CHECK(mg_get_last_error() != MG_ERROR_NONE, "error code set after failed connect");
        mg_free_string(r);
    }

    // 5. Disconnected state
    fprintf(stderr, "[Group] Disconnected state\n");
    {
        mg_core_t* core = mg_create();
        char* state = mg_connection_get_state(core);
        CHECK(json_valid_envelope(state), "get_state returns valid JSON envelope");
        CHECK(json_has(state, "connected"), "get_state has 'connected' field");
        mg_free_string(state);

        char* summary = mg_vehicle_get_summary(core);
        CHECK(json_valid_envelope(summary), "v2 summary returns valid JSON");
        mg_free_string(summary);

        char* battery = mg_vehicle_get_battery(core);
        CHECK(json_valid_envelope(battery), "v2 battery returns valid JSON");
        mg_free_string(battery);

        char* sensor = mg_sensor_decode_device_id(core, 0x00010001, "ins");
        CHECK(json_valid_envelope(sensor), "offline sensor decode returns valid JSON");
        mg_free_string(sensor);

        mg_destroy(core);
    }

    // 6. Multi-vehicle API
    fprintf(stderr, "[Group] Multi-vehicle API\n");
    {
        mg_core_t* core = mg_create();

        // List should be empty
        char* list = mg_vehicle_list(core);
        CHECK(json_has(list, "[]") || json_has(list, "count"), "vehicle list is empty array or has count");
        mg_free_string(list);

        // Active should be -1 before any selection
        int active = mg_vehicle_get_active(core);
        CHECK(active == -1, "no active vehicle initially (-1)");

        // Select non-existent vehicle
        char* sel = mg_vehicle_select(core, 999);
        CHECK(sel != nullptr, "select returns non-null response");
        CHECK(json_has(sel, "\"success\":false"), "select non-existent vehicle returns failure");
        CHECK(mg_vehicle_get_active(core) == -1, "select non-existent vehicle leaves active unchanged");
        mg_free_string(sel);

        mg_destroy(core);
    }

    // 7. Free string safety
    fprintf(stderr, "[Group] Free string safety\n");
    {
        mg_free_string(nullptr); // should not crash
        CHECK(true, "free_string(nullptr) doesn't crash");

        char* s = mg_get_version();
        mg_free_string(s);
        CHECK(true, "free_string(valid pointer) doesn't crash");
    }

    // 8. PX4 placeholders return NOT_SUPPORTED
    fprintf(stderr, "[Group] PX4 placeholders\n");
    {
        mg_core_t* core = mg_create();

        char* r1 = mg_px4_actuator_get_config(core);
        CHECK(r1 != nullptr, "px4_actuator_get_config returns non-null");
        mg_free_string(r1);

        char* r2 = mg_px4_safety_get(core);
        CHECK(r2 != nullptr, "px4_safety_get returns non-null");
        mg_free_string(r2);

        mg_destroy(core);
    }

    // 9. Settings APIs are safe while disconnected
    fprintf(stderr, "[Group] Settings API disconnected safety\n");
    {
        mg_core_t* core = mg_create();

        char* serial = mg_serial_get_configs(core);
        CHECK(json_valid_envelope(serial), "serial configs returns JSON");
        mg_free_string(serial);

        char* rc = mg_rc_get_channels(core);
        CHECK(json_valid_envelope(rc), "rc channels returns JSON");
        mg_free_string(rc);

        char* servo = mg_servo_get_outputs(core);
        CHECK(json_valid_envelope(servo), "servo outputs returns JSON");
        mg_free_string(servo);

        char* modes = mg_flight_get_available_modes(core);
        CHECK(json_valid_envelope(modes), "available modes returns JSON");
        mg_free_string(modes);

        char* preflight = mg_preflight_check(core);
        CHECK(json_valid_envelope(preflight), "preflight check returns JSON");
        mg_free_string(preflight);

        mg_destroy(core);
    }

    // 10. Industrial hardening APIs are safe while disconnected
    fprintf(stderr, "[Group] Backend hardening API disconnected safety\n");
    {
        mg_core_t* core = mg_create();

        char* conn_diag = mg_connection_get_diagnostics(core);
        CHECK(json_valid_envelope(conn_diag), "connection diagnostics returns JSON");
        CHECK(json_has(conn_diag, "state"), "connection diagnostics has state");
        CHECK(json_has(conn_diag, "heartbeat"), "connection diagnostics has heartbeat");
        CHECK(json_has(conn_diag, "link"), "connection diagnostics has link");
        CHECK(json_has(conn_diag, "quality_pct"), "connection diagnostics has link quality");
        mg_free_string(conn_diag);

        char* ops = mg_operations_get_snapshot(core);
        CHECK(json_valid_envelope(ops), "operations snapshot returns JSON");
        CHECK(json_has(ops, "operations"), "operations snapshot has operations");
        CHECK(json_has(ops, "params"), "operations snapshot has params operation");
        CHECK(json_has(ops, "mission"), "operations snapshot has mission operation");
        mg_free_string(ops);

        char* cancel = mg_operations_cancel(core, "params");
        CHECK(json_valid_envelope(cancel), "operations cancel returns JSON");
        CHECK(json_has(cancel, "success"), "operations cancel has success envelope");
        CHECK(json_has(cancel, "canceled"), "operations cancel reports canceled");
        mg_free_string(cancel);

        char* dashboard = mg_telemetry_get_dashboard_snapshot(core);
        CHECK(json_valid_envelope(dashboard), "dashboard snapshot returns JSON");
        CHECK(json_has(dashboard, "connection"), "dashboard snapshot has connection");
        CHECK(json_has(dashboard, "telemetry"), "dashboard snapshot has telemetry");
        CHECK(json_has(dashboard, "freshness"), "dashboard snapshot has freshness");
        mg_free_string(dashboard);

        char* system = mg_system_get_diagnostics(core);
        CHECK(json_valid_envelope(system), "system diagnostics returns JSON");
        CHECK(json_has(system, "connection"), "system diagnostics has connection");
        CHECK(json_has(system, "operations"), "system diagnostics has operations");
        CHECK(json_has(system, "last_error"), "system diagnostics has last_error");
        mg_free_string(system);

        mg_destroy(core);
    }

    // 11. Industrial hardening APIs handle null core consistently
    fprintf(stderr, "[Group] Backend hardening API null safety\n");
    {
        char* conn_diag = mg_connection_get_diagnostics(nullptr);
        CHECK(json_valid_envelope(conn_diag), "connection diagnostics null core returns JSON");
        CHECK(json_has(conn_diag, "error"), "connection diagnostics null core reports error");
        mg_free_string(conn_diag);

        char* ops = mg_operations_get_snapshot(nullptr);
        CHECK(json_valid_envelope(ops), "operations snapshot null core returns JSON");
        CHECK(json_has(ops, "error"), "operations snapshot null core reports error");
        mg_free_string(ops);

        char* cancel = mg_operations_cancel(nullptr, "params");
        CHECK(json_valid_envelope(cancel), "operations cancel null core returns JSON");
        CHECK(json_has(cancel, "error"), "operations cancel null core reports error");
        mg_free_string(cancel);

        char* dashboard = mg_telemetry_get_dashboard_snapshot(nullptr);
        CHECK(json_valid_envelope(dashboard), "dashboard snapshot null core returns JSON");
        CHECK(json_has(dashboard, "error"), "dashboard snapshot null core reports error");
        mg_free_string(dashboard);

        char* system = mg_system_get_diagnostics(nullptr);
        CHECK(json_valid_envelope(system), "system diagnostics null core returns JSON");
        CHECK(json_has(system, "error"), "system diagnostics null core reports error");
        mg_free_string(system);
    }

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
