/**
 * test_sim_full_flow.cc - End-to-end simulation test
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


static bool json_has(const char* json, const char* key) {
    return json && strstr(json, key);
}

static cJSON* object_item(cJSON* root, const char* key) {
    return root ? cJSON_GetObjectItemCaseSensitive(root, key) : nullptr;
}

struct SimEnv : MockVehicleTestEnv {
    SimEnv() : MockVehicleTestEnv({}, true) {}

    char* call_arm_with_pump() {
        start_pump();
        char* result = mg_command_arm(core, 0);
        stop_pump();
        return result;
    }

    char* call_takeoff_with_pump(float altitude_m) {
        start_pump();
        char* result = mg_command_takeoff(core, altitude_m);
        stop_pump();
        return result;
    }

    char* call_disarm_with_pump() {
        start_pump();
        char* result = mg_command_disarm(core, 0);
        stop_pump();
        return result;
    }
};

int main() {
    fprintf(stderr, "=== Full Flight Simulation Test ===\n");

    SimEnv env;

    fprintf(stderr, "\n[Step 1] Initial state\n");
    CHECK(env.core != nullptr, "core created");
    char* state = mg_connection_get_state(env.core);
    CHECK(state && strstr(state, "false"), "initially disconnected");
    mg_free_string(state);

    fprintf(stderr, "\n[Step 2] Heartbeat & vehicle detection\n");
    env.tick(1500);
    state = mg_connection_get_state(env.core);
    CHECK(state != nullptr, "heartbeat drives state");
    mg_free_string(state);

    char* diag = mg_connection_get_diagnostics(env.core);
    CHECK(diag && strstr(diag, "\"alive\":true"), "diagnostics sees active heartbeat");
    CHECK(diag && strstr(diag, "\"connected\":true"), "diagnostics has connected state");
    mg_free_string(diag);

    fprintf(stderr, "\n[Step 3] Parameter download\n");
    char* pr = mg_params_request_all(env.core);
    CHECK(json_has(pr, "success"), "request_all returns ok");
    mg_free_string(pr);

    for (int i = 0; i < 50; i++) {
        env.tick(100);
        char* prog = mg_params_get_progress(env.core);
        if (prog && strstr(prog, "\"received\":") && strstr(prog, "\"total\":")) {
            const char* r = strstr(prog, "\"received\":");
            const char* t = strstr(prog, "\"total\":");
            if (r && t) {
                const int recv = atoi(r + 11);
                const int total = atoi(t + 8);
                if (recv > 0 && recv == total) {
                    mg_free_string(prog);
                    break;
                }
            }
        }
        mg_free_string(prog);
    }
    char* prog = mg_params_get_progress(env.core);
    CHECK(prog && strstr(prog, "\"received\":"), "params fully downloaded");
    mg_free_string(prog);

    char* ops = mg_operations_get_snapshot(env.core);
    CHECK(ops && strstr(ops, "\"id\":\"params\""), "operations reports params operation");
    CHECK(ops && strstr(ops, "\"state\":\"complete\""), "operations reports complete params");
    mg_free_string(ops);

    char* pv = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    CHECK(pv && strstr(pv, "5"), "WPNAV_SPEED exists");
    mg_free_string(pv);

    fprintf(stderr, "\n[Step 4] Mission upload\n");
    const char* mission_json = R"([
        {"lat":39.9,"lon":116.4,"alt":50,"command":16,"frame":3},
        {"lat":39.91,"lon":116.41,"alt":60,"command":16,"frame":3},
        {"lat":39.92,"lon":116.42,"alt":70,"command":16,"frame":3}
    ])";
    char* mr = mg_mission_upload(env.core, mission_json);
    CHECK(mr != nullptr, "mission upload started");
    mg_free_string(mr);

    for (int i = 0; i < 50; i++) env.tick(200);
    CHECK(env.mock->mission_items_received >= 3, "3 items received by mock");

    fprintf(stderr, "\n[Step 5] Arm\n");
    env.tick(500);
    char* arm_r = env.call_arm_with_pump();
    CHECK(arm_r != nullptr, "arm command issued");
    CHECK(arm_r && strstr(arm_r, "success"), "arm accepted");
    CHECK(env.mock->received_cmds.size() >= 1, "mock got ARM command");
    mg_free_string(arm_r);

    fprintf(stderr, "\n[Step 6] Takeoff\n");
    env.tick(100);
    char* tk_r = env.call_takeoff_with_pump(10.0f);
    CHECK(tk_r && strstr(tk_r, "success"), "takeoff succeeds");
    mg_free_string(tk_r);

    fprintf(stderr, "\n[Step 7] Flight telemetry\n");
    env.mock->telem.lat_deg7 = 399100000;
    env.mock->telem.lon_deg7 = 1164100000;
    env.mock->telem.alt_mm = 10000;
    env.mock->telem.groundspeed = 5.0f;
    env.mock->telem.battery_remaining = 85;
    env.mock->telem.roll = 0.25f;
    env.mock->telem.pitch = -0.1f;
    env.mock->telem.yaw = 1.57f;
    env.tick(500);
    char* att = mg_vehicle_get_attitude(env.core);
    CHECK(att != nullptr && strstr(att, "success"), "telemetry updates during flight");
    mg_free_string(att);

    char* dash = mg_telemetry_get_dashboard_snapshot(env.core);
    CHECK(dash && strstr(dash, "\"connection\""), "dashboard snapshot has connection");
    CHECK(dash && strstr(dash, "\"telemetry\""), "dashboard snapshot has telemetry");
    CHECK(dash && strstr(dash, "\"freshness\""), "dashboard snapshot has freshness");
    cJSON* dash_root = dash ? mg::core::json::parse(dash) : nullptr;
    cJSON* dash_telemetry = object_item(dash_root, "telemetry");
    cJSON* dash_attitude = object_item(dash_telemetry, "attitude");
    cJSON* dash_roll_deg = object_item(dash_attitude, "roll_deg");
    cJSON* dash_pitch_deg = object_item(dash_attitude, "pitch_deg");
    CHECK(cJSON_IsNumber(dash_roll_deg), "dashboard attitude exposes roll_deg");
    CHECK(cJSON_IsNumber(dash_pitch_deg), "dashboard attitude exposes pitch_deg");
    CHECK(cJSON_IsNumber(dash_roll_deg) &&
            std::fabs(dash_roll_deg->valuedouble - (0.25 * 180.0 / 3.141592653589793)) < 0.2, "dashboard roll uses degrees");
    CHECK(cJSON_IsNumber(dash_pitch_deg) &&
            std::fabs(dash_pitch_deg->valuedouble - (-0.1 * 180.0 / 3.141592653589793)) < 0.2, "dashboard pitch uses degrees");
    if (dash_root) cJSON_Delete(dash_root);
    mg_free_string(dash);

    char* sys = mg_system_get_diagnostics(env.core);
    CHECK(sys && strstr(sys, "\"operations\""), "system diagnostics has operations");
    CHECK(sys && strstr(sys, "\"last_error\""), "system diagnostics has last_error");
    mg_free_string(sys);

    fprintf(stderr, "\n[Step 8] RTL\n");
    char* rtl_r = env.call_with_pump(mg_command_rtl);
    CHECK(rtl_r && strstr(rtl_r, "success"), "RTL succeeds");
    mg_free_string(rtl_r);

    fprintf(stderr, "\n[Step 9] Disarm\n");
    char* disarm_r = env.call_disarm_with_pump();
    CHECK(disarm_r && strstr(disarm_r, "success"), "disarm succeeds");
    mg_free_string(disarm_r);

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
