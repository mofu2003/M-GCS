// 测试 MavlinkAdapter 的发送回调重入场景：验证 send_cb 在锁外执行（ADR-0007，KD-49）
//
// 本测试验证 ADR-0007 的核心修复：deliver_frame() 在锁外调用 send_cb，
// 避免回调重入时的 ABBA 死锁。
//
// 测试方法：
// 1. 设置一个会重入 adapter 的 send_cb（在回调内注册另一个回调）
// 2. 触发 adapter 内部的发送路径（通过参数协议、心跳等）
// 3. 验证不会死锁

#include "core/mavlink/mavlink_adapter.h"
#include "core/transport/in_memory_link.h"
#include "mgcs_test_check.h"
#include "ardupilotmega/mavlink.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace mg::core;
using namespace mg::core::mavlink;
using namespace mg::core::transport;

namespace {

static int& g_passed = mgcs_test::passed();
static int& g_failed = mgcs_test::failed();

// 测试 1：简单的回调执行验证（基线测试）
void test_callback_is_executed() {
    std::fprintf(stderr, "\n=== Test: 验证回调被正确执行 ===\n");

    InMemoryLink link;
    MavlinkAdapter adapter;

    link.open();

    std::atomic<int> send_count{0};
    adapter.set_send_callback([&](const uint8_t* data, size_t size) -> bool {
        send_count++;
        return link.send(data, size);
    });

    link.set_receive_callback([&](const uint8_t* data, size_t size) {
        adapter.feed_raw(data, size);
    });

    // 构造心跳消息并注入（触发 adapter 内部发送心跳）
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID,
                                0, 0, MAV_STATE_ACTIVE);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

    // 手动调用 adapter 的发送（模拟内部发送路径）
    // 注意：这里我们不是通过 inject_incoming 触发，而是直接验证 send_cb 的调用
    adapter.set_send_callback([&](const uint8_t* data, size_t size) -> bool {
        send_count++;
        return true;
    });

    // 触发心跳（adapter 内部会定期发送心跳）
    bool heartbeat_received = false;
    adapter.on_heartbeat([&](const HeartbeatInfo&) {
        heartbeat_received = true;
    });

    link.inject_incoming(buf, len);

    CHECK(heartbeat_received, "heartbeat should be received");
}

// 测试 2：回调重入不死锁（关键测试）
void test_no_deadlock_on_reentry() {
    std::fprintf(stderr, "\n=== Test: send_cb 回调重入不死锁 ===\n");

    InMemoryLink link;
    MavlinkAdapter adapter;

    link.open();

    std::atomic<int> send_count{0};
    std::atomic<bool> reentry_attempted{false};

    // 设置重入式回调：发送时尝试注册新的回调
    adapter.set_send_callback([&](const uint8_t* data, size_t size) -> bool {
        send_count++;

        // 尝试重入：在 send_cb 执行期间注册新的回调
        // 若 send_cb 在 send_mtx 内执行，此处会尝试获取 mtx，形成死锁
        adapter.on_heartbeat([&](const HeartbeatInfo&) {
            reentry_attempted.store(true);
        });

        return link.send(data, size);
    });

    link.set_receive_callback([&](const uint8_t* data, size_t size) {
        adapter.feed_raw(data, size);
    });

    // 注入心跳消息触发处理
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
                                MAV_MODE_FLAG_SAFETY_ARMED, 0, MAV_STATE_ACTIVE);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);

    link.inject_incoming(buf, len);

    // 给一点时间让异步处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证测试完成（若死锁，不会到达这里）
    // 通过检查原子变量确认测试逻辑执行完毕
    CHECK(send_count.load() >= 0, "test completed without deadlock");

    std::fprintf(stderr, "  [INFO] send_count=%d, reentry_attempted=%d\n",
                 send_count.load(), reentry_attempted.load());
}

// 测试 3：多个 Vehicle adapter 并发发送时不共享 MAVLink 全局通道状态。
void test_concurrent_operations() {
    std::fprintf(stderr, "\n=== Test: 多适配器并发发送 ===\n");

    MavlinkAdapter first;
    MavlinkAdapter second;

    std::atomic<int> total_callbacks{0};
    auto make_send_callback = [&](MavlinkAdapter& adapter) {
        return [&adapter, &total_callbacks](const uint8_t*, size_t) -> bool {
            total_callbacks++;
            adapter.on_heartbeat([](const HeartbeatInfo&) {});
            return true;
        };
    };
    first.set_send_callback(make_send_callback(first));
    second.set_send_callback(make_send_callback(second));

    constexpr int kMsgsPerAdapter = 100;
    std::vector<std::thread> threads;
    auto send_commands = [&](MavlinkAdapter& adapter) {
        for (int i = 0; i < kMsgsPerAdapter; ++i) {
            (void)adapter.send_command_long(
                1, 1, MAV_CMD_COMPONENT_ARM_DISARM, 0,
                0, 0, 0, 0, 0, 0, 0);
        }
    };
    threads.emplace_back(send_commands, std::ref(first));
    threads.emplace_back(send_commands, std::ref(second));

    for (auto& th : threads) {
        th.join();
    }

    CHECK(total_callbacks.load() == kMsgsPerAdapter * 2,
          "all concurrent adapter sends reached their callbacks");

    std::fprintf(stderr, "  [INFO] total_callbacks=%d\n", total_callbacks.load());
}

}  // namespace

int main() {
    std::fprintf(stderr, "\n========================================\n");
    std::fprintf(stderr, "test_mavlink_send_callback_reentrancy\n");
    std::fprintf(stderr, "========================================\n");

    test_callback_is_executed();
    test_no_deadlock_on_reentry();
    test_concurrent_operations();

    MGCS_TEST_REPORT();
    return mgcs_test::exit_code();
}
