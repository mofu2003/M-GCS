/**
 * test_sim_commands.cc - Command ACK handling simulation tests.
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>
#include <vector>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static bool json_has(const char* json, const char* needle) {
    return json && strstr(json, needle) != nullptr;
}

static bool is_success(const char* json) {
    return json_has(json, "\"success\":true");
}

static bool is_failure(const char* json) {
    return json_has(json, "\"success\":false");
}

using TestEnv = MockVehicleTestEnv;

static bool mode_status_is(const char* json, const char* expected_mode, int expected_custom_mode, bool expected_armed) {
    if (!json) return false;
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    cJSON* mode = cJSON_GetObjectItemCaseSensitive(root, "current_mode");
    cJSON* custom_mode = cJSON_GetObjectItemCaseSensitive(root, "custom_mode");
    cJSON* armed = cJSON_GetObjectItemCaseSensitive(root, "armed");
    const bool ok =
        cJSON_IsString(mode) && mode->valuestring && strcmp(mode->valuestring, expected_mode) == 0 &&
        cJSON_IsNumber(custom_mode) && custom_mode->valueint == expected_custom_mode &&
        ((expected_armed && cJSON_IsTrue(armed)) || (!expected_armed && cJSON_IsFalse(armed)));
    cJSON_Delete(root);
    return ok;
}

static void test_arm() {
    fprintf(stderr, "\n[Group] ARM command\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    env.start_pump();
    char* result = mg_cmd_arm(env.core);
    env.stop_pump();

    CHECK(result != nullptr, "arm returned non-null");
    CHECK(is_success(result), "arm success:true");
    if (result) {
        fprintf(stderr, "    arm result: %s\n", result);
        mg_free_string(result);
    }

    bool found_arm = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == MAV_CMD_COMPONENT_ARM_DISARM && cmd.p1 == 1.0f) {
            found_arm = true;
            break;
        }
    }
    CHECK(found_arm, "mock received ARM cmd with param1=1");
}

static void test_takeoff() {
    fprintf(stderr, "\n[Group] TAKEOFF command\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    env.start_pump();
    char* result = mg_cmd_takeoff(env.core, 10.0f);
    env.stop_pump();

    CHECK(result != nullptr, "takeoff returned non-null");
    CHECK(is_success(result), "takeoff success:true");
    if (result) {
        fprintf(stderr, "    takeoff result: %s\n", result);
        mg_free_string(result);
    }

    bool found_takeoff = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == MAV_CMD_NAV_TAKEOFF) {
            found_takeoff = true;
            break;
        }
    }
    CHECK(found_takeoff, "mock received TAKEOFF cmd");
}

static void test_rtl() {
    fprintf(stderr, "\n[Group] RTL command\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    env.start_pump();
    char* result = mg_cmd_rtl(env.core);
    env.stop_pump();

    CHECK(result != nullptr, "rtl returned non-null");
    CHECK(is_success(result), "rtl success:true");
    if (result) {
        fprintf(stderr, "    rtl result: %s\n", result);
        mg_free_string(result);
    }

    bool found_rtl = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == MAV_CMD_NAV_RETURN_TO_LAUNCH) {
            found_rtl = true;
            break;
        }
    }
    CHECK(found_rtl, "mock received RTL cmd");
}

static void test_mode_change() {
    fprintf(stderr, "\n[Group] MODE CHANGE command\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    env.start_pump();
    char* result = mg_cmd_set_mode(env.core, "Guided");
    env.stop_pump();

    CHECK(result != nullptr, "set_mode returned non-null");
    CHECK(is_success(result), "set_mode success:true");
    if (result) {
        fprintf(stderr, "    set_mode result: %s\n", result);
        mg_free_string(result);
    }

    bool found_mode_cmd = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == MAV_CMD_DO_SET_MODE &&
            static_cast<int>(cmd.p1) == static_cast<int>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) &&
            static_cast<int>(cmd.p2) == 4) {
            found_mode_cmd = true;
            break;
        }
    }
    CHECK(found_mode_cmd, "set_mode sends CUSTOM_MODE_ENABLED base flag and Guided custom mode");
}

static void test_mode_status_tracks_latest_heartbeat() {
    fprintf(stderr, "\n[Group] MODE STATUS follows latest heartbeat\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    char* initial = mg_flight_get_mode_status(env.core);
    CHECK(mode_status_is(initial, "Stabilize", 0, false), "initial mode status is Stabilize from heartbeat");
    if (initial) {
        fprintf(stderr, "    initial mode status: %s\n", initial);
        mg_free_string(initial);
    }

    mavlink_message_t hb;
    mavlink_msg_heartbeat_pack(
        1, 1, &hb,
        MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_SAFETY_ARMED | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        2,
        MAV_STATE_ACTIVE);
    auto bytes = std::vector<uint8_t>(MAVLINK_MAX_PACKET_LEN);
    const uint16_t len = mavlink_msg_to_send_buffer(bytes.data(), &hb);
    env.link->inject_incoming(bytes.data(), len);
    env.tick(10);

    char* updated = mg_flight_get_mode_status(env.core);
    CHECK(mode_status_is(updated, "AltHold", 2, true), "mode status refreshes to AltHold from latest heartbeat");
    if (updated) {
        fprintf(stderr, "    updated mode status: %s\n", updated);
        mg_free_string(updated);
    }
}

static void test_command_nak() {
    fprintf(stderr, "\n[Group] COMMAND NAK\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");
    env.mock->set_next_cmd_result(MAV_RESULT_TEMPORARILY_REJECTED);

    env.start_pump();
    char* result = mg_cmd_arm(env.core);
    env.stop_pump();

    CHECK(result != nullptr, "arm nak returned non-null");
    CHECK(!is_success(result), "arm nak: not success:true");
    CHECK(json_has(result, "\"result\"") || is_failure(result), "arm nak: has result or failure");
    if (result) {
        fprintf(stderr, "    nak result: %s\n", result);
        mg_free_string(result);
    }
}

static void test_command_timeout() {
    fprintf(stderr, "\n[Group] COMMAND TIMEOUT\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");
    env.mock->set_cmd_no_response(true);

    env.start_pump();
    char* result = mg_cmd_arm(env.core);
    env.stop_pump();

    CHECK(result != nullptr, "timeout returned non-null");
    CHECK(is_failure(result), "timeout: success:false");
    CHECK(json_has(result, "timeout") || json_has(result, "\"code\":-6"),
          "timeout: contains timeout indicator or MG_ERROR_TIMEOUT code (-6)");
    if (result) {
        fprintf(stderr, "    timeout result: %s\n", result);
        mg_free_string(result);
    }
}

static void test_arm_no_retry_on_drop() {
    fprintf(stderr, "\n[Group] COMMAND NO RETRY (ARM response dropped)\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");
    env.mock->set_cmd_drop_count(1);

    env.start_pump();
    char* result = mg_cmd_arm(env.core);
    env.stop_pump();

    CHECK(result != nullptr, "arm returned non-null after dropped ACK");
    CHECK(is_failure(result), "arm times out instead of retrying safety command");
    CHECK(env.mock->cmd_dropped_so_far_ >= 1, "at least 1 command was dropped");
    if (result) {
        fprintf(stderr, "    arm result: %s\n", result);
        mg_free_string(result);
    }
}

static void test_safe_cmd_retry_on_drop() {
    fprintf(stderr, "\n[Group] COMMAND RETRY (safe command first attempt dropped)\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");
    env.mock->set_cmd_drop_count(1);

    env.start_pump();
    auto ticket = env.mav->send_command_queued(
        1, 0, MAV_CMD_REQUEST_MESSAGE,
        static_cast<float>(MAVLINK_MSG_ID_AUTOPILOT_VERSION),
        0, 0, 0, 0, 0, 0);
    auto ack = env.mav->wait_for_command(ticket, 5000);
    env.stop_pump();

    CHECK(ack.has_value(), "safe command returned ACK after retry");
    CHECK(ack.has_value() && ack->result == MAV_RESULT_ACCEPTED, "safe command succeeded after retry");
    CHECK(env.mock->cmd_dropped_so_far_ >= 1, "at least 1 command was dropped");
    CHECK(env.mock->received_cmds.size() >= 2, "mock received retry attempt");
}

static void test_param_list_no_response() {
    fprintf(stderr, "\n[Group] PARAM LIST NO RESPONSE\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");
    env.mock->set_param_list_no_response(true);

    env.start_pump();
    char* result = mg_params_request_all(env.core);
    env.stop_pump();

    CHECK(result != nullptr, "param request returned non-null");
    if (result) {
        fprintf(stderr, "    param request result: %s\n", result);
        mg_free_string(result);
    }
}

static void test_mission_retry_on_drop() {
    fprintf(stderr, "\n[Group] MISSION RETRY (first request dropped)\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    std::vector<mavlink_mission_item_int_t> items(3);
    memset(items.data(), 0, sizeof(mavlink_mission_item_int_t) * items.size());
    for (int i = 0; i < 3; i++) {
        items[i].seq = static_cast<uint16_t>(i);
        items[i].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
        items[i].command = MAV_CMD_NAV_WAYPOINT;
        items[i].autocontinue = 1;
        items[i].x = 399042000 + (i * 1000);
        items[i].y = 1164074000 + (i * 1000);
        items[i].z = 50.0f + (i * 25.0f);
        items[i].mission_type = MAV_MISSION_TYPE_MISSION;
    }
    env.mock->set_mission(items);
    env.mock->set_mission_first_attempt_drop(true);

    env.start_pump();
    char* mission_json = strdup("[{\"lat\":39.9042,\"lon\":116.4074,\"alt\":100},"
                                "{\"lat\":39.9043,\"lon\":116.4075,\"alt\":50},"
                                "{\"lat\":39.9044,\"lon\":116.4076,\"alt\":75}]");
    char* result = mg_mission_upload(env.core, mission_json);
    free(mission_json);
    env.stop_pump();

    CHECK(result != nullptr, "mission upload returned non-null");
    if (result) {
        fprintf(stderr, "    mission result: %s\n", result);
        mg_free_string(result);
    }
}

// 直接覆盖 mg_command_* 入口（审计 TEST-002）。别名路径已有测试，这里固定
// 命令编码本身，尤其是 guided_point 的经纬度单位。
static void test_safety_command_encoding() {
    fprintf(stderr, "\n[Group] SAFETY COMMAND ENCODING\n");

    {
        TestEnv env;
        CHECK(env.wait_connected(), "set_mode: connected after heartbeat");
        env.start_pump();
        char* result = mg_command_set_mode(env.core, 0, 4);
        env.stop_pump();
        CHECK(is_success(result), "set_mode success:true");
        if (result) mg_free_string(result);

        bool encoded = false;
        for (const auto& cmd : env.mock->received_cmds) {
            if (cmd.command == MAV_CMD_DO_SET_MODE &&
                cmd.p1 == static_cast<float>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) &&
                cmd.p2 == 4.0f) {
                encoded = true;
            }
        }
        CHECK(encoded, "DO_SET_MODE encodes custom-mode-enabled + custom_mode=4");
    }

    {
        TestEnv env;
        CHECK(env.wait_connected(), "rtl: connected after heartbeat");
        env.start_pump();
        char* result = mg_command_rtl(env.core);
        env.stop_pump();
        CHECK(is_success(result), "rtl success:true");
        if (result) mg_free_string(result);

        bool sent = false;
        for (const auto& cmd : env.mock->received_cmds) {
            if (cmd.command == MAV_CMD_NAV_RETURN_TO_LAUNCH) sent = true;
        }
        CHECK(sent, "rtl sends RETURN_TO_LAUNCH");
    }

    {
        TestEnv env;
        CHECK(env.wait_connected(), "land: connected after heartbeat");
        env.start_pump();
        char* result = mg_command_land(env.core);
        env.stop_pump();
        CHECK(is_success(result), "land success:true");
        if (result) mg_free_string(result);

        bool sent = false;
        for (const auto& cmd : env.mock->received_cmds) {
            if (cmd.command == MAV_CMD_NAV_LAND) sent = true;
        }
        CHECK(sent, "land sends NAV_LAND");
    }

    {
        TestEnv env;
        CHECK(env.wait_connected(), "guided point: connected after heartbeat");
        const double lat = 39.9042;
        const double lon = 116.4074;
        env.start_pump();
        char* result = mg_command_guided_point(
            env.core, lat, lon, 120.0f, -1.0f, -1.0f);
        env.stop_pump();
        CHECK(is_success(result), "guided point success:true");
        if (result) mg_free_string(result);

        // The mock records COMMAND_LONG param5/param6 as (int32_t)(value*1e7),
        // so a correctly encoded DO_REPOSITION lands on lat/lon degE7.
        const int32_t expect_lat = 399042000;
        const int32_t expect_lon = 1164074000;
        bool encoded = false;
        for (const auto& cmd : env.mock->received_cmds) {
            if (cmd.command != MAV_CMD_DO_REPOSITION) continue;
            const int32_t dlat = cmd.x - expect_lat;
            const int32_t dlon = cmd.y - expect_lon;
            if ((dlat < 0 ? -dlat : dlat) < 1000 &&
                (dlon < 0 ? -dlon : dlon) < 1000 &&
                cmd.z == 120) {
                encoded = true;
            }
        }
        CHECK(encoded,
              "DO_REPOSITION carries lat/lon in degrees for COMMAND_LONG");
    }
}

// 设置 home 点：参数编码对齐 QGC 的 doSetHome / MP 的 Set Home Here。
static void test_set_home_encoding() {
    fprintf(stderr, "\n[Group] SET HOME ENCODING\n");
    TestEnv env;
    CHECK(env.wait_connected(), "set_home: connected after heartbeat");
    env.mock->received_cmds.clear();

    env.start_pump();
    char* result = mg_command_set_home(env.core, 39.9042, 116.4074, 42.5f);
    env.stop_pump();
    CHECK(is_success(result), "set_home success:true");
    if (result) mg_free_string(result);

    bool encoded = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command != MAV_CMD_DO_SET_HOME) continue;
        const int32_t expected_lat = 399042000;
        const int32_t expected_lon = 1164074000;
        const int32_t dlat = cmd.x - expected_lat;
        const int32_t dlon = cmd.y - expected_lon;
        if (cmd.p1 == 0.0f && (dlat < 0 ? -dlat : dlat) < 1000 &&
            (dlon < 0 ? -dlon : dlon) < 1000 && cmd.z == 42) {
            encoded = true;
        }
    }
    CHECK(encoded, "DO_SET_HOME carries param1=0 plus lat/lon/alt in degrees");

    // 越界坐标必须被本地拒绝，不能把垃圾值发给飞控。
    env.mock->received_cmds.clear();
    char* bad_lat = mg_command_set_home(env.core, 120.0, 116.4, 10.0f);
    CHECK(is_failure(bad_lat), "out-of-range latitude is rejected locally");
    if (bad_lat) mg_free_string(bad_lat);
    char* bad_lon = mg_command_set_home(env.core, 39.9, 200.0, 10.0f);
    CHECK(is_failure(bad_lon), "out-of-range longitude is rejected locally");
    if (bad_lon) mg_free_string(bad_lon);
    CHECK(env.mock->received_cmds.empty(),
          "rejected set_home never reaches the flight controller");
}

static void test_flight_mode_state_setters() {
    fprintf(stderr, "\n[Group] FLIGHT MODE STATE SETTERS\n");
    TestEnv env;

    char* takeoff = mg_flight_set_takeoff_altitude(env.core, 30.0f);
    CHECK(is_success(takeoff), "set takeoff altitude accepted");
    if (takeoff) mg_free_string(takeoff);

    char* too_high = mg_flight_set_takeoff_altitude(env.core, 500.0f);
    CHECK(is_failure(too_high), "takeoff altitude out of range rejected");
    if (too_high) mg_free_string(too_high);

    char* readback = mg_flight_get_takeoff_altitude(env.core);
    CHECK(json_has(readback, "\"altitude_m\":30"), "takeoff altitude readback matches");
    if (readback) mg_free_string(readback);

    char* loiter = mg_flight_set_loiter_radius(env.core, 100.0f);
    CHECK(is_success(loiter), "set loiter radius accepted");
    if (loiter) mg_free_string(loiter);

    char* too_small = mg_flight_set_loiter_radius(env.core, 1.0f);
    CHECK(is_failure(too_small), "loiter radius out of range rejected");
    if (too_small) mg_free_string(too_small);

    char* radius = mg_flight_get_loiter_radius(env.core);
    CHECK(json_has(radius, "\"radius_m\":100"), "loiter radius readback matches");
    if (radius) mg_free_string(radius);
}

int main() {
    fprintf(stderr, "=== Command Simulation Tests ===\n");
    test_arm();
    test_takeoff();
    test_rtl();
    test_mode_change();
    test_safety_command_encoding();
    test_flight_mode_state_setters();
    test_set_home_encoding();
    test_mode_status_tracks_latest_heartbeat();
    test_command_nak();
    test_command_timeout();
    test_arm_no_retry_on_drop();
    test_safe_cmd_retry_on_drop();
    test_param_list_no_response();
    test_mission_retry_on_drop();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
