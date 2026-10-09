/**
 * test_sim_mission.cc - Mission protocol simulation tests
 *
 * Uses MockFlightController and the FFI API to test mission upload/download.
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"
#include "command_queue.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

using TestEnv = MockVehicleTestEnv;

static mg::core::mission::MissionPlan make_test_mission_plan(size_t item_count) {
    mg::core::mission::MissionPlan plan;
    for (size_t i = 0; i < item_count; ++i) {
        mg::core::mission::MissionItem item{};
        item.seq = static_cast<uint16_t>(i);
        item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
        item.command = MAV_CMD_NAV_WAYPOINT;
        item.autocontinue = 1;
        item.x = static_cast<int32_t>((39.9 + static_cast<double>(i) * 0.01) * 1e7);
        item.y = static_cast<int32_t>((116.4 + static_cast<double>(i) * 0.01) * 1e7);
        item.z = 50.0f + static_cast<float>(i) * 10.0f;
        item.mission_type = MAV_MISSION_TYPE_MISSION;
        plan.items.push_back(item);
    }
    return plan;
}

static void inject_mavlink_message(TestEnv& env, mavlink_message_t& message) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const uint16_t len = mavlink_msg_to_send_buffer(buffer, &message);
    env.link->inject_incoming(buffer, len);
}

static void inject_mission_count(TestEnv& env, uint8_t sysid, uint8_t compid,
                                 uint16_t count, uint8_t mission_type) {
    mavlink_message_t message;
    mavlink_msg_mission_count_pack(sysid, compid, &message,
                                   255, 0, count, mission_type, 0);
    inject_mavlink_message(env, message);
}

static void inject_mission_item_int(TestEnv& env, uint8_t sysid, uint8_t compid,
                                    uint16_t seq, int32_t lat_e7,
                                    uint8_t mission_type) {
    mavlink_mission_item_int_t item{};
    item.seq = seq;
    item.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
    item.command = MAV_CMD_NAV_WAYPOINT;
    item.autocontinue = 1;
    item.x = lat_e7;
    item.y = 1164000000;
    item.z = 50.0f;
    item.mission_type = mission_type;

    mavlink_message_t message;
    mavlink_msg_mission_item_int_encode(sysid, compid, &message, &item);
    inject_mavlink_message(env, message);
}

static void inject_mission_item_float(TestEnv& env, uint8_t sysid, uint8_t compid,
                                      uint16_t seq, float lat_deg,
                                      uint8_t mission_type) {
    mavlink_message_t message;
    mavlink_msg_mission_item_pack(sysid, compid, &message,
                                  255, 0, seq, MAV_FRAME_GLOBAL_RELATIVE_ALT,
                                  MAV_CMD_NAV_WAYPOINT, 0, 1,
                                  0, 0, 0, 0, lat_deg, 116.4f, 50.0f,
                                  mission_type);
    inject_mavlink_message(env, message);
}

static void inject_mission_request_int(TestEnv& env, uint8_t sysid, uint8_t compid,
                                       uint16_t seq, uint8_t mission_type) {
    mavlink_message_t message;
    mavlink_msg_mission_request_int_pack(sysid, compid, &message,
                                         255, 0, seq, mission_type);
    inject_mavlink_message(env, message);
}

static void inject_mission_request(TestEnv& env, uint8_t sysid, uint8_t compid,
                                   uint16_t seq, uint8_t mission_type) {
    mavlink_message_t message;
    mavlink_msg_mission_request_pack(sysid, compid, &message,
                                     255, 0, seq, mission_type);
    inject_mavlink_message(env, message);
}

static int count_outgoing_messages(const std::vector<uint8_t>& bytes, uint32_t msgid) {
    mavlink_message_t message{};
    mavlink_status_t status{};
    int count = 0;
    for (uint8_t byte : bytes) {
        if (mavlink_parse_char(MAVLINK_COMM_2, byte, &message, &status) &&
            message.msgid == msgid) {
            ++count;
        }
    }
    return count;
}

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

static void test_mission_transfer_guards() {
    fprintf(stderr, "\n[Group] Mission transfer guards\n");
    TestEnv env;
    CHECK(env.wait_connected(), "vehicle connected for transfer guard test");

    // 先制造一次旧任务的执行进度，验证新上传事务开始时会被清空。
    mavlink_message_t reached;
    mavlink_msg_mission_item_reached_pack(1, 1, &reached, 1);
    inject_mavlink_message(env, reached);
    CHECK(env.mav->mission_get_execution_progress().reached_count == 1,
          "old execution progress is present before new transaction");

    const auto plan = make_test_mission_plan(2);
    CHECK(env.mav->mission_upload(1, 1, plan), "first mission upload starts");
    CHECK(env.mav->mission_get_execution_progress().reached_count == 0,
          "mission upload clears old execution progress");
    CHECK(!env.mav->mission_upload(1, 1, plan),
          "second mission upload is rejected while one is active");
    CHECK(!env.mav->mission_download(1, 1),
          "mission download is rejected while upload is active");
    CHECK(!env.mav->fence_upload(1, 1, plan),
          "fence upload is rejected while mission transfer is active");

    CHECK(env.mav->mission_cancel(), "active mission transfer can be cancelled");
    CHECK(env.mav->fence_download(1, 1), "fence download starts after cancel");
    CHECK(!env.mav->mission_upload(1, 1, plan),
          "mission upload is rejected while fence transfer is active");
}

static void test_mission_request_routing() {
    fprintf(stderr, "\n[Group] Mission request routing\n");
    TestEnv env;
    CHECK(env.wait_connected(), "vehicle connected for request routing test");

    const auto plan = make_test_mission_plan(2);
    CHECK(env.mav->mission_upload(1, 1, plan), "mission upload starts for request routing");
    env.link->take_outgoing();

    inject_mission_request_int(env, 2, 1, 0, MAV_MISSION_TYPE_MISSION);
    CHECK(count_outgoing_messages(env.link->take_outgoing(), MAVLINK_MSG_ID_MISSION_ITEM_INT) == 0,
          "MISSION_REQUEST_INT from wrong system sends no mission item");

    inject_mission_request_int(env, 1, 2, 0, MAV_MISSION_TYPE_MISSION);
    CHECK(count_outgoing_messages(env.link->take_outgoing(), MAVLINK_MSG_ID_MISSION_ITEM_INT) == 0,
          "MISSION_REQUEST_INT from wrong component sends no mission item");

    inject_mission_request_int(env, 1, 1, 0, MAV_MISSION_TYPE_FENCE);
    CHECK(count_outgoing_messages(env.link->take_outgoing(), MAVLINK_MSG_ID_MISSION_ITEM_INT) == 0,
          "MISSION_REQUEST_INT for another mission type sends no mission item");

    inject_mission_request_int(env, 1, 1, 0, MAV_MISSION_TYPE_MISSION);
    CHECK(count_outgoing_messages(env.link->take_outgoing(), MAVLINK_MSG_ID_MISSION_ITEM_INT) == 1,
          "matching MISSION_REQUEST_INT sends the requested item");

    inject_mission_request(env, 2, 1, 1, MAV_MISSION_TYPE_MISSION);
    CHECK(count_outgoing_messages(env.link->take_outgoing(), MAVLINK_MSG_ID_MISSION_ITEM) == 0,
          "compat MISSION_REQUEST from wrong system sends no mission item");

    inject_mission_request(env, 1, 1, 1, MAV_MISSION_TYPE_MISSION);
    CHECK(count_outgoing_messages(env.link->take_outgoing(), MAVLINK_MSG_ID_MISSION_ITEM) == 1,
          "matching compat MISSION_REQUEST sends the requested item");
}

static void test_mission_download_protocol_validation() {
    fprintf(stderr, "\n[Group] Mission download source and sequence validation\n");
    TestEnv env;
    CHECK(env.wait_connected(), "vehicle connected for strict download test");

    CHECK(env.mav->mission_download(1, 1), "mission download starts for strict validation");
    env.link->take_outgoing();

    inject_mission_count(env, 2, 1, 3, MAV_MISSION_TYPE_MISSION);
    inject_mission_count(env, 1, 2, 3, MAV_MISSION_TYPE_MISSION);
    inject_mission_count(env, 1, 1, 3, MAV_MISSION_TYPE_FENCE);
    auto progress = env.mav->mission_get_progress();
    CHECK(progress.total == 0 && progress.state == mg::core::mission::MissionState::Downloading,
          "foreign or wrong-type MISSION_COUNT is ignored");

    inject_mission_count(env, 1, 1, 3, MAV_MISSION_TYPE_MISSION);
    progress = env.mav->mission_get_progress();
    CHECK(progress.total == 3 && progress.state == mg::core::mission::MissionState::Downloading,
          "matching MISSION_COUNT starts the download");
    env.link->take_outgoing();

    inject_mission_item_int(env, 1, 1, 2, 392000000, MAV_MISSION_TYPE_MISSION);
    CHECK(env.mav->mission_get_plan().items.empty(),
          "out-of-order MISSION_ITEM_INT is not written into the mission");

    inject_mission_item_int(env, 2, 1, 0, 399000000, MAV_MISSION_TYPE_MISSION);
    CHECK(env.mav->mission_get_plan().items.empty(),
          "MISSION_ITEM_INT from the wrong system is ignored");

    inject_mission_item_int(env, 1, 1, 0, 399000000, MAV_MISSION_TYPE_MISSION);
    auto plan = env.mav->mission_get_plan();
    CHECK(plan.items.size() == 1 && plan.items[0].x == 399000000,
          "expected MISSION_ITEM_INT is accepted");

    inject_mission_item_int(env, 1, 1, 0, 401000000, MAV_MISSION_TYPE_MISSION);
    plan = env.mav->mission_get_plan();
    CHECK(plan.items.size() == 1 && plan.items[0].x == 399000000,
          "duplicate MISSION_ITEM_INT does not overwrite the accepted item");

    inject_mission_item_float(env, 2, 1, 1, 40.0f, MAV_MISSION_TYPE_MISSION);
    CHECK(env.mav->mission_get_plan().items.size() == 1,
          "compat MISSION_ITEM from the wrong system is ignored");

    inject_mission_item_float(env, 1, 1, 1, 40.0f, MAV_MISSION_TYPE_MISSION);
    plan = env.mav->mission_get_plan();
    CHECK(plan.items.size() == 2 && plan.items[1].seq == 1,
          "expected compat MISSION_ITEM is accepted");

    inject_mission_item_float(env, 1, 1, 1, 41.0f, MAV_MISSION_TYPE_MISSION);
    CHECK(env.mav->mission_get_plan().items.size() == 2,
          "duplicate compat MISSION_ITEM is not written twice");

    inject_mission_item_int(env, 1, 1, 2, 401000000, MAV_MISSION_TYPE_MISSION);
    plan = env.mav->mission_get_plan();
    progress = env.mav->mission_get_progress();
    CHECK(plan.items.size() == 3 && plan.valid &&
              progress.state == mg::core::mission::MissionState::Idle,
          "sequential final item completes the download without gaps");
}

static void test_command_queue_timeout_notifies_waiters() {
    fprintf(stderr, "\n[Group] CommandQueue timeout notification\n");

    using namespace mg::core::mavlink;
    CommandQueue queue;
    CommandQueueEntry entry{};
    entry.id = 1;
    entry.command = MAV_CMD_COMPONENT_ARM_DISARM;
    entry.target_sys = 1;
    entry.target_comp = 1;
    entry.max_tries = 1;
    entry.try_count = 1;
    entry.ack_timeout_ms = 0;
    entry.send_time = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    queue.entries.push_back(entry);

    mg::core::platform::Mutex mutex;
    std::atomic<bool> waiting{false};
    std::atomic<bool> woke{false};

    std::thread waiter([&]() {
        mg::core::platform::UniqueLock lock(mutex);
        waiting = true;
        woke = queue.cv.wait_for(lock, std::chrono::milliseconds(2000), [&]() {
            return queue.entries[0].state == CommandQueueEntry::State::TimedOut;
        });
    });

    while (!waiting.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto start = std::chrono::steady_clock::now();
    {
        mg::core::platform::UniqueLock lock(mutex);
        queue.check_timeouts([](const CommandQueueEntry&) { return true; });
    }
    waiter.join();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(woke.load(), "timeout waiter is released by notify_all");
    CHECK(elapsed < 800, "timeout waiter does not wait for the outer timeout");
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
    // 最后一项到达只代表航段飞完，任务完成另有 QGC 式判定（见下一个用例）。
    CHECK(progress && strstr(progress, "\"reached_end\":true"),
          "reaching the last mission item reports reached_end");
    if (progress) mg_free_string(progress);

    // 相同飞控任务重新下载时，上一轮 reached 状态必须清空。
    char* redownload = mg_mission_download(env.core);
    if (redownload) mg_free_string(redownload);
    for (int i = 0; i < 80; i++) env.tick(50);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"valid\":true") &&
              strstr(progress, "\"reached_count\":0") &&
              strstr(progress, "\"last_reached_seq\":0") &&
              strstr(progress, "\"reached_end\":false"),
          "mission download clears execution progress from the previous transfer");
    if (progress) mg_free_string(progress);
}

/// 任务完成判定按 QGC 的语义：在任务模式下解锁飞行、之后上锁才算完成；
/// 仅"最后一项到达"只是 reached_end（航段飞完，固定翼还要返航降落）。
static void test_mission_completion_follows_qgc_rules() {
    fprintf(stderr, "\n[Group] Mission completion follows QGC rules\n");
    TestEnv env;
    for (int i = 0; i < 15; i++) env.tick(100);

    // 关掉 mock 的周期心跳，否则它会把注入的解锁/模式状态覆盖掉。
    env.mock->set_heartbeat_enabled(false);

    auto send_heartbeat = [&env](bool armed, uint32_t custom_mode) {
        mavlink_message_t hb;
        const uint8_t base_mode = static_cast<uint8_t>(
            MAV_MODE_FLAG_CUSTOM_MODE_ENABLED |
            (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0));
        mavlink_msg_heartbeat_pack(
            1, 1, &hb, MAV_TYPE_FIXED_WING, MAV_AUTOPILOT_ARDUPILOTMEGA,
            base_mode, custom_mode, MAV_STATE_ACTIVE);
        auto bytes = std::vector<uint8_t>(MAVLINK_MAX_PACKET_LEN);
        const uint16_t len = mavlink_msg_to_send_buffer(bytes.data(), &hb);
        env.link->inject_incoming(bytes.data(), len);
        env.tick(10);
    };

    // ArduPlane 的 AUTO 是 custom_mode 10。
    send_heartbeat(false, 0);
    char* progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"finished\":false"),
          "unarmed flight without a mission is not finished");
    if (progress) mg_free_string(progress);

    // 在 GUIDED（15）解锁飞行：不算任务飞行，上锁后不应判完成。
    send_heartbeat(true, 15);
    send_heartbeat(false, 0);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"finished\":false"),
          "armed flight outside mission mode does not count as mission complete");
    if (progress) mg_free_string(progress);

    // 在 AUTO（10）解锁飞行后上锁：QGC 判定的任务完成。
    send_heartbeat(true, 10);
    send_heartbeat(false, 0);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"finished\":true"),
          "armed mission-mode flight followed by disarm reports finished");
    CHECK(progress && strstr(progress, "\"reached_end\":false"),
          "reached_end stays false when no mission item was reached");
    if (progress) mg_free_string(progress);

    // 再次解锁开启新一轮飞行，完成标记要清掉。
    send_heartbeat(true, 10);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"finished\":false"),
          "a new arm clears the previous mission-complete flag");
    if (progress) mg_free_string(progress);

    // aborted 需要飞控上有任务缓存（valid=true），先给 mock 一份并下载。
    std::vector<mavlink_mission_item_int_t> cached(3);
    for (int i = 0; i < 3; i++) {
        memset(&cached[i], 0, sizeof(mavlink_mission_item_int_t));
        cached[i].seq = static_cast<uint16_t>(i);
        cached[i].command = MAV_CMD_NAV_WAYPOINT;
        cached[i].frame = MAV_FRAME_GLOBAL_RELATIVE_ALT;
        cached[i].x = static_cast<int32_t>((31.23 + i * 0.01) * 1e7);
        cached[i].y = static_cast<int32_t>((121.47 + i * 0.01) * 1e7);
        cached[i].z = 50.0f;
    }
    env.mock->set_mission(cached);
    char* download = mg_mission_download(env.core);
    if (download) mg_free_string(download);
    for (int i = 0; i < 60; i++) env.tick(50);

    // 任务模式下解锁后切到非任务模式（LOITER=12）且没飞到终点 => 中止。
    send_heartbeat(true, 12);
    progress = mg_mission_get_execution_progress(env.core);
    CHECK(progress && strstr(progress, "\"aborted\":true"),
          "leaving mission mode before the last item marks the mission aborted");
    if (progress) mg_free_string(progress);
}


// ================================================================
// 上传 ACK 的来源校验与失败回滚（S1 / S2 / S7）
// ================================================================

/// 只有来自本事务目标飞控/组件的 MISSION_ACK 才算数。
///
/// 多机共用一条链路时，若只按 mission_type 匹配，别的飞机的 ACK 会让本机
/// 把"上传成功/失败"记在自己账上。
static void test_mission_ack_source_validation() {
    fprintf(stderr, "\n[Group] Mission ACK source validation\n");
    TestEnv env;
    CHECK(env.wait_connected(), "vehicle connected for ACK source test");

    // 抑制飞控自带的 ACK，让结论只取决于测试注入的那一条。
    env.mock->set_mission_ack_suppressed(true);

    const auto plan = make_test_mission_plan(2);
    CHECK(env.mav->mission_upload(1, 1, plan), "upload starts");

    mavlink_message_t foreign_ack;
    mavlink_msg_mission_ack_pack(9, MAV_COMP_ID_AUTOPILOT1, &foreign_ack,
                                 0, MAV_COMP_ID_AUTOPILOT1,
                                 MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_MISSION, 0);
    inject_mavlink_message(env, foreign_ack);

    CHECK(env.mav->mission_get_progress().state ==
              mg::core::mission::MissionState::Uploading,
          "ACK from another system does not end the upload");
    CHECK(env.mav->mission_get_plan().valid == false,
          "foreign ACK does not mark the local mission as accepted");

    // 正对照：同一时刻来自目标飞控的 ACK 必须生效。
    mavlink_message_t own_ack;
    mavlink_msg_mission_ack_pack(1, 1, &own_ack,
                                 0, MAV_COMP_ID_AUTOPILOT1,
                                 MAV_MISSION_ACCEPTED, MAV_MISSION_TYPE_MISSION, 0);
    inject_mavlink_message(env, own_ack);

    CHECK(env.mav->mission_get_progress().state ==
              mg::core::mission::MissionState::Idle,
          "ACK from the transfer target ends the upload");
}

/// 飞控拒绝上传后，本地"飞控任务"必须退回上一份被接受的任务。
///
/// 上传一开始就把客户端计划写进了本地缓存；不回滚的话 mission_get_plan 会返回
/// 一份飞控从未接受的任务，界面与飞控就此不一致。
static void test_rejected_upload_rolls_back_vehicle_plan() {
    fprintf(stderr, "\n[Group] Rejected upload rolls back\n");
    TestEnv env;
    CHECK(env.wait_connected(), "vehicle connected for rollback test");

    // 1) 先走一次真实上传，让飞控接受，作为"飞控里的任务"。
    const auto accepted_plan = make_test_mission_plan(2);
    CHECK(env.mav->mission_upload(1, 1, accepted_plan), "first upload starts");
    for (int i = 0; i < 60 && env.mav->mission_get_progress().state !=
                                  mg::core::mission::MissionState::Idle; ++i) {
        env.tick(20);
    }
    CHECK(env.mav->mission_get_progress().state ==
              mg::core::mission::MissionState::Idle,
          "first upload completes");

    const size_t accepted_items = env.mav->mission_get_plan().items.size();
    const uint64_t accepted_revision =
        env.mav->mission_get_plan_provenance().plan_revision;
    CHECK(accepted_items == 2, "accepted plan is cached locally");
    CHECK(env.mav->mission_get_plan().valid, "accepted plan is marked valid");

    // 2) 第二次上传：飞控拒绝，本地缓存必须退回第 1 份。
    env.mock->set_mission_ack_suppressed(true);
    const auto rejected_plan = make_test_mission_plan(5);
    CHECK(env.mav->mission_upload(1, 1, rejected_plan), "second upload starts");
    CHECK(env.mav->mission_get_plan().items.size() == 5,
          "upload optimistically publishes the client plan");

    mavlink_message_t rejection;
    mavlink_msg_mission_ack_pack(1, 1, &rejection,
                                 0, MAV_COMP_ID_AUTOPILOT1,
                                 MAV_MISSION_ERROR, MAV_MISSION_TYPE_MISSION, 0);
    inject_mavlink_message(env, rejection);

    const auto cached = env.mav->mission_get_plan();
    CHECK(cached.items.size() == accepted_items,
          "rejected upload leaves the previously accepted plan in place");
    CHECK(env.mav->mission_get_plan_provenance().plan_revision == accepted_revision,
          "rejected upload restores the previous plan revision");
    CHECK(env.mav->mission_get_progress().error.find("rejected") != std::string::npos,
          "rejection reason is reported to the caller");
}

int main() {
    fprintf(stderr, "=== Mission Simulation Tests ===\n");
    test_mission_upload();
    test_mission_upload_compat_request();
    test_mission_download();
    test_mission_download_compat_item();
    test_mission_clear();
    test_mission_transfer_guards();
    test_mission_ack_source_validation();
    test_rejected_upload_rolls_back_vehicle_plan();
    test_mission_request_routing();
    test_mission_download_protocol_validation();
    test_command_queue_timeout_notifies_waiters();
    test_mission_execution_progress();
    test_mission_completion_follows_qgc_rules();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
