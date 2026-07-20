/**
 * test_state_machines.cc - 独立测试提取出的状态机
 *
 * 测试 CommandQueue、ParamCache、MissionTransferState 的核心逻辑。
 * 不需要 MavlinkAdapter、InMemoryLink 或 MockFlightController。
 */
#include "command_queue.h"
#include "param_cache.h"
#include "mission_transfer.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

using namespace mg::core::mavlink;

// ================================================================
// CommandQueue 测试
// ================================================================
static void test_command_queue() {
    fprintf(stderr, "\n[Group] CommandQueue\n");

    // 入队和查找
    CommandQueue q;
    CHECK(q.entries.empty(), "queue starts empty");

    CommandQueueEntry e1{};
    e1.id = q.allocate_id();
    e1.command = 400;  // ARM
    e1.target_sys = 1;
    e1.target_comp = 0;
    e1.params[0] = 1.0f;
    e1.max_tries = 1;
    e1.ack_timeout_ms = 1500;
    e1.send_time = std::chrono::steady_clock::now();
    q.entries.push_back(e1);

    CHECK(q.entries.size() == 1, "entry added to queue");
    CHECK(q.find_by_id(e1.id) == 0, "find_by_id works");
    CHECK(q.find_by_command(0, 400) == 0, "find_by_command works");
    CHECK(q.find_by_command(1, 400) == 0, "broadcast target component accepts autopilot ACK");
    CHECK(q.find_by_command(0, 999) == -1, "find_by_command returns -1 for unknown");

    // ACK 处理
    uint64_t matched = q.handle_ack(1, 400, 0, 0);  // ACCEPTED
    CHECK(matched == e1.id, "handle_ack returns matched id");
    CHECK(q.entries[0].state == CommandQueueEntry::State::Acked, "entry state is Acked");
    CHECK(q.entries[0].ack_result == 0, "ack result is ACCEPTED");

    // IN_PROGRESS 处理
    CommandQueueEntry e2{};
    e2.id = q.allocate_id();
    e2.command = 22;  // TAKEOFF
    e2.target_sys = 1;
    e2.target_comp = 0;
    e2.max_tries = 1;
    e2.ack_timeout_ms = 3000;
    e2.send_time = std::chrono::steady_clock::now();
    q.entries.push_back(e2);

    uint64_t matched2 = q.handle_ack(0, 22, 5, 50);  // IN_PROGRESS, progress=50
    CHECK(matched2 == e2.id, "IN_PROGRESS matched");
    CHECK(q.entries[1].state == CommandQueueEntry::State::InProgress, "state is InProgress");
    CHECK(q.entries[1].ack_progress == 50, "progress is 50");
    CHECK(q.entries[1].ack_timeout_ms == 30000, "timeout reset to 30s for IN_PROGRESS");

    q.entries[1].send_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(31000);
    int in_progress_resend_count = 0;
    auto in_progress_timed_out = q.check_timeouts([&](const CommandQueueEntry&) -> bool {
        in_progress_resend_count++;
        return true;
    });
    CHECK(in_progress_resend_count == 0, "IN_PROGRESS timeout does not resend");
    CHECK(q.entries[1].state == CommandQueueEntry::State::TimedOut, "IN_PROGRESS moves to TimedOut after final timeout");
    CHECK(in_progress_timed_out.size() == 1, "IN_PROGRESS timeout returns one completed entry");

    // 超时重发
    CommandQueueEntry e3{};
    e3.id = q.allocate_id();
    e3.command = 511;  // SET_MESSAGE_INTERVAL
    e3.target_sys = 1;
    e3.target_comp = 0;
    e3.max_tries = 3;
    e3.ack_timeout_ms = 100;  // 短超时用于测试
    e3.try_count = 1;
    e3.send_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(200);
    q.entries.push_back(e3);

    int resend_count = 0;
    auto timed_out = q.check_timeouts([&](const CommandQueueEntry&) -> bool {
        resend_count++;
        return true;
    });
    CHECK(resend_count == 1, "timeout triggered resend");
    CHECK(q.entries[2].try_count == 2, "try_count incremented");
    CHECK(q.entries[2].state == CommandQueueEntry::State::Pending, "still pending after retry");

    // 重试耗尽
    q.entries[2].try_count = 3;
    q.entries[2].send_time = std::chrono::steady_clock::now() - std::chrono::milliseconds(200);
    timed_out = q.check_timeouts([](const CommandQueueEntry&) -> bool { return true; });
    CHECK(q.entries[2].state == CommandQueueEntry::State::TimedOut, "state is TimedOut after max retries");
    CHECK(timed_out.size() == 1, "one entry timed out");

    // purge
    q.entries[2].send_time = std::chrono::steady_clock::now() - std::chrono::seconds(60);
    q.purge(std::chrono::seconds(1));
    CHECK(q.entries.size() == 2, "purge removed old TimedOut entry");

    // classify_command
    CommandRetryPolicy policy;
    int max_tries, timeout_ms;
    classify_command(400, policy, max_tries, timeout_ms);  // ARM
    CHECK(policy == CommandRetryPolicy::NoRetry, "ARM is NoRetry");
    CHECK(max_tries == 1, "ARM max_tries is 1");

    classify_command(511, policy, max_tries, timeout_ms);  // SET_MESSAGE_INTERVAL
    CHECK(policy == CommandRetryPolicy::AlwaysRetry, "SET_MESSAGE_INTERVAL is AlwaysRetry");
    CHECK(max_tries == 3, "SET_MESSAGE_INTERVAL max_tries is 3");

    classify_command(999, policy, max_tries, timeout_ms);  // unknown
    CHECK(policy == CommandRetryPolicy::SafeToRetry, "unknown command is SafeToRetry");
    CHECK(max_tries == 3, "unknown max_tries is 3");
}

