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
#include "core/util/json.h"
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


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
    cJSON* root = mg::core::json::parse(json);
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
    char* result = mg_command_arm(env.core, 0);
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
    char* result = mg_command_takeoff(env.core, 10.0f);
    env.stop_pump();

    CHECK(result != nullptr, "takeoff returned non-null");
    CHECK(is_success(result), "takeoff success:true");
    if (result) {
        fprintf(stderr, "    takeoff result: %s\n", result);
        mg_free_string(result);
    }

    bool found_takeoff = false;
    bool ap_relative_alt = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == MAV_CMD_NAV_TAKEOFF) {
            found_takeoff = true;
            // ArduPilot 的 NAV_TAKEOFF param7 是相对 Home 的高度，不做 AMSL 换算。
            if (cmd.z == 10) ap_relative_alt = true;
            break;
        }
    }
    CHECK(found_takeoff, "mock received TAKEOFF cmd");
    CHECK(ap_relative_alt, "ArduPilot TAKEOFF keeps param7 as the requested relative altitude");
}


static void test_rtl() {
    fprintf(stderr, "\n[Group] RTL command\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    env.start_pump();
    char* result = mg_command_rtl(env.core);
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

/// 飞行模式操作台：模式表按机型给，槽位参数带模式名选项。
static void test_flight_mode_catalog() {
    fprintf(stderr, "\n[Group] Flight mode catalog\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected before reading the mode catalog");

    char* catalog = mg_flight_get_modes(env.core);
    CHECK(is_success(catalog), "mode catalog returns success");
    CHECK(json_has(catalog, "\"available\":true"), "mode catalog is available on a connected vehicle");
    CHECK(json_has(catalog, "\"name\":\"Stabilize\""), "copter catalog contains Stabilize");
    CHECK(json_has(catalog, "\"name\":\"Loiter\""), "copter catalog contains Loiter");
    CHECK(json_has(catalog, "\"current_mode\":\"Stabilize\""), "catalog reports the heartbeat mode");
    if (catalog) mg_free_string(catalog);

    // 模式档位参数要带上模式名选项，参数控件才会渲染成名称下拉（MP 的做法）。
    char* slot = mg_config_read_param(env.core, "FLTMODE1");
    CHECK(json_has(slot, "\"options_from_flight_modes\":true"),
          "mode slot parameter declares flight-mode sourced options");
    CHECK(json_has(slot, "\"label\":\"Loiter\""),
          "mode slot options carry readable mode names");
    if (slot) mg_free_string(slot);
}

/// S5：模式状态必须附带心跳新鲜度；过期时调用方不能把它当成当前值。
static void test_mode_state_reports_freshness() {
    fprintf(stderr, "\n[Group] MODE STATE freshness\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected before reading mode state");

    char* status = mg_flight_get_modes(env.core);
    CHECK(json_has(status, "\"heartbeat_fresh\":true"),
          "mode status is fresh while heartbeats keep arriving");
    if (status) mg_free_string(status);

    char* catalog = mg_flight_get_modes(env.core);
    CHECK(json_has(catalog, "\"heartbeat_fresh\":true"),
          "mode catalog reports heartbeat freshness");
    if (catalog) mg_free_string(catalog);

    char* summary = mg_vehicle_get_summary(env.core);
    CHECK(json_has(summary, "\"heartbeat_fresh\":true"),
          "vehicle summary reports heartbeat freshness");
    if (summary) mg_free_string(summary);

    // 从未连上的核心：状态是未知，而不是“未解锁”。
    mg_core_t* bare = mg_create();
    char* unknown = mg_flight_get_modes(bare);
    CHECK(json_has(unknown, "\"heartbeat_fresh\":false") &&
              json_has(unknown, "\"heartbeat_age_ms\":-1"),
          "an unconnected core reports mode state as unknown, not disarmed");
    if (unknown) mg_free_string(unknown);

    char* bare_summary = mg_vehicle_get_summary(bare);
    CHECK(json_has(bare_summary, "\"heartbeat_fresh\":false") &&
              json_has(bare_summary, "\"heartbeat_age_ms\":-1"),
          "an unconnected core reports summary state as unknown");
    if (bare_summary) mg_free_string(bare_summary);
    mg_destroy(bare);
}

static void test_mode_status_tracks_latest_heartbeat() {
    fprintf(stderr, "\n[Group] MODE STATUS follows latest heartbeat\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    char* initial = mg_flight_get_modes(env.core);
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

    char* updated = mg_flight_get_modes(env.core);
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
    char* result = mg_command_arm(env.core, 0);
    env.stop_pump();

    CHECK(result != nullptr, "arm nak returned non-null");
    CHECK(is_failure(result), "arm rejection is a failure");
    CHECK(json_has(result, "\"result\":1"), "arm rejection preserves TEMPORARILY_REJECTED");
    CHECK(json_has(result, "\"result_str\":\"temporarily_rejected\""),
          "arm rejection preserves the reason");
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
    char* result = mg_command_arm(env.core, 0);
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
    char* result = mg_command_arm(env.core, 0);
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

static void test_queued_send_failure_invokes_callback() {
    fprintf(stderr, "\n[Group] QUEUED COMMAND SEND FAILURE CALLBACK\n");
    TestEnv env;
    CHECK(env.wait_connected(), "connected after heartbeat");

    int called = 0;
    uint8_t result = 0;
    std::string text;
    env.mav->set_send_callback([](const uint8_t*, size_t) { return false; });
    auto ticket = env.mav->send_command_queued(
        1, 0, MAV_CMD_REQUEST_MESSAGE, 0, 0, 0, 0, 0, 0, 0,
        [&called, &result, &text, &env](const mg::core::mavlink::CommandAckInfo& ack) {
            ++called;
            result = ack.result;
            text = ack.result_text;
            // Re-entry must be safe: failure callbacks execute outside the lock.
            env.mav->latest_heartbeat();
        });
    CHECK(called == 1, "send failure callback is delivered without a later tick");
    auto ack = env.mav->wait_for_command(ticket, 1);
    CHECK(ack && ack->result_text == "command send failed",
          "send failure is distinguishable from ACK timeout for waiting callers");
    env.mav->tick();

    CHECK(called == 1, "send failure callback is delivered exactly once");
    CHECK(result == MAV_RESULT_FAILED, "send failure callback reports FAILED");
    CHECK(text == "command send failed", "send failure callback explains failure");

    env.mav->send_command_int_queued(
        1, 1, MAV_CMD_DO_REPOSITION, MAV_FRAME_GLOBAL,
        0, 0, 0, 0, 0, 0, 10,
        [&called, &env](const mg::core::mavlink::CommandAckInfo& info) {
            if (info.result == MAV_RESULT_FAILED && info.result_text == "command send failed") {
                ++called;
            }
            env.mav->latest_heartbeat();
        });
    CHECK(called == 2, "COMMAND_INT failure also delivers a reentrant callback immediately");
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

        // ArduPilot rejects DO_REPOSITION over COMMAND_LONG because that
        // message has no frame field. The GCS must use COMMAND_INT.
        const int32_t expect_lat = 399042000;
        const int32_t expect_lon = 1164074000;
        bool encoded = false;
        for (const auto& cmd : env.mock->received_cmds) {
            if (cmd.command != MAV_CMD_DO_REPOSITION) continue;
            const int32_t dlat = cmd.x - expect_lat;
            const int32_t dlon = cmd.y - expect_lon;
            if ((dlat < 0 ? -dlat : dlat) < 1000 &&
                (dlon < 0 ? -dlon : dlon) < 1000 &&
                cmd.z == 120 &&
                cmd.frame == MAV_FRAME_GLOBAL) {
                encoded = true;
            }
        }
        CHECK(encoded,
              "DO_REPOSITION uses COMMAND_INT with MAV_FRAME_GLOBAL");
    }
}

// S2：EKF 全局原点与起落架都必须以 COMMAND_ACK 为准。
static void test_global_origin_wait_for_ack() {
    fprintf(stderr, "\n[Group] GLOBAL ORIGIN / LANDING GEAR ACK\n");

    {
        TestEnv env;
        CHECK(env.wait_connected(), "origin: connected after heartbeat");
        env.mock->received_cmds.clear();
        env.start_pump();
        char* result = mg_command_set_global_origin(env.core, 39.9042, 116.4074, 50.0f);
        env.stop_pump();
        CHECK(is_success(result),
              "set_global_origin reports success only after the flight controller ACKs");
        if (result) mg_free_string(result);

        // QGC 发 COMMAND_INT + MAV_FRAME_GLOBAL，经纬度用 degE7。
        bool encoded = false;
        for (const auto& cmd : env.mock->received_cmds) {
            if (cmd.command != 48) continue;
            const int32_t dlat = cmd.x - 399042000;
            const int32_t dlon = cmd.y - 1164074000;
            if ((dlat < 0 ? -dlat : dlat) < 1000 &&
                (dlon < 0 ? -dlon : dlon) < 1000 &&
                cmd.z == 50 && cmd.frame == MAV_FRAME_GLOBAL) {
                encoded = true;
            }
        }
        CHECK(encoded, "set_global_origin sends COMMAND_INT 48 with degE7 lat/lon");
    }

    {
        TestEnv env;
        CHECK(env.wait_connected(), "origin rejection: connected after heartbeat");
        env.mock->set_next_cmd_result(MAV_RESULT_DENIED);
        env.start_pump();
        char* result = mg_command_set_global_origin(env.core, 39.9042, 116.4074, 50.0f);
        env.stop_pump();
        CHECK(is_failure(result),
              "a rejected set_global_origin is reported as a failure, not a success");
        if (result) mg_free_string(result);
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

int main() {
    fprintf(stderr, "=== Command Simulation Tests ===\n");
    test_arm();
    test_takeoff();
    test_rtl();
    test_safety_command_encoding();
    test_global_origin_wait_for_ack();
    test_set_home_encoding();
    test_mode_state_reports_freshness();
    test_mode_status_tracks_latest_heartbeat();
    test_flight_mode_catalog();
    test_command_nak();
    test_command_timeout();
    test_arm_no_retry_on_drop();
    test_safe_cmd_retry_on_drop();
    test_queued_send_failure_invokes_callback();
    test_param_list_no_response();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
