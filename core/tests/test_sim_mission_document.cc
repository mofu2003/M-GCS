#define MGCORE_EXPORTS

#include "ffi_api.h"
#include "mock_vehicle_test_env.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(condition, message) do { \
    if (condition) { \
        ++g_pass; \
        fprintf(stderr, "  [PASS] %s\n", message); \
    } else { \
        ++g_fail; \
        fprintf(stderr, "  [FAIL] %s\n", message); \
    } \
} while (0)

int main() {
    fprintf(stderr, "=== Mission Document Simulation Tests ===\n");
    MockVehicleTestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    char* result = mg_mission_doc_create(
        env.core,
        R"({"metadata":{"name":"Simulation Mission"},"home":{"set":true,"lat":39.9,"lon":116.4,"alt_m":20}})");
    CHECK(result && strstr(result, "\"success\":true"), "create document succeeds");
    if (result) mg_free_string(result);

    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"add","expected_revision":1,"entity":{"id":"takeoff","type":"takeoff","command":22,"alt_m":30}})");
    CHECK(result && strstr(result, "\"revision\":2"), "add takeoff increments revision");
    if (result) mg_free_string(result);

    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"add","expected_revision":2,"entity":{"id":"waypoint","type":"waypoint","command":16,"lat":39.9001,"lon":116.4001,"alt_m":50,"params":[0,2,0,0]}})");
    CHECK(result && strstr(result, "\"revision\":3"), "add waypoint increments revision");
    if (result) mg_free_string(result);

    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"add","expected_revision":3,"entity":{"id":"land","type":"land","command":21,"lat":39.9002,"lon":116.4002,"alt_m":0}})");
    CHECK(result && strstr(result, "\"revision\":4"), "add land increments revision");
    if (result) mg_free_string(result);

    result = mg_mission_doc_upload(env.core, 0);
    CHECK(
        result && strstr(result, "\"code\":-4"),
        "upload requires an explicit expected revision");
    if (result) mg_free_string(result);

    result = mg_mission_doc_upload(env.core, 4);
    CHECK(result && strstr(result, "\"success\":true"), "document upload starts");
    CHECK(result && strstr(result, "\"document_item_count\":3"), "upload reports visible document items");
    CHECK(result && strstr(result, "\"transport_item_count\":4"), "ArduPilot upload includes home item");
    if (result) mg_free_string(result);

    for (int i = 0; i < 80; ++i) env.tick(50);
    CHECK(env.mock->mission_items_received == 4, "mock vehicle received home plus mission items");

    result = mg_mission_doc_verify(env.core, 0.1);
    CHECK(result && strstr(result, "\"code\":-5"), "verify requires a completed readback");
    if (result) mg_free_string(result);

    result = mg_mission_download(env.core);
    CHECK(result && strstr(result, "\"success\":true"), "mission download starts");
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) env.tick(50);

    result = mg_mission_doc_verify(env.core, 0.1);
    CHECK(result && strstr(result, "\"matches\":true"), "vehicle mission matches compiled document");
    if (result) mg_free_string(result);

    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"update","expected_revision":4,"entity":{"id":"waypoint","type":"waypoint","command":16,"lat":39.9001,"lon":116.4001,"alt_m":65,"params":[0,2,0,0]}})");
    CHECK(result && strstr(result, "\"revision\":5"), "update waypoint increments revision");
    if (result) mg_free_string(result);

    // 从飞控读取任务：QGC 的 Download / MP 的 Read。读取前必须有完成的下载。
    MockVehicleTestEnv fresh;
    for (int i = 0; i < 15; ++i) fresh.tick(100);
    result = mg_mission_doc_create(fresh.core, R"({"metadata":{"name":"Import"}})");
    if (result) mg_free_string(result);

    result = mg_mission_doc_import_from_vehicle(fresh.core);
    CHECK(
        result && strstr(result, "\"code\":-5"),
        "import from vehicle requires a completed download");
    if (result) mg_free_string(result);

    std::vector<mavlink_mission_item_int_t> items(2);
    memset(items.data(), 0, sizeof(mavlink_mission_item_int_t) * items.size());
    for (int i = 0; i < 2; ++i) {
        items[i].seq = static_cast<uint16_t>(i);
        items[i].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
        items[i].command = (i == 0) ? MAV_CMD_NAV_TAKEOFF : MAV_CMD_NAV_WAYPOINT;
        items[i].autocontinue = 1;
        items[i].x = 399042000 + i * 1000;
        items[i].y = 1164074000 + i * 1000;
        items[i].z = 30.0f + i * 20.0f;
        items[i].mission_type = MAV_MISSION_TYPE_MISSION;
    }
    fresh.mock->set_mission(items);

    result = mg_mission_download(fresh.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) fresh.tick(50);

    result = mg_mission_doc_import_from_vehicle(fresh.core);
    CHECK(
        result && strstr(result, "\"success\":true"),
        "import from vehicle succeeds after a download");
    CHECK(result && strstr(result, "\"wp-1\""), "imported entities are named");
    CHECK(result && strstr(result, "\"takeoff\""), "imported command keeps its entity type");
    if (result) mg_free_string(result);

    result = mg_mission_doc_validate(fresh.core);
    CHECK(result && strstr(result, "\"valid\":true"), "imported document validates");
    if (result) mg_free_string(result);

    result = mg_mission_doc_verify(env.core, 0.1);
    CHECK(result && strstr(result, "\"matches\":false"), "verify detects local document drift");
    CHECK(result && strstr(result, "\"difference_count\":1"), "verify reports altitude difference");
    if (result) mg_free_string(result);

    // 主动取消：新下载尚未收到 MISSION_COUNT 时必须能终止，并保留可查询终态。
    MockVehicleTestEnv cancel_env;
    for (int i = 0; i < 15; ++i) cancel_env.tick(100);
    result = mg_mission_download(cancel_env.core);
    CHECK(result && strstr(result, "\"success\":true"), "cancel test download starts");
    if (result) mg_free_string(result);

    result = mg_operations_cancel(cancel_env.core, "mission");
    CHECK(result && strstr(result, "\"canceled\":true"), "active mission transfer can be cancelled");
    if (result) mg_free_string(result);

    result = mg_operations_get_snapshot(cancel_env.core);
    CHECK(result && strstr(result, "\"state\":\"idle\""), "cancelled transfer returns to idle");
    CHECK(result && strstr(result, "\"cancelled\":true"), "cancelled transfer keeps terminal state");
    CHECK(result && strstr(result, "mission transfer cancelled"), "cancelled transfer keeps reason");
    if (result) mg_free_string(result);

    result = mg_mission_download(cancel_env.core);
    CHECK(result && strstr(result, "\"success\":true"), "download can restart after cancellation");
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) cancel_env.tick(50);

    fprintf(
        stderr,
        "\n=== Results: %d passed, %d failed ===\n",
        g_pass,
        g_fail);
    return g_fail == 0 ? 0 : 1;
}
