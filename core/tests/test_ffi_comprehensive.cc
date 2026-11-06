/**
 * test_ffi_comprehensive.cc - Comprehensive offline FFI function tests
 *
 * Tests all FFI functions that can be verified without a real FC connection.
 * Uses mg_create() + no connection to test null/disconnected paths.
 * Uses InMemoryLink for connected-path tests where needed.
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "ffi_internal.h"
#include "mock_flight_controller.h"
#include "cJSON.h"
#include "core/util/json.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();

static bool contains(const char* json, const char* needle) {
    return json && strstr(json, needle) != nullptr;
}

static bool is_success(const char* json) { return contains(json, "\"success\":true"); }
static bool is_failure(const char* json) { return contains(json, "\"success\":false"); }

static void check_failure_and_free(char* result, const char* label) {
    CHECK(is_failure(result), label);
    mg_free_string(result);
}

static void test_null_core() {
    fprintf(stderr, "\n[Group] Null core safety\n");
    // All functions should handle null core gracefully
    CHECK(is_failure(mg_connection_get_session_state(nullptr, 0)), "connection_get_session_state null");
    CHECK(is_failure(mg_firmware_get_type(nullptr)), "firmware_get_type null");
    CHECK(is_failure(mg_rc_get_channels(nullptr)), "rc_get_channels null");
    CHECK(is_failure(mg_servo_get_outputs(nullptr)), "servo_get_outputs null");
    CHECK(is_failure(mg_params_request_all(nullptr)), "params_request_all null");
    CHECK(is_failure(mg_params_get_by_name(nullptr, "TEST")), "params_get_by_name null");
    check_failure_and_free(mg_tlog_status(nullptr), "logging_stats null");
    CHECK(mg_vehicle_get_active(nullptr) == -1, "vehicle_get_active null -> -1");
    CHECK(mg_vehicle_select(nullptr, 0) == nullptr || is_failure(mg_vehicle_select(nullptr, 0)), "vehicle_select null");
}

static void test_lifecycle() {
    fprintf(stderr, "\n[Group] Lifecycle\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create returns non-null");

    // Vehicle list should be empty
    char* list = mg_vehicle_list(core);
    CHECK(contains(list, "\"count\":0") || contains(list, "[]") || contains(list, "\"count\":0,"), "empty vehicle list");
    mg_free_string(list);

    // Active vehicle should be -1
    CHECK(mg_vehicle_get_active(core) == -1, "no active vehicle");

    // Error system
    mg_clear_last_error();
    CHECK(mg_get_last_error() == MG_ERROR_NONE, "error cleared to NONE");

    mg_destroy(core);
}

static void test_disconnected_telemetry_getters() {
    fprintf(stderr, "\n[Group] Telemetry getters (disconnected)\n");
    mg_core_t* core = mg_create();

    // Telemetry getters use one response contract when disconnected.
    char* r;
    r = mg_vehicle_get_summary(core);  CHECK(is_success(r), "vehicle summary success"); mg_free_string(r);
    r = mg_vehicle_get_attitude(core); CHECK(is_success(r), "attitude success"); mg_free_string(r);
    r = mg_vehicle_get_global_position(core); CHECK(is_success(r), "global position success"); mg_free_string(r);
    r = mg_vehicle_get_home_position(core); CHECK(is_success(r), "home position success"); mg_free_string(r);
    r = mg_vehicle_get_vfr_hud(core);  CHECK(is_success(r), "VFR HUD success"); mg_free_string(r);
    r = mg_vehicle_get_extended_sys_state(core); CHECK(is_success(r), "extended system state success"); mg_free_string(r);
    r = mg_vehicle_get_firmware_version(core); CHECK(is_success(r), "firmware version success"); mg_free_string(r);

    mg_destroy(core);
}

static void test_disconnected_commands() {
    fprintf(stderr, "\n[Group] Commands (disconnected, should fail)\n");
    mg_core_t* core = mg_create();

    check_failure_and_free(mg_command_arm(core, 0), "arm disconnected -> fail");
    check_failure_and_free(mg_command_disarm(core, 0), "disarm disconnected -> fail");
    check_failure_and_free(mg_command_rtl(core), "rtl disconnected -> fail");
    check_failure_and_free(mg_command_takeoff(core, 10), "takeoff disconnected -> fail");
    check_failure_and_free(mg_command_land(core), "land disconnected -> fail");
    check_failure_and_free(
        mg_command_submit_for_vehicle(core, 0, R"({"command":"set_mode","args":{"mode":"Stabilize"}})"),
        "set_mode disconnected -> fail");

    // Mission
    check_failure_and_free(mg_mission_download(core), "mission_download disconnected -> fail");

    mg_destroy(core);
}

static void test_disconnected_health() {
    fprintf(stderr, "\n[Group] Health & firmware (disconnected)\n");
    mg_core_t* core = mg_create();

    char* r;
    r = mg_firmware_get_type(core);
    CHECK(r != nullptr, "firmware_type no crash");
    mg_free_string(r);

    r = mg_rc_get_channels(core);
    CHECK(r != nullptr, "rc_get_channels no crash");
    mg_free_string(r);

    r = mg_servo_get_outputs(core);
    CHECK(r != nullptr, "servo_get_outputs no crash");
    mg_free_string(r);

    mg_destroy(core);
}

static void test_firmware_type_normalization_contract() {
    fprintf(stderr, "\n[Group] Firmware type normalization contract (disconnected defaults)\n");
    mg_core_t* core = mg_create();

    char* r = mg_firmware_get_type(core);
    CHECK(r != nullptr, "firmware_type returns json");
    CHECK(is_success(r), "firmware_type uses success envelope");
    CHECK(contains(r, "\"data\""), "firmware_type contains data envelope");
    CHECK(contains(r, "\"schema_version\""), "firmware_type contains schema_version");
    CHECK(contains(r, "\"firmware\""), "firmware_type contains firmware");
    CHECK(contains(r, "\"profile\""), "firmware_type contains profile");
    CHECK(contains(r, "\"vehicle_type\""), "firmware_type contains vehicle_type");
    CHECK(contains(r, "\"capability_flags\""), "firmware_type contains capability_flags");
    CHECK(contains(r, "\"capabilities\""), "firmware_type contains capabilities object");
    CHECK(contains(r, "\"guided_mode\""), "firmware_type capabilities include guided_mode");
    mg_free_string(r);

    mg_destroy(core);
}

static void test_backend_hardening_contracts() {
    fprintf(stderr, "\n[Group] Backend hardening contracts\n");
    mg_core_t* core = mg_create();

    char* conn = mg_connection_get_diagnostics(core);
    CHECK(is_success(conn), "connection diagnostics success envelope");
    CHECK(contains(conn, "\"connected\":false"), "connection diagnostics reports disconnected");
    CHECK(contains(conn, "\"reconnectable\":true"), "connection diagnostics reports reconnectable");
    CHECK(contains(conn, "\"target\""), "connection diagnostics contains target");
    mg_free_string(conn);

    char* ops = mg_operations_get_snapshot(core);
    CHECK(is_success(ops), "operations snapshot success envelope");
    CHECK(contains(ops, "\"id\":\"params\""), "operations snapshot includes params");
    CHECK(contains(ops, "\"id\":\"mission\""), "operations snapshot includes mission");
    CHECK(contains(ops, "\"id\":\"calibration\""), "operations snapshot includes calibration");
    CHECK(contains(ops, "\"id\":\"log\""), "operations snapshot includes log download");
    CHECK(contains(ops, "\"state\":\"idle\""), "operations snapshot starts idle");
    mg_free_string(ops);

    char* cancel_unknown = mg_operations_cancel(core, "unknown");
    CHECK(is_failure(cancel_unknown), "unknown operation cancel fails");
    CHECK(contains(cancel_unknown, "\"unknown operation\""), "unknown operation cancel explains reason");
    mg_free_string(cancel_unknown);

    char* cancel_params = mg_operations_cancel(core, "params");
    CHECK(is_success(cancel_params), "known operation cancel returns success envelope");
    CHECK(contains(cancel_params, "\"canceled\":false"), "idle operation cancel is explicit no-op");
    mg_free_string(cancel_params);

    char* dash = mg_telemetry_get_dashboard_snapshot(core);
    CHECK(is_success(dash), "dashboard snapshot success envelope");
    CHECK(contains(dash, "\"connection\""), "dashboard includes connection");
    CHECK(contains(dash, "\"telemetry\""), "dashboard includes telemetry");
    CHECK(contains(dash, "\"rc_channels\""), "dashboard includes RC channels");
    CHECK(contains(dash, "\"servo_output\""), "dashboard includes servo outputs");
    CHECK(contains(dash, "\"validity\""), "dashboard includes validity");
    mg_free_string(dash);

    char* sys = mg_system_get_diagnostics(core);
    CHECK(is_success(sys), "system diagnostics success envelope");
    CHECK(contains(sys, "\"last_error\""), "system diagnostics includes last_error");
    CHECK(contains(sys, "\"dashboard\""), "system diagnostics includes dashboard");
    mg_free_string(sys);

    mg_destroy(core);
}

static void test_disconnected_config() {
    fprintf(stderr, "\n[Group] Config (disconnected)\n");
    mg_core_t* core = mg_create();

    char* metadata = mg_config_read_param(core, "TEST");
    CHECK(contains(metadata, "\"found\":false"),
          "unknown parameter metadata is absent without a connection");
    mg_free_string(metadata);
    CHECK(is_failure(mg_config_write_param(core, "TEST", 1.0f)), "config_write disconnected -> fail");

    char* r;
    r = mg_tlog_status(core);
    CHECK(contains(r, "\"recording\":false"), "logging is inactive before connection");
    mg_free_string(r);

    mg_destroy(core);
}

static void test_error_system() {
    fprintf(stderr, "\n[Group] Error system\n");

    mg_clear_last_error();
    CHECK(mg_get_last_error() == MG_ERROR_NONE, "clear -> NONE");

    mg_set_last_error(MG_ERROR_NULL_CORE);
    CHECK(mg_get_last_error() == MG_ERROR_NULL_CORE, "set/get error round-trip");
    mg_clear_last_error();
    CHECK(mg_get_last_error() == MG_ERROR_NONE, "clear after set");
}

// ================================================================
// 并发缺陷回归（契约 t3）
//
// 1) 断链悬垂指针：FFI 的等待路径（等 COMMAND_ACK 最长 5 s）必须持有
//    shared_ptr 租约，不能在 reset() 析构适配器后继续用裸指针。
// 2) 命令队列槽位：只提交不 poll 不得把 32 个槽位永久占满。
// ================================================================
namespace {

/// 最小会话环境：只注册一架车，用于"取适配器 vs 断链"的并发语义测试。
struct SessionEnv {
    mg_core_t* core = nullptr;
    std::shared_ptr<mg::core::vehicle::Vehicle> vehicle;
    int vehicle_id = 77;

    explicit SessionEnv(int id = 77) : vehicle_id(id) {
        core = mg_create();
        vehicle = std::make_shared<mg::core::vehicle::Vehicle>(id);
        core->vehicles.add_for_test(vehicle);
        core->vehicles.set_active_vehicle(id);
    }
    ~SessionEnv() { mg_destroy(core); }

    /// 接线一对全新的 InMemoryLink + MavlinkAdapter：每轮都是新对象，
    /// 于是随后的断链会真的析构上一轮的适配器（正是悬垂指针的触发条件）。
    void wire_fresh() {
        auto link = std::make_unique<mg::core::transport::InMemoryLink>();
        link->open();
        auto* raw_link = link.get();
        auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
        mav->set_send_callback([raw_link](const uint8_t* data, size_t size) {
            return raw_link->send(data, size);
        });
        vehicle->conn().wire_for_test(std::move(link), std::move(mav));
    }
};

/// 带 mock 飞控的环境：有真实心跳（进入 Connected），可抑制 COMMAND_ACK。
struct CommandEnv {
    mg_core_t* core = nullptr;
    std::shared_ptr<mg::core::vehicle::Vehicle> vehicle;
    mg::core::transport::InMemoryLink* link = nullptr;
    MockFlightController* mock = nullptr;
    std::atomic<bool> pumping{false};
    std::thread pump_thread;
    int vehicle_id = 77;

    explicit CommandEnv(bool ack_commands = true, int id = 77) : vehicle_id(id) {
        core = mg_create();
        vehicle = std::make_shared<mg::core::vehicle::Vehicle>(id);
        auto link_up = std::make_unique<mg::core::transport::InMemoryLink>();
        link_up->open();
        auto* raw_link = link_up.get();
        auto mav_up = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
        mav_up->set_send_callback([raw_link](const uint8_t* data, size_t size) {
            return raw_link->send(data, size);
        });
        vehicle->conn().wire_for_test(std::move(link_up), std::move(mav_up));
        vehicle->init_subsystems();
        vehicle->wire_telemetry_callbacks();
        core->vehicles.add_for_test(vehicle);
        core->vehicles.set_active_vehicle(id);
        link = raw_link;
        mock = new MockFlightController(*raw_link, MockFlightController::Config());
        mock->set_cmd_no_response(!ack_commands);
    }
    ~CommandEnv() {
        stop_pump();
        delete mock;
        mg_destroy(core);
    }

    void tick(int elapsed_ms = 20, int drain_limit = 20) {
        mock->tick(elapsed_ms);
        for (int drain = 0; drain < drain_limit; ++drain) {
            auto outgoing = link->take_outgoing();
            if (outgoing.empty()) break;
            mock->on_gcs_data(outgoing.data(), outgoing.size());
        }
    }

    /// 建立连接。用"推进 mock 时钟"而不是墙钟超时来判定：每 tick 推进 20 ms
    /// 模拟时间、心跳每 1000 ms 发一次，所以 60 tick 内必到；给 200 tick 的余量。
    /// 旧版用 5 s 墙钟上限，机器被 39 个可运行线程（10 核）压住时本线程会被饿死
    /// 到超过 5 s，于是把"连接握手"误判成失败（实测 101 PASSED, 3 FAILED）。
    /// 这里不再有墙钟 deadline：负载只会让它变慢，不会让它失败。
    bool wait_connected(int max_ticks = 200) {
        for (int i = 0; i < max_ticks; ++i) {
            if (vehicle->conn().is_connected()) return true;
            tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return vehicle->conn().is_connected();
    }

    void start_pump() {
        if (pumping.load()) return;
        pumping = true;
        pump_thread = std::thread([this] {
            while (pumping.load()) {
                tick();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    void stop_pump() {
        pumping = false;
        if (pump_thread.joinable()) pump_thread.join();
    }
};

/// 从 mg_command_submit_for_vehicle 的响应里取 ticket（并释放响应）。
uint64_t ticket_of(char* result) {
    cJSON* json = result ? mg::core::json::parse(result) : nullptr;
    cJSON* ticket = json ? cJSON_GetObjectItemCaseSensitive(json, "ticket") : nullptr;
    const uint64_t id = ticket && cJSON_IsNumber(ticket)
        ? static_cast<uint64_t>(ticket->valuedouble) : 0;
    cJSON_Delete(json);
    mg_free_string(result);
    return id;
}

/// 任务结果是否已就绪（不消费条目，供"客户端从不 poll"的用例使用）。
bool command_future_ready(mg_core_t* core, uint64_t ticket, bool& found) {
    mg::core::platform::LockGuard lk(core->command_jobs_mtx);
    const auto it = core->command_jobs.find(ticket);
    found = it != core->command_jobs.end();
    return found &&
        it->second.result.wait_for(std::chrono::milliseconds(0)) ==
            std::future_status::ready;
}

}  // namespace

/// 并发缺陷 1 的判别用例（TSan 报 data race / ASan 报 use-after-free）。
/// 读取方走生产 FFI 入口 resolve_vehicle_session()（每条 FFI 命令都会经过它），
/// 写入方反复"接线 -> 断链"。
/// 旧代码：resolve -> ConnectionManager 的借用适配器访问，
///   与 reset() 在锁内的 move 并发 = data race；随后解引用已析构的适配器 = UAF。
/// 新代码：resolve 在锁内取 shared_mavlink() 租约，view 存活期间适配器不析构。
static void test_session_adapter_access_survives_concurrent_disconnect() {
    fprintf(stderr, "\n[Group] session adapter access vs concurrent disconnect\n");
    SessionEnv env;
    env.wire_fresh();

    std::atomic<bool> stop{false};
    std::atomic<bool> reader_live{false};
    std::atomic<long> observations{0};
    std::thread reader([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            const auto view = mg::bridge::ffi::resolve_vehicle_session(env.core);
            if (view.mavlink_lease) {
                (void)view.mavlink_lease->latest_heartbeat();
                observations.fetch_add(1, std::memory_order_relaxed);
                reader_live.store(true, std::memory_order_relaxed);
            }
        }
    });

    // 先确认读取线程真的跑起来（env.wire_fresh() 已经在启动前接好线，所以它第一次
    // 迭代就该看到活适配器），再开始写方循环。
    //
    // 这一步把"读线程还没被调度"从偶发失败（旧版断言 observations > 0，重负载下
    // 会以 114/1 的形式随机报错）变成一条有上限、信息明确的检查：它测的是"线程是否
    // 起得来"，而不是"调度延迟恰好小于写方循环耗时"。读取方仍然贯穿整个 150 轮
    // wire/reset，并发语义的检验力不变（TSan/ASan 的判别靠真实交错，不靠计数器）。
    const auto reader_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!reader_live.load(std::memory_order_relaxed) &&
           std::chrono::steady_clock::now() < reader_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(reader_live.load(std::memory_order_relaxed),
          "the reader thread observed a live adapter before the writer loop");

    // mavlink_ptr 每轮都在锁内被写回、被移出并析构。
    for (int i = 0; i < 150; ++i) {
        env.wire_fresh();
        env.vehicle->conn().reset();
    }
    stop.store(true);
    reader.join();

    CHECK(observations.load() > 0, "the reader observed a live adapter");
    CHECK(env.vehicle->conn().shared_mavlink() == nullptr,
          "the manager ends detached after the last reset");
}

/// 并发缺陷 1 的端到端用例：飞控不回应 ACK，命令卡在 5 s 等待期间断链。
/// 旧代码：worker 的裸适配器被 reset() 在锁外析构 -> use-after-free。
/// 新代码：worker 全程持有租约，命令以超时失败收场而不是崩溃。
static void test_command_ack_wait_survives_concurrent_disconnect() {
    fprintf(stderr, "\n[Group] command ACK wait vs concurrent disconnect\n");
    CommandEnv env(/*ack_commands=*/false);
    // T2：不能写 CHECK(true, ...)——那是恒真的假断言。把前置条件的真实
    // 结果断言一次，失败时仍然提前返回。
    const bool connected = env.wait_connected();
    CHECK(connected, "mock vehicle connects for the disconnect test");
    if (!connected) return;
    env.start_pump();

    std::string result;
    std::thread worker([&] {
        char* raw = mg_command_execute_for_vehicle(
            env.core, env.vehicle_id, R"({"command":"arm"})");
        result = raw ? raw : "";
        mg_free_string(raw);
    });

    // 等命令确实发出去、worker 已进入 ACK 等待（mock 不回应，最长 5 s）。
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    env.stop_pump();                 // 测试泵先停，避免它碰已关闭的 link
    env.vehicle->conn().reset();     // 断链：适配器在锁外析构
    worker.join();                   // 有界：最多等到 ACK 超时

    CHECK(!result.empty(), "the in-flight command still returns a response");
    CHECK(contains(result.c_str(), "\"success\":false"),
          "the command fails cleanly instead of crashing on a freed adapter");
}

