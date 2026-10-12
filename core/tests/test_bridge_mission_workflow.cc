#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "ffi_mission_bridge.h"
#include "mock_vehicle_test_env.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


static void test_present_plan_hides_ardupilot_home_item() {
    fprintf(stderr, "\n[Group] mission presentation\n");
    MockVehicleTestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    // 飞控里的任务在 seq 0 放 Home（ArduPilot 约定），seq 1 是首个航点。
    std::vector<mavlink_mission_item_int_t> items(2);
    memset(items.data(), 0, items.size() * sizeof(mavlink_mission_item_int_t));
    items[0].seq = 0;
    items[0].frame = MAV_FRAME_GLOBAL;
    items[0].command = MAV_CMD_NAV_WAYPOINT;
    items[0].x = static_cast<int32_t>(39.9042 * 1e7);
    items[0].y = static_cast<int32_t>(116.4074 * 1e7);
    items[0].z = 0.0f;
    items[1].seq = 1;
    items[1].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    items[1].command = MAV_CMD_NAV_WAYPOINT;
    items[1].autocontinue = 1;
    items[1].x = static_cast<int32_t>(39.9 * 1e7);
    items[1].y = static_cast<int32_t>(116.4 * 1e7);
    items[1].z = 50.0f;
    env.mock->set_mission(items);

    char* started = mg_mission_download(env.core);
    if (started) mg_free_string(started);
    for (int i = 0; i < 80; i++) env.tick(50);

    mg::core::mission::cad::MissionHome home;
    bool home_present = false;
    auto plan = mg::bridge::ffi::present_mission_plan(
        env.core,
        env.mav->mission_get_plan(),
        &home,
        &home_present);
    CHECK(plan.items.size() == 1, "presented plan hides injected home item");
    CHECK(plan.items.front().seq == 0, "presented plan resequences first visible item to zero");
    CHECK(home_present, "presented plan reports the hidden ArduPilot home item");
    CHECK(std::fabs(home.latitude - 39.9042) < 1e-6,
          "presented plan exposes home latitude to the document layer");
    CHECK(std::fabs(home.longitude - 116.4074) < 1e-6,
          "presented plan exposes home longitude to the document layer");
    CHECK(mg::bridge::ffi::prepare_mission_current_index(env.core, 0) == 1,
          "client-visible mission index maps to transport index when home item is injected");

    mg::core::mission::cad::MissionHome planned_home;
    planned_home.set = true;
    planned_home.latitude = 47.1;
    planned_home.longitude = 8.2;
    planned_home.altitude_m = 123.0f;
    auto upload_plan = mg::bridge::ffi::prepare_mission_upload(
        env.core,
        plan,
        &planned_home);
    CHECK(upload_plan.items.size() == 2,
          "prepared upload contains planned home plus visible item");
    CHECK(upload_plan.items.front().x == 471000000,
          "prepared upload uses planned home latitude");
    CHECK(upload_plan.items.front().y == 82000000,
          "prepared upload uses planned home longitude");
    CHECK(std::fabs(upload_plan.items.front().z - 123.0f) < 0.01f,
          "prepared upload uses planned home altitude");
}

int main() {
    fprintf(stderr, "=== Bridge Mission Workflow Tests ===\n");
    test_present_plan_hides_ardupilot_home_item();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
