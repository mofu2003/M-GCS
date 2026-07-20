/**
 * test_ffi_comprehensive.cc - Comprehensive offline FFI function tests
 *
 * Tests all FFI functions that can be verified without a real FC connection.
 * Uses mg_create() + no connection to test null/disconnected paths.
 * Uses InMemoryLink for connected-path tests where needed.
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* label) {
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", label); }
    else      { g_fail++; fprintf(stderr, "  [FAIL] %s\n", label); }
}

static bool contains(const char* json, const char* needle) {
    return json && strstr(json, needle) != nullptr;
}

static bool is_success(const char* json) { return contains(json, "\"success\":true"); }
static bool is_failure(const char* json) { return contains(json, "\"success\":false"); }

static void test_null_core() {
    fprintf(stderr, "\n[Group] Null core safety\n");
    // All functions should handle null core gracefully
    check(is_failure(mg_connection_get_state(nullptr)), "connection_get_state null");
    check(mg_connection_disconnect(nullptr) == nullptr || is_failure(mg_connection_disconnect(nullptr)), "disconnect null");
    check(is_failure(mg_firmware_get_type(nullptr)), "firmware_get_type null");
    check(is_failure(mg_health_get_all(nullptr)), "health_get_all null");
    check(is_failure(mg_preflight_check(nullptr)), "preflight_check null");
    check(is_failure(mg_rc_get_channels(nullptr)), "rc_get_channels null");
    check(is_failure(mg_servo_get_outputs(nullptr)), "servo_get_outputs null");
    check(is_failure(mg_param_list(nullptr)), "param_list null");
    check(is_failure(mg_param_get(nullptr, "TEST")), "param_get null");
    // check(is_failure(mg_geofence_get(nullptr)), "geofence_get null"); // not linked
    // check(is_failure(mg_logging_get_stats(nullptr)), "logging_stats null"); // not linked
    check(mg_vehicle_get_active(nullptr) == -1, "vehicle_get_active null -> -1");
    check(mg_vehicle_select(nullptr, 0) == nullptr || is_failure(mg_vehicle_select(nullptr, 0)), "vehicle_select null");
}

static void test_lifecycle() {
    fprintf(stderr, "\n[Group] Lifecycle\n");
    mg_core_t* core = mg_create();
    check(core != nullptr, "mg_create returns non-null");

    // State before connection
    char* state = mg_connection_get_state(core);
    check(contains(state, "\"connected\":false"), "initial state: not connected");
    mg_free_string(state);

    // Vehicle list should be empty
    char* list = mg_vehicle_list(core);
    check(contains(list, "\"count\":0") || contains(list, "[]") || contains(list, "\"count\":0,"), "empty vehicle list");
    mg_free_string(list);

    // Active vehicle should be -1
    check(mg_vehicle_get_active(core) == -1, "no active vehicle");

    // Version
    const char* ver = mg_get_version();
    check(ver != nullptr && ver[0] != '\0', "version non-empty");

    // Error system
    mg_clear_last_error();
    check(mg_get_last_error() == MG_ERROR_NONE, "error cleared to NONE");

    mg_destroy(core);
}

static void test_disconnected_v1_getters() {
    fprintf(stderr, "\n[Group] v1 getters (disconnected, no crash)\n");
    mg_core_t* core = mg_create();
    check(core != nullptr, "mg_create");

    // All v1 getters should return valid JSON without crashing
    char* r;
    r = mg_vehicle_get_summary(core);     check(r != nullptr, "v1 summary no crash"); mg_free_string(r);
    r = mg_vehicle_get_battery(core);     check(r != nullptr, "v1 battery no crash"); mg_free_string(r);
    r = mg_vehicle_get_gps(core);         check(r != nullptr, "v1 gps no crash"); mg_free_string(r);
    r = mg_vehicle_get_attitude(core);    check(r != nullptr, "v1 attitude no crash"); mg_free_string(r);
    r = mg_vehicle_get_firmware(core);    check(r != nullptr, "v1 firmware no crash"); mg_free_string(r);
    r = mg_vehicle_get_firmware_version(core); check(r != nullptr, "v1 fw_ver no crash"); mg_free_string(r);
    r = mg_vehicle_get_global_position(core);  check(r != nullptr, "v1 global_pos no crash"); mg_free_string(r);
    r = mg_vehicle_get_home_position(core);    check(r != nullptr, "v1 home no crash"); mg_free_string(r);
    r = mg_vehicle_get_extended_sys_state(core); check(r != nullptr, "v1 ext_sys no crash"); mg_free_string(r);
    r = mg_vehicle_get_vfr_hud(core);     check(r != nullptr, "v1 vfr_hud no crash"); mg_free_string(r);
    r = mg_vehicle_get_imu(core);         check(r != nullptr, "v1 imu no crash"); mg_free_string(r);
    r = mg_vehicle_get_pressure(core);    check(r != nullptr, "v1 pressure no crash"); mg_free_string(r);
    r = mg_sensors_get_health(core);      check(r != nullptr, "v1 health no crash"); mg_free_string(r);
    r = mg_sensors_get_statustext(core);  check(r != nullptr, "v1 statustext no crash"); mg_free_string(r);
    r = mg_vehicle_get_battery(core); check(r != nullptr, "v1 batt_status no crash"); mg_free_string(r);
    r = mg_vehicle_get_highres_imu(core); check(r != nullptr, "v1 highres_imu no crash"); mg_free_string(r);
    r = mg_vehicle_get_vibration(core);   check(r != nullptr, "v1 vibration no crash"); mg_free_string(r);
    r = mg_vehicle_get_prearm_check(core); check(r != nullptr, "v1 prearm no crash"); mg_free_string(r);
    r = mg_vehicle_get_esc_status(core);  check(r != nullptr, "v1 esc no crash"); mg_free_string(r);
    r = mg_vehicle_get_wind_estimate(core); check(r != nullptr, "v1 wind no crash"); mg_free_string(r);

    mg_destroy(core);
}

static void test_disconnected_v2_getters() {
    fprintf(stderr, "\n[Group] v2 getters (disconnected, returns defaults)\n");
    mg_core_t* core = mg_create();

    // All v2 getters should return success with default values when disconnected
    char* r;
    r = mg_vehicle_get_summary(core);  check(is_success(r), "v2 summary success"); mg_free_string(r);
    r = mg_vehicle_get_battery(core);  check(is_success(r), "v2 battery success"); mg_free_string(r);
    r = mg_vehicle_get_gps(core);      check(is_success(r), "v2 gps success"); mg_free_string(r);
    r = mg_vehicle_get_attitude(core); check(is_success(r), "v2 attitude success"); mg_free_string(r);
    r = mg_vehicle_get_global_position(core); check(is_success(r), "v2 global_pos success"); mg_free_string(r);
    r = mg_vehicle_get_rc_channels(core); check(is_success(r), "v2 rc success"); mg_free_string(r);
    r = mg_vehicle_get_servo_output(core); check(is_success(r), "v2 servo success"); mg_free_string(r);
    r = mg_vehicle_get_home_position(core); check(is_success(r), "v2 home success"); mg_free_string(r);
    r = mg_vehicle_get_vfr_hud(core);  check(is_success(r), "v2 vfr_hud success"); mg_free_string(r);
    r = mg_vehicle_get_imu(core);      check(is_success(r), "v2 imu success"); mg_free_string(r);
    r = mg_vehicle_get_pressure(core); check(is_success(r), "v2 pressure success"); mg_free_string(r);
    r = mg_vehicle_get_highres_imu(core); check(is_success(r), "v2 highres_imu success"); mg_free_string(r);
    r = mg_vehicle_get_extended_sys_state(core); check(is_success(r), "v2 ext_sys success"); mg_free_string(r);
    r = mg_vehicle_get_firmware_version(core); check(is_success(r), "v2 fw_ver success"); mg_free_string(r);
    r = mg_vehicle_get_vibration(core); check(is_success(r), "v2 vibration success"); mg_free_string(r);
    r = mg_vehicle_get_esc_status(core); check(is_success(r), "v2 esc success"); mg_free_string(r);
    r = mg_vehicle_get_wind_estimate(core); check(is_success(r), "v2 wind success"); mg_free_string(r);
    r = mg_vehicle_get_prearm_check(core); check(is_success(r), "v2 prearm success"); mg_free_string(r);

    mg_destroy(core);
}

static void test_disconnected_commands() {
    fprintf(stderr, "\n[Group] Commands (disconnected, should fail)\n");
    mg_core_t* core = mg_create();

    // check(is_failure(mg_cmd_arm(core)), "arm disconnected -> fail"); // not linked
    // check(is_failure(mg_cmd_disarm(core)), "disarm disconnected -> fail"); // not linked
    // check(is_failure(mg_cmd_rtl(core)), "rtl disconnected -> fail"); // not linked
    // check(is_failure(mg_cmd_takeoff(core, 10)), "takeoff disconnected -> fail"); // not linked
    // check(is_failure(mg_cmd_land(core)), "land disconnected -> fail"); // not linked
    // check(is_failure(mg_cmd_set_mode(core, "Stabilize")), "set_mode disconnected -> fail"); // not linked

    // Params
    check(is_failure(mg_param_set(core, "TEST", 1.0f)), "param_set disconnected -> fail");

    // Mission
    // check(is_failure(mg_mission_download(core)), "mission_download disconnected -> fail"); // not linked

    mg_destroy(core);
}

static void test_disconnected_health() {
    fprintf(stderr, "\n[Group] Health & firmware (disconnected)\n");
    mg_core_t* core = mg_create();

    char* r;
    r = mg_health_get_all(core);
    check(r != nullptr && contains(r, "\"connected\":false"), "health disconnected");
    mg_free_string(r);

    r = mg_firmware_get_type(core);
    check(r != nullptr, "firmware_type no crash");
    mg_free_string(r);

    r = mg_preflight_check(core);
    check(r != nullptr, "preflight_check no crash");
    mg_free_string(r);

    r = mg_rc_get_channels(core);
    check(r != nullptr, "rc_get_channels no crash");
    mg_free_string(r);

    r = mg_servo_get_outputs(core);
    check(r != nullptr, "servo_get_outputs no crash");
    mg_free_string(r);

    mg_destroy(core);
}

static void test_firmware_type_normalization_contract() {
    fprintf(stderr, "\n[Group] Firmware type normalization contract (disconnected defaults)\n");
    mg_core_t* core = mg_create();

    char* r = mg_firmware_get_type(core);
    check(r != nullptr, "firmware_type returns json");
    check(is_success(r), "firmware_type uses success envelope");
    check(contains(r, "\"data\""), "firmware_type contains data envelope");
    check(contains(r, "\"schema_version\""), "firmware_type contains schema_version");
    check(contains(r, "\"firmware\""), "firmware_type contains firmware");
    check(contains(r, "\"profile\""), "firmware_type contains profile");
    check(contains(r, "\"vehicle_type\""), "firmware_type contains vehicle_type");
    check(contains(r, "\"capability_flags\""), "firmware_type contains capability_flags");
    check(contains(r, "\"capabilities\""), "firmware_type contains capabilities object");
    check(contains(r, "\"guided_mode\""), "firmware_type capabilities include guided_mode");
    mg_free_string(r);

    mg_destroy(core);
}

static void test_backend_hardening_contracts() {
    fprintf(stderr, "\n[Group] Backend hardening contracts\n");
    mg_core_t* core = mg_create();

    char* conn = mg_connection_get_diagnostics(core);
    check(is_success(conn), "connection diagnostics success envelope");
    check(contains(conn, "\"connected\":false"), "connection diagnostics reports disconnected");
    check(contains(conn, "\"reconnectable\":true"), "connection diagnostics reports reconnectable");
    check(contains(conn, "\"target\""), "connection diagnostics contains target");
    mg_free_string(conn);

    char* ops = mg_operations_get_snapshot(core);
    check(is_success(ops), "operations snapshot success envelope");
    check(contains(ops, "\"id\":\"params\""), "operations snapshot includes params");
    check(contains(ops, "\"id\":\"mission\""), "operations snapshot includes mission");
    check(contains(ops, "\"state\":\"idle\""), "operations snapshot starts idle");
    mg_free_string(ops);

    char* cancel_unknown = mg_operations_cancel(core, "unknown");
    check(is_failure(cancel_unknown), "unknown operation cancel fails");
    check(contains(cancel_unknown, "\"unknown operation\""), "unknown operation cancel explains reason");
    mg_free_string(cancel_unknown);

    char* cancel_params = mg_operations_cancel(core, "params");
    check(is_success(cancel_params), "known operation cancel returns success envelope");
    check(contains(cancel_params, "\"canceled\":false"), "idle operation cancel is explicit no-op");
    mg_free_string(cancel_params);

    char* dash = mg_telemetry_get_dashboard_snapshot(core);
    check(is_success(dash), "dashboard snapshot success envelope");
    check(contains(dash, "\"connection\""), "dashboard includes connection");
    check(contains(dash, "\"telemetry\""), "dashboard includes telemetry");
    check(contains(dash, "\"validity\""), "dashboard includes validity");
    mg_free_string(dash);

    char* sys = mg_system_get_diagnostics(core);
    check(is_success(sys), "system diagnostics success envelope");
    check(contains(sys, "\"last_error\""), "system diagnostics includes last_error");
    check(contains(sys, "\"dashboard\""), "system diagnostics includes dashboard");
    mg_free_string(sys);

    mg_destroy(core);
}

static void test_disconnected_config() {
    fprintf(stderr, "\n[Group] Config & geofence (disconnected)\n");
    mg_core_t* core = mg_create();

    // check(is_failure(mg_config_read_param(core, "TEST")), "config_read disconnected -> fail"); // not linked
    check(is_failure(mg_config_write_param(core, "TEST", 1.0f)), "config_write disconnected -> fail");

    char* r;
    r = nullptr; // mg_geofence_get not linked (skipped)
    mg_free_string(r);

    r = nullptr; // mg_logging_get_stats not linked (skipped)
    mg_free_string(r);

    mg_destroy(core);
}

static void test_disconnected_stubs() {
    fprintf(stderr, "\n[Group] Stub functions (should return NOT_SUPPORTED)\n");
    mg_core_t* core = mg_create();

    // SITL is now real — start may fail (no SITL binary) or succeed, just verify no crash
    { char* r = mg_sitl_start(core, ""); if (r) mg_free_string(r); }
    check(true, "sitl_start no crash");
    { char* r = mg_sitl_stop(core); if (r) mg_free_string(r); }
    check(true, "sitl_stop no crash");
    check(is_failure(mg_firmware_flash(core, "")), "firmware_flash -> NOT_SUPPORTED");
    check(is_failure(mg_px4_actuator_set_config(core, "")), "px4_actuator -> NOT_SUPPORTED");
    check(is_failure(mg_px4_flight_mode_get_config(core)), "px4_flight_mode -> NOT_SUPPORTED");

    mg_destroy(core);
}

static void test_error_system() {
    fprintf(stderr, "\n[Group] Error system\n");

    mg_clear_last_error();
    check(mg_get_last_error() == MG_ERROR_NONE, "clear -> NONE");

    mg_set_last_error(MG_ERROR_NULL_CORE);
    check(mg_get_last_error() == MG_ERROR_NULL_CORE, "set/get error round-trip");
    mg_clear_last_error();
    check(mg_get_last_error() == MG_ERROR_NONE, "clear after set");
}

static void test_sensor_decode() {
    fprintf(stderr, "\n[Group] Sensor decode (offline)\n");
    mg_core_t* core = mg_create();

    // These should work without connection (offline decode)
    char* r;
    r = mg_sensor_decode_device_id(core, 0x00010001, "ins");
    check(r != nullptr, "decode_device_id ins no crash");
    mg_free_string(r);

    r = mg_sensor_decode_device_id(core, 0x00010001, "compass");
    check(r != nullptr, "decode_device_id compass no crash");
    mg_free_string(r);

    r = mg_sensor_decode_device_id(core, 0x00010001, "baro");
    check(r != nullptr, "decode_device_id baro no crash");
    mg_free_string(r);

    r = mg_sensor_decode_type_param(core, 0, "ins");
    check(r != nullptr, "decode_type_param no crash");
    mg_free_string(r);

    r = mg_sensor_decode_px4_id(core, 0x00010001, "imu");
    check(r != nullptr, "decode_px4_id no crash");
    mg_free_string(r);

    r = mg_sensor_decode_px4_type(core, 0, "imu");
    check(r != nullptr, "decode_px4_type no crash");
    mg_free_string(r);

    mg_destroy(core);
}

int main() {
    fprintf(stderr, "=== Comprehensive FFI Test Suite ===\n");

    test_null_core();
    test_lifecycle();
    test_disconnected_v1_getters();
    test_disconnected_v2_getters();
    test_disconnected_commands();
    test_disconnected_health();
    test_firmware_type_normalization_contract();
    test_backend_hardening_contracts();
    test_disconnected_config();
    test_disconnected_stubs();
    test_error_system();
    test_sensor_decode();

    fprintf(stderr, "\n========================================\n");
    fprintf(stderr, "  Results: %d PASSED, %d FAILED\n", g_pass, g_fail);
    fprintf(stderr, "========================================\n");
    return g_fail > 0 ? 1 : 0;
}