/// 并发缺陷 3：32 条"提交了但从不 poll"的结果不得永久占满队列。
/// 旧代码只在客户端 poll 且 future ready 时 erase，第 33 次提交恒返回 BUSY。
static void test_command_queue_reclaims_never_polled_slots() {
    fprintf(stderr, "\n[Group] command queue slot reclamation\n");
    CommandEnv env;
    // T2：不能写 CHECK(true, ...)——那是恒真的假断言。把前置条件的真实
    // 结果断言一次，失败时仍然提前返回。
    const bool connected = env.wait_connected();
    CHECK(connected, "mock vehicle connects for the queue test");
    if (!connected) return;

    // 造出"连续提交、结果已就绪、客户端从未 poll"的满队列状态。
    {
        mg::core::platform::LockGuard lk(env.core->command_jobs_mtx);
        for (int i = 0; i < 32; ++i) {
            auto ready = std::async(std::launch::async,
                                    [] { return std::string("{\"success\":true}"); });
            ready.wait();
            env.core->command_jobs.emplace(
                env.core->next_command_job++,
                mg_core_t::CommandJob{env.vehicle_id, "never_polled", std::move(ready)});
        }
    }
    CHECK(env.core->command_jobs.size() == 32,
          "queue is full of never-polled results");

    char* submitted = mg_command_submit_for_vehicle(
        env.core, env.vehicle_id,
        R"({"command":"change_altitude","args":{"altitude_change_m":1}})");
    CHECK(is_success(submitted),
          "a new submission reclaims a slot instead of returning BUSY");
    mg_free_string(submitted);

    const size_t reclaimed = ffi_command_jobs_collect(env.core, 0);
    CHECK(reclaimed >= 31,
          "completed-but-unpolled slots are reclaimable, not permanent");
}

