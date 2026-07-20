/**
 * test_sim_full_flow.cc - End-to-end simulation test
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(msg, cond) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static bool json_has(const char* json, const char* key) {
    return json && strstr(json, key);
}

static cJSON* object_item(cJSON* root, const char* key) {
    return root ? cJSON_GetObjectItemCaseSensitive(root, key) : nullptr;
}

struct SimEnv : MockVehicleTestEnv {
    SimEnv() : MockVehicleTestEnv({}, true) {}

    char* call_takeoff_with_pump(float altitude_m) {
        start_pump();
        char* result = mg_cmd_takeoff(core, altitude_m);
        stop_pump();
        return result;
    }
};

int main() {
    fprintf(stderr, "=== Full Flight Simulation Test ===\n");

    SimEnv env;

    fprintf(stderr, "\n[Step 1] Initial state\n");
    CHECK("core created", env.core != nullptr);
    char* state = mg_connection_get_state(env.core);
    CHECK("initially disconnected", state && strstr(state, "false"));
    mg_free_string(state);

    fprintf(stderr, "\n[Step 2] Heartbeat & vehicle detection\n");
    env.tick(1500);
    state = mg_connection_get_state(env.core);
    CHECK("heartbeat drives state", state != nullptr);
    mg_free_string(state);

    char* diag = mg_connection_get_diagnostics(env.core);
    CHECK("diagnostics sees active heartbeat", diag && strstr(diag, "\"alive\":true"));
    CHECK("diagnostics has connected state", diag && strstr(diag, "\"connected\":true"));
    mg_free_string(diag);

    fprintf(stderr, "\n[Step 3] Parameter download\n");
    char* pr = mg_params_request_all(env.core);
    CHECK("request_all returns ok", json_has(pr, "success"));
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
    CHECK("params fully downloaded", prog && strstr(prog, "\"received\":"));
    mg_free_string(prog);

    char* ops = mg_operations_get_snapshot(env.core);
    CHECK("operations reports params operation", ops && strstr(ops, "\"id\":\"params\""));
    CHECK("operations reports complete params", ops && strstr(ops, "\"state\":\"complete\""));
    mg_free_string(ops);

    char* pv = mg_param_get(env.core, "WPNAV_SPEED");
    CHECK("WPNAV_SPEED exists", pv && strstr(pv, "5"));
    mg_free_string(pv);

    fprintf(stderr, "\n[Step 4] Mission upload\n");
    const char* mission_json = R"([
        {"lat":39.9,"lon":116.4,"alt":50,"command":16,"frame":3},
        {"lat":39.91,"lon":116.41,"alt":60,"command":16,"frame":3},
        {"lat":39.92,"lon":116.42,"alt":70,"command":16,"frame":3}
    ])";
    char* mr = mg_mission_upload(env.core, mission_json);
    CHECK("mission upload started", mr != nullptr);
    mg_free_string(mr);

    for (int i = 0; i < 50; i++) env.tick(200);
    CHECK("3 items received by mock", env.mock->mission_items_received >= 3);

    fprintf(stderr, "\n[Step 5] Arm\n");
    env.tick(500);
    char* arm_r = env.call_with_pump(mg_cmd_arm);
    CHECK("arm command issued", arm_r != nullptr);
    CHECK("arm accepted", arm_r && strstr(arm_r, "success"));
    CHECK("mock got ARM command", env.mock->received_cmds.size() >= 1);
    mg_free_string(arm_r);

    fprintf(stderr, "\n[Step 6] Takeoff\n");
    env.tick(100);
    char* tk_r = env.call_takeoff_with_pump(10.0f);
    CHECK("takeoff succeeds", tk_r && strstr(tk_r, "success"));
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
    CHECK("telemetry updates during flight", att != nullptr && strstr(att, "success"));
    mg_free_string(att);

    char* dash = mg_telemetry_get_dashboard_snapshot(env.core);
    CHECK("dashboard snapshot has connection", dash && strstr(dash, "\"connection\""));
    CHECK("dashboard snapshot has telemetry", dash && strstr(dash, "\"telemetry\""));
    CHECK("dashboard snapshot has freshness", dash && strstr(dash, "\"freshness\""));
    cJSON* dash_root = dash ? cJSON_Parse(dash) : nullptr;
    cJSON* dash_telemetry = object_item(dash_root, "telemetry");
    cJSON* dash_attitude = object_item(dash_telemetry, "attitude");
    cJSON* dash_roll_deg = object_item(dash_attitude, "roll_deg");
    cJSON* dash_pitch_deg = object_item(dash_attitude, "pitch_deg");
    CHECK("dashboard attitude exposes roll_deg", cJSON_IsNumber(dash_roll_deg));
    CHECK("dashboard attitude exposes pitch_deg", cJSON_IsNumber(dash_pitch_deg));
    CHECK(
        "dashboard roll uses degrees",
        cJSON_IsNumber(dash_roll_deg) &&
            std::fabs(dash_roll_deg->valuedouble - (0.25 * 180.0 / 3.141592653589793)) < 0.2);
    CHECK(
        "dashboard pitch uses degrees",
        cJSON_IsNumber(dash_pitch_deg) &&
            std::fabs(dash_pitch_deg->valuedouble - (-0.1 * 180.0 / 3.141592653589793)) < 0.2);
    if (dash_root) cJSON_Delete(dash_root);
    mg_free_string(dash);

    char* sys = mg_system_get_diagnostics(env.core);
    CHECK("system diagnostics has operations", sys && strstr(sys, "\"operations\""));
    CHECK("system diagnostics has last_error", sys && strstr(sys, "\"last_error\""));
    mg_free_string(sys);

    fprintf(stderr, "\n[Step 8] RTL\n");
    char* rtl_r = env.call_with_pump(mg_cmd_rtl);
    CHECK("RTL succeeds", rtl_r && strstr(rtl_r, "success"));
    mg_free_string(rtl_r);

    fprintf(stderr, "\n[Step 9] Disarm\n");
    char* disarm_r = env.call_with_pump(mg_cmd_disarm);
    CHECK("disarm succeeds", disarm_r && strstr(disarm_r, "success"));
    mg_free_string(disarm_r);

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
