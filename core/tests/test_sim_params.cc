/**
 * test_sim_params.cc - Parameter protocol simulation tests
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(msg, cond) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

struct TestEnv : MockVehicleTestEnv {
    explicit TestEnv(MockFlightController::Config cfg = {})
        : MockVehicleTestEnv(cfg, true) {}
};

static int parse_progress_received(const char* json) {
    const char* p = strstr(json, "\"received\":");
    if (!p) return -1;
    return atoi(p + 11);
}

static int parse_progress_total(const char* json) {
    const char* p = strstr(json, "\"total\":");
    if (!p) return -1;
    return atoi(p + 8);
}

static void test_full_download() {
    fprintf(stderr, "\n[Group] Full parameter download\n");
    TestEnv env;

    env.tick(1100);

    char* res = mg_params_request_all(env.core);
    CHECK("request_all returns success", res != nullptr && strstr(res, "true") != nullptr);
    mg_free_string(res);

    int received = 0;
    int total = 0;
    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        received = parse_progress_received(prog);
        total = parse_progress_total(prog);
        mg_free_string(prog);
        if (total > 0 && received >= total) break;
    }

    CHECK("total > 0", total > 0);
    CHECK("total >= 25", total >= 25);
    CHECK("received == total (download complete)", total > 0 && received >= total);

    fprintf(stderr, "  (params: received=%d total=%d)\n", received, total);
}

static void test_param_get() {
    fprintf(stderr, "\n[Group] Parameter get by name\n");
    TestEnv env;

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    char* json = mg_param_get(env.core, "WPNAV_SPEED");
    CHECK("get WPNAV_SPEED not null", json != nullptr);
    CHECK("get WPNAV_SPEED found", strstr(json, "\"found\":true") != nullptr);

    const char* vp = strstr(json, "\"value\":");
    CHECK("value field present", vp != nullptr);
    if (vp) {
        const float val = static_cast<float>(atof(vp + 8));
        CHECK("WPNAV_SPEED == 5.0", val > 4.9f && val < 5.1f);
        fprintf(stderr, "  (WPNAV_SPEED = %g)\n", val);
    }
    mg_free_string(json);

    char* notfound = mg_param_get(env.core, "NONEXISTENT_PARAM");
    CHECK("not found param returns found:false", strstr(notfound, "\"found\":false") != nullptr);
    mg_free_string(notfound);
}

static void test_param_set() {
    fprintf(stderr, "\n[Group] Parameter set\n");
    TestEnv env;

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    env.mock->received_params_set.clear();

    char* set_res = mg_param_set(env.core, "WPNAV_SPEED", 10.0f);
    CHECK("set returns success", set_res != nullptr && strstr(set_res, "true") != nullptr);
    mg_free_string(set_res);

    env.tick(100);

    bool found = false;
    for (const auto& name : env.mock->received_params_set) {
        if (name == "WPNAV_SPEED") {
            found = true;
            break;
        }
    }
    CHECK("mock received WPNAV_SPEED set", found);

    char* json = mg_param_get(env.core, "WPNAV_SPEED");
    const char* vp = strstr(json, "\"value\":");
    if (vp) {
        const float val = static_cast<float>(atof(vp + 8));
        CHECK("WPNAV_SPEED updated to 10.0", val > 9.9f && val < 10.1f);
        fprintf(stderr, "  (WPNAV_SPEED = %g after set)\n", val);
    }
    mg_free_string(json);
}

static void test_config_uses_ardupilot_profile_metadata() {
    fprintf(stderr, "\n[Group] Config metadata profile\n");
    TestEnv env;

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    char* groups = mg_config_get_groups(env.core);
    CHECK("copter metadata exposes BATT group", strstr(groups, "\"name\":\"BATT\"") != nullptr);
    mg_free_string(groups);

    char* descriptor = mg_config_get_group_descriptor(env.core, "BATT");
    CHECK("BATT_MONITOR official display name",
          strstr(descriptor, "\"display_name\":\"电池监测器\"") != nullptr);
    CHECK("BATT_MONITOR bilingual enum option",
          strstr(descriptor, "模拟电压和电流") != nullptr &&
          strstr(descriptor, "Analog Voltage and Current") != nullptr);
    mg_free_string(descriptor);

    char* values = mg_config_read_group(env.core, "BATT");
    CHECK("BATT_MONITOR bilingual live display value",
          strstr(values, "模拟电压和电流") != nullptr &&
          strstr(values, "Analog Voltage and Current") != nullptr);
    mg_free_string(values);
}

static void test_geofence_get_uses_loaded_ardupilot_params() {
    fprintf(stderr, "\n[Group] Geofence config from parameters\n");
    TestEnv env;

    env.mock->add_param("FENCE_ENABLE", 1, 2);
    env.mock->add_param("FENCE_TYPE", 7, 2);
    env.mock->add_param("FENCE_ACTION", 1, 2);
    env.mock->add_param("FENCE_ALT_MAX", 100, 9);
    env.mock->add_param("FENCE_ALT_MIN", -10, 9);
    env.mock->add_param("FENCE_MARGIN", 2, 9);

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    char* fence = mg_geofence_get(env.core);
    CHECK("FENCE_ENABLE maps to enabled true", strstr(fence, "\"enabled\":true") != nullptr);
    CHECK("FENCE_ALT_MAX maps to max_altitude 100", strstr(fence, "\"max_altitude\":100") != nullptr);
    CHECK("FENCE_ALT_MIN maps to min_altitude -10", strstr(fence, "\"min_altitude\":-10") != nullptr);
    CHECK("FENCE_ACTION maps to action 1", strstr(fence, "\"action\":1") != nullptr);
    CHECK("FENCE_MARGIN maps to breach_distance 2", strstr(fence, "\"breach_distance\":2") != nullptr);
    mg_free_string(fence);
}

int main() {
    fprintf(stderr, "=== Parameter Simulation Tests ===\n");
    test_full_download();
    test_param_get();
    test_param_set();
    test_config_uses_ardupilot_profile_metadata();
    test_geofence_get_uses_loaded_ardupilot_params();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