/// 并发缺陷 3 的真实路径：命令跑完、客户端从未 poll 时槽位仍可回收；
/// 取舍是被回收的 ticket 再 poll 会拿到 not found —— 比"整条命令通道永久卡死"好。
static void test_completed_command_is_reclaimable_without_poll() {
    fprintf(stderr, "\n[Group] unpolled completed command reclamation\n");
    CommandEnv env;
    // T2：不能写 CHECK(true, ...)——那是恒真的假断言。把前置条件的真实
    // 结果断言一次，失败时仍然提前返回。
    const bool connected = env.wait_connected();
    CHECK(connected, "mock vehicle connects for the unpolled test");
    if (!connected) return;

    const uint64_t ticket = ticket_of(mg_command_submit_for_vehicle(
        env.core, env.vehicle_id, R"({"command":"rtl"})"));
    CHECK(ticket > 0, "async submission returns a ticket");

    // 不用后台泵线程：由本线程确定性地把命令送给 mock、把 ACK 收回。命令本身仍在
    // worker 线程上执行，"结果何时就绪"不再取决于第二个线程能否被及时调度。
    // 同样不用墙钟 deadline：按 tick 计数推进，负载只会让它变慢，不会让它失败。
    bool ready = false;
    bool found = false;
    for (int i = 0; i < 1000 && !ready; ++i) {
        ready = command_future_ready(env.core, ticket, found);
        if (ready) break;
        env.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(ready, "the command completed while its client never polled");

    CHECK(env.core->command_jobs.size() == 1,
          "a completed result is kept for a client that may still poll");
    CHECK(ffi_command_jobs_collect(env.core, 0) == 1,
          "the unpolled completed result is reclaimed");
    CHECK(env.core->command_jobs.empty(), "the slot is free again");

    char* polled = mg_command_poll(env.core, ticket);
    CHECK(is_failure(polled),
          "a reclaimed ticket reports not found instead of pinning the queue");
    mg_free_string(polled);
}

/// t14 A 组回归：断链之后 Vehicle::conn() 不得返回空引用解引用。
///
/// 旧代码 `Vehicle::conn()` == `*primary_connection()`；`Vehicle::disconnect()`
/// 会把 `connections_` 整个 swap 走，而 fallback vehicle 永远可以被
/// `resolve_vehicle()` 选中。于是这条**纯公开 FFI 调用序列**在第二步之后就在
/// `ConnectionManager::state_`（偏移 0x74）上空指针解引用崩溃（实测
/// EXC_BAD_ACCESS, address=0x74，连接状态读取路径 + 96）。
///
/// 修复后 conn() 在没有路由时返回一个永不接线的 ConnectionManager，语义就是
/// "这辆车没有连接"：connected:false / state:"disconnected" / 无心跳、无链路、
/// 无适配器。本用例同时覆盖同族的其它读接口（它们都走 Vehicle::conn()）。
static void test_connection_state_after_disconnect_without_active_vehicle() {
    fprintf(stderr, "\n[Group] disconnect without an active vehicle\n");
    mg_core_t* core = mg_create();
    CHECK(mg_vehicle_get_active(core) == -1,
          "no active vehicle before the disconnect (fallback stays reachable)");

    core->fallback_vehicle->disconnect();

    // 断链后 fallback vehicle 的 conn() 必须返回断开基线，而不是解引用空指针。
    auto& conn = core->fallback_vehicle->conn();
    CHECK(!conn.is_connected(), "fallback connection reports connected:false");
    // state_string() 返回 const char*，直接和字面量比是在比指针（靠字面量合并
    // 碰巧通过），必须转成 std::string 再比内容。
    CHECK(std::string(mg::core::transport::ConnectionManager::state_string(
              conn.state())) == "disconnected",
          "fallback connection reports the disconnected state string");
    CHECK(!conn.heartbeat_alive(5000), "fallback connection reports no live heartbeat");
    CHECK(conn.heartbeat_age_ms() == 0, "fallback connection reports no heartbeat age");

    // 同族读接口同样取道 Vehicle::conn()，都必须安全且返回断开语义。
    char* diagnostics = mg_connection_get_diagnostics(core);
    CHECK(is_success(diagnostics), "connection diagnostics survive the disconnect");
    mg_free_string(diagnostics);

    mg_destroy(core);
}

int main() {
    fprintf(stderr, "=== Comprehensive FFI Test Suite ===\n");

    test_null_core();
    test_lifecycle();
    test_disconnected_telemetry_getters();
    test_disconnected_commands();
    test_disconnected_health();
    test_firmware_type_normalization_contract();
    test_backend_hardening_contracts();
    test_disconnected_config();
    test_error_system();
    test_session_adapter_access_survives_concurrent_disconnect();
    test_command_ack_wait_survives_concurrent_disconnect();
    test_command_queue_reclaims_never_polled_slots();
    test_completed_command_is_reclaimable_without_poll();
    test_connection_state_after_disconnect_without_active_vehicle();

    fprintf(stderr, "\n========================================\n");
    fprintf(stderr, "  Results: %d PASSED, %d FAILED\n", g_pass, g_fail);
    fprintf(stderr, "========================================\n");
    return g_fail > 0 ? 1 : 0;
}

