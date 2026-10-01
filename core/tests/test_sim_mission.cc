/**
 * test_sim_mission.cc - Mission protocol simulation tests
 *
 * Uses MockFlightController and the FFI API to test mission upload/download.
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

using TestEnv = MockVehicleTestEnv;

static void test_mission_upload() {
    fprintf(stderr, "\n[Group] Mission upload\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    const char* json_wps =
        "[{\"lat\":39.9,\"lon\":116.4,\"alt\":50,\"command\":16,\"frame\":3},"
        "{\"lat\":39.91,\"lon\":116.41,\"alt\":60,\"command\":16,\"frame\":3},"
        "{\"lat\":39.92,\"lon\":116.42,\"alt\":70,\"command\":16,\"frame\":3}]";

    char* result = mg_mission_upload(env.core, json_wps);
    if (result) mg_free_string(result);

    for (int i = 0; i < 50; i++) env.tick(50);

    CHECK(env.mock->mission_items_received == 4, "ArduPilot upload includes injected home + 3 mission items");

    const auto& home = env.mock->stored_mission().front();
    CHECK(home.seq == 0 &&
          home.command == MAV_CMD_NAV_WAYPOINT &&
          home.frame == MAV_FRAME_GLOBAL,
          "injected item 0 is ArduPilot home waypoint");

    char* plan = mg_mission_get_plan(env.core);
    const bool presented_count_is_three = plan && strstr(plan, "\"count\":3");
    if (plan) mg_free_string(plan);
    CHECK(presented_count_is_three, "presented mission plan hides injected home item");
}

static void test_mission_upload_compat_request() {
    fprintf(stderr, "\n[Group] Mission upload with compat request\n");
    TestEnv env;
    env.mock->set_mission_use_compat_request(true);

    for (int i = 0; i < 15; i++) env.tick(100);

    const char* json_wps =
        "[{\"lat\":39.9,\"lon\":116.4,\"alt\":50,\"command\":16,\"frame\":3}]";

    char* result = mg_mission_upload(env.core, json_wps);
    if (result) mg_free_string(result);

    for (int i = 0; i < 50; i++) env.tick(50);

    char* progress = mg_mission_get_progress(env.core);
    const bool sent_one = progress && strstr(progress, "\"sent\":1");
    if (progress) mg_free_string(progress);

    CHECK(env.mock->mission_items_received == 2, "compat request upload includes injected home + 1 mission item");
    CHECK(sent_one, "compat mission upload updates sent progress");
}

static void test_mission_download() {
    fprintf(stderr, "\n[Group] Mission download\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    std::vector<mavlink_mission_item_int_t> items(3);
    for (int i = 0; i < 3; i++) {
        memset(&items[i], 0, sizeof(mavlink_mission_item_int_t));
        items[i].seq = i;
        items[i].command = MAV_CMD_NAV_WAYPOINT;
        items[i].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
        items[i].x = static_cast<int32_t>((39.9 + i * 0.01) * 1e7);
        items[i].y = static_cast<int32_t>((116.4 + i * 0.01) * 1e7);
        items[i].z = 50.0f + i * 10.0f;
    }
    env.mock->set_mission(items);

    char* result = mg_mission_download(env.core);
    if (result) mg_free_string(result);

    bool progress_done = false;
    for (int i = 0; i < 80; i++) {
        env.tick(50);
        char* prog = mg_mission_get_progress(env.core);
        if (prog) {
            if (strstr(prog, "\"complete\":true") || strstr(prog, "\"done\":true")) {
                progress_done = true;
                mg_free_string(prog);
                break;
            }
            mg_free_string(prog);
        }
    }

    char* plan = mg_mission_get_plan(env.core);
    const bool has_plan = (plan != nullptr);
    if (plan) mg_free_string(plan);

    CHECK(progress_done || has_plan, "download completed or plan retrieved");
}

static void test_mission_download_compat_item() {
    fprintf(stderr, "\n[Group] Mission download with compat item\n");
    TestEnv env;
    env.mock->set_mission_download_use_compat_item(true);

    for (int i = 0; i < 15; i++) env.tick(100);

    std::vector<mavlink_mission_item_int_t> items(1);
    memset(&items[0], 0, sizeof(mavlink_mission_item_int_t));
    items[0].seq = 0;
    items[0].command = MAV_CMD_NAV_WAYPOINT;
    items[0].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    items[0].x = static_cast<int32_t>(39.9 * 1e7);
    items[0].y = static_cast<int32_t>(116.4 * 1e7);
    items[0].z = 50.0f;
    env.mock->set_mission(items);

    char* result = mg_mission_download(env.core);
    if (result) mg_free_string(result);

    for (int i = 0; i < 80; i++) {
        env.tick(50);
    }

    char* plan = mg_mission_get_plan(env.core);
    const bool has_lat = plan && strstr(plan, "\"lat\":39.9");
    const bool has_lon = plan && strstr(plan, "\"lon\":116.4");
    if (plan) mg_free_string(plan);

    CHECK(has_lat && has_lon, "compat mission item download preserves coordinates");
}

static void test_mission_clear() {
    fprintf(stderr, "\n[Group] Mission clear\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_mission_clear(env.core);
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    CHECK(env.mock->mission_clear_received, "mission clear received");
}

static void test_mission_execution_progress() {
    fprintf(stderr, "\n[Group] Mission execution progress\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    std::vector<mavlink_mission_item_int_t> items(4);
    for (int i = 0; i < 4; i++) {
        memset(&items[i], 0, sizeof(mavlink_mission_item_int_t));
        items[i].seq = static_cast<uint16_t>(i);
        items[i].command = MAV_CMD_NAV_WAYPOINT;
        items[i].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
        items[i].x = static_cast<int32_t>((39.9 + i * 0.01) * 1e7);
        items[i].y = static_cast<int32_t>((116.4 + i * 0.01) * 1e7);
        items[i].z = 50.0f + i * 10.0f;
    }
    env.mock->set_mission(items);

    char* result = mg_mission_download(env.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; i++) env.tick(50);

    char* progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"valid\":true"),
          "execution progress reports a cached mission");
    CHECK(progress && strstr(progress, "\"reached_count\":0") &&
              strstr(progress, "\"finished\":false"),
          "execution progress starts with nothing reached");
    if (progress) mg_free_string(progress);

    // 飞控报告当前目标为第 1 项，并已到达。
    env.mock->send_mission_current(1);
    env.mock->send_mission_item_reached(1);
    for (int i = 0; i < 10; i++) env.tick(50);

    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"reached_count\":1") &&
              strstr(progress, "\"last_reached_seq\":1") &&
              strstr(progress, "\"finished\":false"),
          "reached count tracks MISSION_ITEM_REACHED");
    if (progress) mg_free_string(progress);

    // 重发同一个 seq 不应重复计数。
    env.mock->send_mission_item_reached(1);
    for (int i = 0; i < 10; i++) env.tick(50);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"reached_count\":1"),
          "duplicate MISSION_ITEM_REACHED is not double counted");
    if (progress) mg_free_string(progress);

    // 到达原始 seq 的最后一项（含 home 偏移）即视为任务完成。
    env.mock->send_mission_item_reached(static_cast<uint16_t>(items.size() - 1));
    for (int i = 0; i < 10; i++) env.tick(50);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"finished\":true"),
          "reaching the last mission item reports finished");
    if (progress) mg_free_string(progress);
}

int main() {
    fprintf(stderr, "=== Mission Simulation Tests ===\n");
    test_mission_upload();
    test_mission_upload_compat_request();
    test_mission_download();
    test_mission_download_compat_item();
    test_mission_clear();
    test_mission_execution_progress();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