// ================================================================
// MissionTransferState 测试
// ================================================================
static void test_mission_transfer() {
    fprintf(stderr, "\n[Group] MissionTransferState\n");

    MissionTransferState state;
    CHECK(state.progress.state == mg::core::mission::MissionState::Idle, "starts in Idle");

    // record_send / record_recv
    state.record_send(1, 0, []() {});
    CHECK(state.retry_count == 0, "retry_count starts at 0");

    state.retry_count = 3;
    state.record_recv();
    CHECK(state.retry_count == 0, "record_recv resets retry_count");

    // check_timeout in Idle does nothing
    bool result = state.check_timeout();
    CHECK(result == false, "check_timeout in Idle returns false");

    // check_timeout with expired send_time
    state.progress.state = mg::core::mission::MissionState::Downloading;
    state.last_send_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    state.target_sys = 1;
    state.target_comp = 0;
    int resend_count = 0;
    state.resend_fn = [&]() { resend_count++; };
    state.retry_count = 0;

    result = state.check_timeout();
    CHECK(result == true, "check_timeout returns true on timeout");
    CHECK(resend_count == 1, "resend_fn was called");
    CHECK(state.retry_count == 1, "retry_count incremented");

    // 重试耗尽
    state.retry_count = 5;  // MISSION_MAX_RETRIES
    state.last_send_time = std::chrono::steady_clock::now() - std::chrono::seconds(5);
    result = state.check_timeout();
    CHECK(result == true, "check_timeout returns true on max retries");
    CHECK(state.progress.state == mg::core::mission::MissionState::Idle, "state reset to Idle");
    CHECK(!state.progress.error.empty(), "error message set");

    // reset
    state.reset();
    CHECK(state.download_total == 0, "reset clears download_total");
    CHECK(state.upload_total == 0, "reset clears upload_total");
    CHECK(state.retry_count == 0, "reset clears retry_count");
    CHECK(!state.resend_fn, "reset clears resend_fn");

    // select_transfer
    MissionTransferState mission, rally, fence;
    auto& r = select_transfer(mission, rally, fence, 2);  // RALLY
    CHECK(&r == &rally, "select_transfer returns rally for type 2");
    auto& f = select_transfer(mission, rally, fence, 1);  // FENCE
    CHECK(&f == &fence, "select_transfer returns fence for type 1");
    auto& m = select_transfer(mission, rally, fence, 0);  // MISSION
    CHECK(&m == &mission, "select_transfer returns mission for type 0");

    // find_uploading
    auto* found = find_uploading(mission, rally, fence);
    CHECK(found == nullptr, "find_uploading returns nullptr when none uploading");

    rally.progress.state = mg::core::mission::MissionState::Uploading;
    found = find_uploading(mission, rally, fence);
    CHECK(found == &rally, "find_uploading returns rally when uploading");
}

// ================================================================
// ParamCache 测试
// ================================================================
static void test_param_cache() {
    fprintf(stderr, "\n[Group] ParamCache\n");

    ParamCache pc;
    CHECK(pc.download_state == ParamCache::DownloadState::Idle, "starts in Idle");
    CHECK(pc.param_map.empty(), "param_map starts empty");

    // start_download
    pc.start_download(1, 1);
    CHECK(pc.download_state == ParamCache::DownloadState::WaitingList, "state is WaitingList");
    CHECK(pc.target_sys == 1, "target_sys set");
    CHECK(pc.pending_writes.empty(), "pending_writes cleared");

    // on_param_received
    pc.on_param_received(0);
    CHECK(pc.download_state == ParamCache::DownloadState::Receiving, "state is Receiving");
    CHECK(pc.list_retry_count == 0, "list_retry_count reset");

    // check_timeouts: no timeout yet
    std::vector<std::function<void()>> cbs;
    int action = pc.check_timeouts(cbs);
    CHECK(action == 0, "no action when not timed out");

    // check_timeouts: PARAM_REQUEST_LIST timeout
    pc.download_state = ParamCache::DownloadState::WaitingList;
    pc.last_send_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    pc.list_retry_count = 0;
    action = pc.check_timeouts(cbs);
    CHECK(action == 1, "action 1 (retry list) on timeout");
    CHECK(pc.list_retry_count == 1, "list_retry_count incremented");

    // check_timeouts: max retries exceeded
    pc.list_retry_count = 4;  // PARAM_LIST_MAX_RETRIES
    pc.last_send_time = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    action = pc.check_timeouts(cbs);
    CHECK(action == 3, "action 3 (give up) on max retries");
    CHECK(pc.download_state == ParamCache::DownloadState::Failed, "state is Failed");

    // missing_indices tracking
    pc.download_state = ParamCache::DownloadState::Receiving;
    pc.expected_param_count = 5;
    pc.missing_indices[2] = 0;
    pc.missing_indices[4] = 0;
    pc.on_param_received(2);
    CHECK(pc.missing_indices.count(2) == 0, "index 2 removed from missing");
    CHECK(pc.missing_indices.count(4) == 1, "index 4 still in missing");
}

// ================================================================
int main() {
    fprintf(stderr, "=== State Machine Unit Tests ===\n");

    test_command_queue();
    test_mission_transfer();
    test_param_cache();

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
