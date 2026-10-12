#define MGCORE_EXPORTS

#include "ffi_api.h"
#include "mock_vehicle_test_env.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


/// ArduPilot 把任务位置存成 float 度，往返会产生约 0.1–0.5 m 的量化误差。
/// 核对必须用米制容差比较经纬度，而不是整数精确比较，否则每次上传都报差异。
static void test_verify_tolerates_position_quantization() {
    fprintf(stderr, "\n[Group] Verify tolerance vs ArduPilot float storage\n");
    MockVehicleTestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    char* result = mg_mission_doc_create(
        env.core, R"({"metadata":{"name":"tolerance"}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"add","expected_revision":1,"entity":{"type":"waypoint","command":16,"lat":31.227,"lon":121.49,"alt_m":50,"params":[0,2,0,0]}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_upload(env.core, 2);
    if (result) mg_free_string(result);
    for (int i = 0; i < 60; ++i) env.tick(50);

    // 模拟飞控侧用 float 存经纬度：31.227 -> 312269984（degE7）。
    auto stored = env.mock->stored_mission();
    for (auto& item : stored) {
        if (item.x == 0 && item.y == 0) continue;
        item.x = static_cast<int32_t>(
            std::lround(static_cast<float>(item.x / 1e7) * 1e7));
        item.y = static_cast<int32_t>(
            std::lround(static_cast<float>(item.y / 1e7) * 1e7));
    }
    env.mock->set_mission(stored);

    result = mg_mission_download(env.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 60; ++i) env.tick(50);

    result = mg_mission_doc_verify(env.core, 1.0);
    CHECK(result && strstr(result, "\"matches\":true"),
          "1 m tolerance ignores ArduPilot position quantization");
    if (result) mg_free_string(result);

    result = mg_mission_doc_verify(env.core, 0.05);
    CHECK(result && strstr(result, "\"matches\":false"),
          "sub-decimeter tolerance still reports the quantization");
    if (result) mg_free_string(result);
}

/// SITL 回归（tools/sitl-mission-scenario.sh 首次跑出来的真实差异）：
/// ArduCopter 的任务结构不存航点接受半径，回读的 param2 固定是 0，实际接受半径
/// 由 WP_RADIUS 决定（libraries/AP_Mission/AP_Mission.cpp:1092-1116）。核对必须
/// 把它记成 normalization，而不是任务差异。
static void test_verify_ignores_fields_the_vehicle_cannot_store() {
    fprintf(stderr, "\n[Group] Verify vs fields the connected vehicle cannot store\n");
    MockVehicleTestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    char* result = mg_mission_doc_create(
        env.core,
        R"({"metadata":{"name":"radius"},"home":{"set":true,"lat":39.9,"lon":116.4,"alt_m":20}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"add","expected_revision":1,"entity":{"type":"waypoint","command":16,"lat":39.9001,"lon":116.4001,"alt_m":50,"params":[0,2,0,0]}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_upload(env.core, 2);
    CHECK(result && strstr(result, "\"success\":true"), "radius mission upload starts");
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) env.tick(50);

    auto stored = env.mock->stored_mission();
    bool dropped = false;
    for (auto& item : stored) {
        if (item.command == MAV_CMD_NAV_WAYPOINT && std::fabs(item.param2 - 2.0f) < 0.01f) {
            item.param2 = 0.0f;  // ArduCopter 读回的值
            dropped = true;
        }
    }
    CHECK(dropped, "copter readback simulation dropped the waypoint acceptance radius");
    env.mock->set_mission(stored);

    result = mg_mission_download(env.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) env.tick(50);

    result = mg_mission_doc_verify(env.core, 1.0);
    CHECK(result && strstr(result, "\"matches\":true"),
          "a dropped acceptance radius still verifies as matching on a copter");
    CHECK(result && strstr(result, "\"difference_count\":0"),
          "the dropped acceptance radius is not reported as a difference");
    CHECK(result && strstr(result, "\"normalization_count\":1"),
          "the dropped acceptance radius is reported as one normalization");
    if (result) mg_free_string(result);
}

/// 本进程 id：并发跑同一个测试二进制时，两个进程必须各用各的临时路径，否则
/// S7 用例会互相覆盖（实测并发 4 份时 3/4 实例失败，失败断言正是
/// "S7: the source file is byte-identical…" 一族）。
unsigned long test_process_id() {
#if defined(_WIN32)
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

/// 读取文件全部字节；用于"原文件逐字节不变"的断言（S7）。
static std::string read_file_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) return {};
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

/// S7：MissionDocument 是唯一内部状态，上传前从它编译 MissionPlan。
/// 导入（读盘）、导出、上传都不得改写用户的原始文件，也不得替换其内容。
static void test_mission_file_is_never_rewritten() {
    fprintf(stderr, "\n[Group] S7 mission file is not silently rewritten\n");
    MockVehicleTestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    // 路径带 pid：并发实例不再共享同一个"用户文件"，S7 的"逐字节不变"语义不变。
    const auto dir = std::filesystem::temp_directory_path() /
                     ("M-GCS-test-" + std::to_string(test_process_id())) /
                     "s7-mission-file";
    std::error_code ignored;
    std::filesystem::remove_all(dir, ignored);
    std::filesystem::create_directories(dir);
    const auto source = dir / "user-mission.mgcs";
    const auto exported = dir / "exported-copy.mgcs";

    // 1) 用户原始文件：先建一份任务文档并落盘。
    char* result = mg_mission_doc_create(env.core, R"({"metadata":{"name":"s7"}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_apply(
        env.core,
        R"({"operation":"add","expected_revision":1,"entity":{"type":"waypoint","command":16,"lat":31.227,"lon":121.49,"alt_m":50,"params":[0,2,0,0]}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_save(env.core, source.string().c_str());
    CHECK(result && strstr(result, "\"success\":true"),
          "the user's mission file is written to disk");
    if (result) mg_free_string(result);

    const std::string before = read_file_bytes(source);
    CHECK(!before.empty(), "the source file really has content to protect");

    // 2) 导入：从磁盘读回文档（真实读盘路径，而不是内存 JSON）。
    result = mg_mission_doc_load(env.core, source.string().c_str());
    CHECK(result && strstr(result, "\"success\":true"),
          "the mission document loads from the user's file");
    if (result) mg_free_string(result);

    result = mg_mission_doc_get(env.core);
    CHECK(result && strstr(result, "31.227") != nullptr,
          "the loaded document carries the file's waypoint (the read really happened)");
    if (result) mg_free_string(result);

    // 3) 导出：原生 JSON + QGC plan 文本，都不接触源文件。
    result = mg_mission_doc_export(env.core);
    CHECK(result && strstr(result, "31.227") != nullptr,
          "exporting the document succeeds");
    if (result) mg_free_string(result);
    result = mg_mission_doc_export_format(env.core, "plan");
    CHECK(result && strstr(result, "Plan") != nullptr,
          "exporting to a QGC plan file succeeds");
    if (result) mg_free_string(result);

    // 4) 上传：飞控真的收到了任务（否则下面的"文件没变"毫无意义）。
    result = mg_mission_doc_upload(env.core, 2);
    if (result) mg_free_string(result);
    for (int i = 0; i < 60; ++i) env.tick(50);
    CHECK(!env.mock->stored_mission().empty(),
          "the mission reached the flight controller");

    // 5) 另存到新路径：只写目标文件。
    result = mg_mission_doc_save(env.core, exported.string().c_str());
    CHECK(result && strstr(result, "\"success\":true"),
          "saving to a new path succeeds");
    if (result) mg_free_string(result);
    CHECK(std::filesystem::exists(exported) &&
              read_file_bytes(exported).find("31.227") != std::string::npos,
          "the new file holds the exported document");

    // 6) 源文件逐字节不变（导入 / 导出 / 上传 / 另存都没有改写它）。
    const std::string after = read_file_bytes(source);
    CHECK(after.size() == before.size(),
          "S7: the source file size is unchanged after import/export/upload");
    CHECK(after == before,
          "S7: the source file is byte-identical after import/export/upload");
    CHECK(std::filesystem::exists(source) && std::filesystem::exists(exported),
          "both the source and the new export exist as separate files");

    // 反向自检：这条断言要能发现"文件被改写"，否则就是恒真的假断言（T2）。
    {
        std::ofstream mutated(source, std::ios::binary | std::ios::trunc);
        mutated << before << "\n";
    }
    CHECK(read_file_bytes(source) != before,
          "the byte comparison really detects a rewritten source file");

    std::filesystem::remove_all(dir, ignored);
}

/// S8：任务传输取消必须把命令发给飞控（MISSION_ACK + OPERATION_CANCELLED），
/// 不能只把本地状态改成 idle 就算"已取消"。
static void test_mission_cancel_reaches_the_vehicle() {
    fprintf(stderr, "\n[Group] S8 mission cancel reaches the vehicle\n");
    MockVehicleTestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    // 对照：取消之前飞控没有收到任何 MISSION_ACK，说明下面的断言不是"总有"。
    env.mock->received_mission_acks.clear();
    char* result = mg_mission_download(env.core);
    CHECK(result && strstr(result, "\"success\":true"),
          "a mission download starts and stays active");
    if (result) mg_free_string(result);
    CHECK(env.mock->received_mission_acks.empty(),
          "no MISSION_ACK is sent while the transfer is running");

    result = mg_operations_cancel(env.core, "mission");
    CHECK(result && strstr(result, "\"canceled\":true"),
          "the active mission transfer is cancelled locally");
    if (result) mg_free_string(result);

    // 命令要真的走到链路对端（env.tick 会把 GCS 的出站字节喂给模拟飞控）。
    for (int i = 0; i < 10; ++i) env.tick(50);

    bool cancel_ack_received = false;
    for (const auto& ack : env.mock->received_mission_acks) {
        if (ack.type == MAV_MISSION_OPERATION_CANCELLED &&
            ack.mission_type == MAV_MISSION_TYPE_MISSION) {
            cancel_ack_received = true;
        }
    }
    CHECK(cancel_ack_received,
          "S8: cancelling sends MISSION_ACK(OPERATION_CANCELLED) to the vehicle");

    // 本地终态同样保持（但这不是唯一的断言）。
    result = mg_operations_get_snapshot(env.core);
    CHECK(result && strstr(result, "\"state\":\"idle\""),
          "the local transfer state is idle after the cancel");
    if (result) mg_free_string(result);
}

int main() {
    fprintf(stderr, "=== Mission Document Simulation Tests ===\n");
    test_verify_tolerates_position_quantization();
    test_verify_ignores_fields_the_vehicle_cannot_store();
    test_mission_file_is_never_rewritten();
    test_mission_cancel_reaches_the_vehicle();
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
    const auto& uploaded_items = env.mock->stored_mission();
    CHECK(uploaded_items.size() == 4, "mock stores the complete transport mission");
    CHECK(uploaded_items[0].frame == MAV_FRAME_GLOBAL &&
              uploaded_items[0].command == MAV_CMD_NAV_WAYPOINT,
          "transport item zero is the ArduPilot home item");
    CHECK(uploaded_items[0].x == 399000000 &&
              uploaded_items[0].y == 1164000000,
          "upload uses the planned document home, not live vehicle position");
    CHECK(std::fabs(uploaded_items[0].z - 20.0f) < 0.01f,
          "upload preserves the planned document home altitude");

    result = mg_mission_doc_verify(env.core, 0.1);
    CHECK(result && strstr(result, "\"code\":-5"), "verify requires a completed readback");
    if (result) mg_free_string(result);

    result = mg_mission_download(env.core);
    CHECK(result && strstr(result, "\"success\":true"), "mission download starts");
    if (result) mg_free_string(result);

    // ArduPilot commonly returns the INT coordinate frame (6) even when the
    // uploaded document used its semantic equivalent (3). Verification must
    // normalize that representation difference instead of reporting drift.
    auto normalized_frame_mission = env.mock->stored_mission();
    normalized_frame_mission[1].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
    normalized_frame_mission[1].x = 123;
    normalized_frame_mission[1].y = 456;
    normalized_frame_mission[1].param4 = NAN;
    env.mock->set_mission(normalized_frame_mission);
    for (int i = 0; i < 80; ++i) env.tick(50);

    result = mg_mission_doc_verify(env.core, 0.1);
    CHECK(result && strstr(result, "\"matches\":true"), "vehicle mission matches compiled document");
    CHECK(result && strstr(result, "\"normalization_count\":4"),
          "verification reports accepted frame, coordinate, and unset-value normalizations");
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

    // 飞控原始列表的第 0 项是 ArduPilot Home。读取时必须把它同步到
    // document.home，并从可见实体里移除，不能显示成第 1 个任务项。
    MockVehicleTestEnv home_env;
    for (int i = 0; i < 15; ++i) home_env.tick(100);
    result = mg_mission_doc_create(
        home_env.core,
        R"({"metadata":{"name":"Home Import"},"home":{"set":true,"lat":1,"lon":2,"alt_m":3}})");
    if (result) mg_free_string(result);

    std::vector<mavlink_mission_item_int_t> home_items(3);
    memset(home_items.data(), 0, sizeof(mavlink_mission_item_int_t) * home_items.size());
    home_items[0].seq = 0;
    home_items[0].frame = MAV_FRAME_GLOBAL;
    home_items[0].command = MAV_CMD_NAV_WAYPOINT;
    home_items[0].autocontinue = 1;
    home_items[0].x = 399000000;
    home_items[0].y = 1164000000;
    home_items[0].z = 20.0f;
    home_items[0].mission_type = MAV_MISSION_TYPE_MISSION;
    home_items[1].seq = 1;
    home_items[1].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
    home_items[1].command = MAV_CMD_NAV_WAYPOINT;
    home_items[1].autocontinue = 1;
    home_items[1].x = 399050000;
    home_items[1].y = 1164050000;
    home_items[1].z = 50.0f;
    home_items[1].mission_type = MAV_MISSION_TYPE_MISSION;
    home_items[2].seq = 2;
    home_items[2].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
    home_items[2].command = MAV_CMD_NAV_LAND;
    home_items[2].autocontinue = 1;
    home_items[2].x = 399060000;
    home_items[2].y = 1164060000;
    home_items[2].z = 0.0f;
    home_items[2].mission_type = MAV_MISSION_TYPE_MISSION;
    home_env.mock->set_mission(home_items);

    result = mg_mission_download(home_env.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) home_env.tick(50);

    result = mg_mission_doc_import_from_vehicle(home_env.core);
    CHECK(result && strstr(result, "\"success\":true"),
          "home-bearing mission imports successfully");
    CHECK(result && strstr(result, "\"lat\":39.9"),
          "import updates document home latitude from transport item zero");
    CHECK(result && strstr(result, "\"lon\":116.4"),
          "import updates document home longitude from transport item zero");
    CHECK(result && strstr(result, "\"wp-1\"") && strstr(result, "\"wp-2\""),
          "import keeps only the real mission entities");
    CHECK(result && !strstr(result, "\"wp-3\""),
          "import does not expose the ArduPilot home item as a task");
    if (result) mg_free_string(result);

    // QGC 的 DO_CHANGE_SPEED 使用 MAV_FRAME_MISSION，x/y 是 param5/6，
    // 不是经纬度。端到端回读时不能把它还原成 (0,0) 航点。
    MockVehicleTestEnv command_env;
    for (int i = 0; i < 15; ++i) command_env.tick(100);
    result = mg_mission_doc_create(
        command_env.core,
        R"({"metadata":{"name":"QGC command import"},"home":{"set":true,"lat":39.9,"lon":116.4,"alt_m":20}})");
    if (result) mg_free_string(result);

    std::vector<mavlink_mission_item_int_t> command_items(3);
    memset(command_items.data(), 0,
           sizeof(mavlink_mission_item_int_t) * command_items.size());
    command_items[0].seq = 0;
    command_items[0].frame = MAV_FRAME_GLOBAL;
    command_items[0].command = MAV_CMD_NAV_WAYPOINT;
    command_items[0].autocontinue = 1;
    command_items[0].x = 399000000;
    command_items[0].y = 1164000000;
    command_items[0].z = 20.0f;
    command_items[0].mission_type = MAV_MISSION_TYPE_MISSION;
    command_items[1].seq = 1;
    // ArduPilot 对非定位命令回读时会规范成 GLOBAL(0)，而不是保留 QGC 的
    // MAV_FRAME_MISSION；这正是产生假 (0,0) 航点的实际回读形态。
    command_items[1].frame = MAV_FRAME_GLOBAL;
    command_items[1].command = MAV_CMD_DO_CHANGE_SPEED;
    command_items[1].autocontinue = 1;
    command_items[1].param1 = 0.0f;
    command_items[1].param2 = 8.0f;
    command_items[1].x = 123;
    command_items[1].y = 456;
    command_items[1].mission_type = MAV_MISSION_TYPE_MISSION;
    command_items[2].seq = 2;
    command_items[2].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
    command_items[2].command = MAV_CMD_NAV_WAYPOINT;
    command_items[2].autocontinue = 1;
    command_items[2].x = 399050000;
    command_items[2].y = 1164050000;
    command_items[2].z = 50.0f;
    command_items[2].mission_type = MAV_MISSION_TYPE_MISSION;
    command_env.mock->set_mission(command_items);

    result = mg_mission_download(command_env.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) command_env.tick(50);

    result = mg_mission_doc_import_from_vehicle(command_env.core);
    CHECK(result && strstr(result, "\"success\":true"),
          "QGC frame-mission command imports from vehicle");
    CHECK(result && strstr(result, "\"lat\":0") == nullptr,
          "downloaded MAV_FRAME_MISSION command is not exposed as (0,0)");
    CHECK(result && strstr(result, "\"command\":178") != nullptr &&
              strstr(result, "\"params\":[0,8,0,0]") != nullptr,
          "downloaded MAV_FRAME_MISSION command keeps its parameters");
    if (result) mg_free_string(result);

    // 只改变飞控侧 Home，不改任务实体；Home 变化必须进入 home_sync，
    // 不能把任务标记为回读失败。
    auto changed_home = home_env.mock->stored_mission();
    changed_home[0].x += 1000;
    home_env.mock->set_mission(changed_home);
    result = mg_mission_download(home_env.core);
    if (result) mg_free_string(result);
    for (int i = 0; i < 80; ++i) home_env.tick(50);

    result = mg_mission_doc_verify(home_env.core, 0.1);
    CHECK(result && strstr(result, "\"matches\":true"),
          "home-only drift does not fail mission verification");
    CHECK(result && strstr(result, "\"difference_count\":0"),
          "home-only drift is not counted as a mission difference");
    CHECK(result && strstr(result, "\"home_sync_count\":1"),
          "verify reports dynamic home drift separately");
    if (result) mg_free_string(result);
    result = mg_mission_doc_verify(env.core, 0.1);
    CHECK(result && strstr(result, "\"matches\":false"),
          "verify detects local document drift");
    CHECK(result && strstr(result, "\"difference_count\":1"),
          "verify reports altitude difference");
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
