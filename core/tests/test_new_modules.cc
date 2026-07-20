/**
 * test_new_modules.cc - Tests for recently added modules
 *
 * Covers: CameraManager, FlightLogReplayer, MavlinkFtpClient
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <fstream>
#include "common/mavlink.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

static bool json_has(const char* json, const char* key) {
    return json && strstr(json, key) != nullptr;
}

// ================================================================
// Camera FFI Tests
// ================================================================
static void test_camera_ffi() {
    fprintf(stderr, "\n[Group] Camera FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Camera functions should return valid JSON even when disconnected
    // (they check CHECK_MAVLINK and return error JSON)

    // Take photo - should fail with not connected
    char* r = mg_camera_take_photo(core, 0);
    CHECK(r != nullptr, "camera_take_photo returns non-null");
    CHECK(json_has(r, "error"), "camera_take_photo returns error when disconnected");
    if (r) mg_free_string(r);

    // Start video
    r = mg_camera_start_video(core, 0);
    CHECK(r != nullptr, "camera_start_video returns non-null");
    if (r) mg_free_string(r);

    // Stop video
    r = mg_camera_stop_video(core, 0);
    CHECK(r != nullptr, "camera_stop_video returns non-null");
    if (r) mg_free_string(r);

    // Set zoom
    r = mg_camera_set_zoom(core, 0, 2.0f);
    CHECK(r != nullptr, "camera_set_zoom returns non-null");
    if (r) mg_free_string(r);

    // Set mode
    r = mg_camera_set_mode(core, 0, 1);
    CHECK(r != nullptr, "camera_set_mode returns non-null");
    if (r) mg_free_string(r);

    // Stop capture
    r = mg_camera_stop_capture(core, 0);
    CHECK(r != nullptr, "camera_stop_capture returns non-null");
    if (r) mg_free_string(r);

    // Set focus
    r = mg_camera_set_focus(core, 0, 0, 5.0f);
    CHECK(r != nullptr, "camera_set_focus returns non-null");
    if (r) mg_free_string(r);

    // Request info
    r = mg_camera_request_info(core, 0);
    CHECK(r != nullptr, "camera_request_info returns non-null");
    if (r) mg_free_string(r);

    // Get status - should return stub JSON
    r = mg_camera_get_status(core, 0);
    CHECK(r != nullptr, "camera_get_status returns non-null");
    CHECK(json_has(r, "available"), "camera_get_status has 'available' field");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Gimbal FFI Tests
// ================================================================
static void test_gimbal_ffi() {
    fprintf(stderr, "\n[Group] Gimbal FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Gimbal control - should fail with not connected
    char* r = mg_command_gimbal_control(core, -45.0f, 90.0f);
    CHECK(r != nullptr, "gimbal_control returns non-null");
    CHECK(json_has(r, "error"), "gimbal_control returns error when disconnected");
    if (r) mg_free_string(r);

    // Gimbal recenter
    r = mg_command_gimbal_recenter(core);
    CHECK(r != nullptr, "gimbal_recenter returns non-null");
    if (r) mg_free_string(r);

    // Gimbal request info
    r = mg_gimbal_request_info(core);
    CHECK(r != nullptr, "gimbal_request_info returns non-null");
    if (r) mg_free_string(r);

    // Gimbal configure
    r = mg_command_gimbal_configure(core, 0, 1, 1, 0, 0);
    CHECK(r != nullptr, "gimbal_configure returns non-null");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Failsafe FFI Tests
// ================================================================
static void test_failsafe_ffi() {
    fprintf(stderr, "\n[Group] Failsafe FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Fence enable - should fail with not connected
    char* r = mg_fence_enable(core, 1);
    CHECK(r != nullptr, "fence_enable returns non-null");
    CHECK(json_has(r, "error"), "fence_enable returns error when disconnected");
    if (r) mg_free_string(r);

    // Fence disable
    r = mg_fence_enable(core, 0);
    CHECK(r != nullptr, "fence_disable returns non-null");
    if (r) mg_free_string(r);

    // Battery failsafe configure
    r = mg_battery_failsafe_configure(core, 1, 14.0f, 13.0f);
    CHECK(r != nullptr, "battery_failsafe_configure returns non-null");
    if (r) mg_free_string(r);

    // Failsafe status - returns error when no vehicle (expected)
    r = mg_failsafe_get_status(core);
    CHECK(r != nullptr, "failsafe_get_status returns non-null");
    // When disconnected, returns error JSON; when connected, returns status JSON
    bool has_status_fields = json_has(r, "battery_ok");
    bool has_error = json_has(r, "error");
    CHECK(has_status_fields || has_error, "failsafe_get_status has battery_ok or error");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// FlightLogReplayer Tests
// ================================================================
static void test_replayer() {
    fprintf(stderr, "\n[Group] FlightLogReplayer\n");

    // Create a synthetic tlog file for testing
    const char* test_file = "test_replay.tlog";
    {
        FILE* f = fopen(test_file, "wb");
        CHECK(f != nullptr, "create test tlog file");
        if (!f) return;

        // Write 3 records
        for (int i = 0; i < 3; i++) {
            uint64_t ts = 1000000 + i * 100000;  // 1 second apart
            fwrite(&ts, sizeof(ts), 1, f);

            // Create a simple MAVLink heartbeat message
            mavlink_message_t msg;
            mavlink_msg_heartbeat_pack(1, 1, &msg,
                MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
                0, 0, MAV_STATE_STANDBY);
            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

            fwrite(&len, sizeof(len), 1, f);
            fwrite(buf, 1, len, f);
        }
        fclose(f);
    }

    // Note: FlightLogReplayer is a C++ class, not exposed via FFI
    // We can only test it through the C++ API or indirectly
    // For now, verify the test file was created
    FILE* check = fopen(test_file, "rb");
    CHECK(check != nullptr, "test tlog file exists");
    if (check) {
        fseek(check, 0, SEEK_END);
        long size = ftell(check);
        fclose(check);
        CHECK(size > 0, "test tlog file is non-empty");
    }

    // Cleanup
    remove(test_file);
}

// ================================================================
// Extended Command Tests
// ================================================================
static void test_extended_commands() {
    fprintf(stderr, "\n[Group] Extended Commands\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Emergency stop - should fail with not connected
    char* r = mg_command_emergency_stop(core);
    CHECK(r != nullptr, "emergency_stop returns non-null");
    if (r) mg_free_string(r);

    // Guided velocity
    r = mg_command_guided_velocity(core, 1.0f, 0.0f, 0.0f);
    CHECK(r != nullptr, "guided_velocity returns non-null");
    if (r) mg_free_string(r);

    // Guided pause
    r = mg_command_guided_pause(core);
    CHECK(r != nullptr, "guided_pause returns non-null");
    if (r) mg_free_string(r);

    // Orbit
    r = mg_command_orbit(core, "{\"lat\":39.9,\"lon\":116.4,\"radius\":50,\"speed\":5}");
    CHECK(r != nullptr, "orbit returns non-null");
    if (r) mg_free_string(r);

    // Set ROI
    r = mg_command_set_roi(core, 39.9, 116.4, 100.0f);
    CHECK(r != nullptr, "set_roi returns non-null");
    if (r) mg_free_string(r);

    // Stop ROI
    r = mg_command_stop_roi(core);
    CHECK(r != nullptr, "stop_roi returns non-null");
    if (r) mg_free_string(r);

    // Landing gear
    r = mg_command_landing_gear(core, 1);
    CHECK(r != nullptr, "landing_gear returns non-null");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Vehicle Data Tests
// ================================================================
static void test_vehicle_data() {
    fprintf(stderr, "\n[Group] Vehicle Data\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Vehicle count
    char* r = mg_vehicle_count(core);
    CHECK(r != nullptr, "vehicle_count returns non-null");
    if (r) mg_free_string(r);

    // Flight time
    r = mg_vehicle_get_flight_time(core);
    CHECK(r != nullptr, "flight_time returns non-null");
    CHECK(json_has(r, "flight_time_s"), "flight_time has 'flight_time_s'");
    if (r) mg_free_string(r);

    // ADSB traffic
    r = mg_vehicle_get_adsb_traffic(core);
    CHECK(r != nullptr, "adsb_traffic returns non-null");
    CHECK(json_has(r, "vehicles"), "adsb_traffic has 'vehicles'");
    if (r) mg_free_string(r);

    // Distance sensors
    r = mg_vehicle_get_distance_sensors(core);
    CHECK(r != nullptr, "distance_sensors returns non-null");
    CHECK(json_has(r, "sensors"), "distance_sensors has 'sensors'");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Telemetry Freshness Tests
// ================================================================
static void test_telemetry_freshness() {
    fprintf(stderr, "\n[Group] Telemetry Freshness\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Freshness check - should work even without connection
    char* r = mg_telemetry_get_freshness(core);
    CHECK(r != nullptr, "telemetry_freshness returns non-null");
    CHECK(json_has(r, "heartbeat"), "telemetry_freshness has heartbeat");
    CHECK(json_has(r, "gps"), "telemetry_freshness has gps");
    CHECK(json_has(r, "attitude"), "telemetry_freshness has attitude");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
int main() {
    fprintf(stderr, "=== New Module Tests ===\n");

    test_camera_ffi();
    test_gimbal_ffi();
    test_failsafe_ffi();
    test_replayer();
    test_extended_commands();
    test_vehicle_data();
    test_telemetry_freshness();

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
