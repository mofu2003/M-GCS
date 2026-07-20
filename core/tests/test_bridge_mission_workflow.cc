#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "ffi_mission_bridge.h"
#include "mock_vehicle_test_env.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static void test_present_plan_hides_ardupilot_home_item() {
    fprintf(stderr, "\n[Group] mission presentation\n");
    MockVehicleTestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    const char* json_wps =
        "[{\"lat\":39.9,\"lon\":116.4,\"alt\":50,\"command\":16,\"frame\":3}]";
    char* result = mg_mission_upload(env.core, json_wps);
    if (result) mg_free_string(result);

    for (int i = 0; i < 50; i++) env.tick(50);

    auto plan = mg::bridge::ffi::present_mission_plan(env.core, env.mav->mission_get_plan());
    CHECK(plan.items.size() == 1, "presented plan hides injected home item");
    CHECK(plan.items.front().seq == 0, "presented plan resequences first visible item to zero");
    CHECK(mg::bridge::ffi::prepare_mission_current_index(env.core, 0) == 1,
          "client-visible mission index maps to transport index when home item is injected");
}

int main() {
    fprintf(stderr, "=== Bridge Mission Workflow Tests ===\n");
    test_present_plan_hides_ardupilot_home_item();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
