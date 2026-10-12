/**
 * test_in_memory.cc - Test FFI layer using InMemoryLink (no hardware required)
 */

#include "core/transport/connection_manager.h"
#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/platform/platform_mutex.h"
#include "ffi_api.h"
#include "core/telemetry/json_builder.h"
#include "ardupilotmega/mavlink.h"
#include <iostream>
#include <cstring>
#include "mgcs_test_check.h"

// Build a valid MAVLink v1 HEARTBEAT in code (avoids CRC issues)
static std::vector<uint8_t> build_heartbeat() {
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_SAFETY_ARMED | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        0, MAV_STATE_ACTIVE);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    return std::vector<uint8_t>(buf, buf + len);
}

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& tests_passed = mgcs_test::passed();
static int& tests_failed = mgcs_test::failed();


int main() {
    std::cout << "=== InMemory Link Test ===" << std::endl;

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    char* ver = mg_get_version();
    CHECK(ver != nullptr, "version");
    mg_free_string(ver);

    char* state = mg_connection_get_state(core);
    CHECK(strstr(state, "false") != nullptr, "initial disconnected");
    mg_free_string(state);

    // InMemoryLink basic ops
    mg::core::transport::InMemoryLink link;
    CHECK(!link.is_open(), "not open initially");
    CHECK(link.open(), "open succeeds");
    CHECK(link.is_open(), "is open");

    uint8_t test_byte = 0x42;
    link.send(&test_byte, 1);
    auto sent = link.take_outgoing();
    CHECK(sent.size() == 1 && sent[0] == 0x42, "send captures");
    CHECK(link.take_outgoing().empty(), "take clears");

    // MavlinkAdapter + InMemoryLink
    mg::core::mavlink::MavlinkAdapter mavlink;
    bool heartbeat_received = false;
    mavlink.on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo& hb) {
        heartbeat_received = true;
        std::cout << "  Heartbeat: sysid=" << (int)hb.system_id
                  << " type=" << (int)hb.vehicle_type
                  << " autopilot=" << (int)hb.autopilot_type
                  << " mode=" << hb.flight_mode << std::endl;
    });

    link.set_receive_callback([&](const uint8_t* data, size_t size) {
        mavlink.feed_raw(data, size);
    });
    mavlink.set_send_callback([&](const uint8_t* data, size_t size) -> bool {
        return link.send(data, size);
    });

    // Build and inject a valid heartbeat
    auto hb_bytes = build_heartbeat();
    link.inject_incoming(hb_bytes.data(), hb_bytes.size());
    CHECK(heartbeat_received, "heartbeat received");

    auto hb = mavlink.latest_heartbeat();
    CHECK(hb.has_value() && hb->system_id == 1, "sysid=1");
    CHECK(hb.has_value() && hb->vehicle_type == 2, "type=2 (quadrotor)");
    CHECK(hb.has_value() && hb->autopilot_type == 3, "autopilot=3 (ardupilot)");
    CHECK(hb.has_value() && hb->armed, "armed");
    CHECK(hb.has_value() && std::string(hb->flight_mode) == "Stabilize", "flight_mode=Stabilize");

    // ConnectionManager with InMemoryLink
    mg::core::transport::ConnectionManager conn;
    auto test_link = std::make_unique<mg::core::transport::InMemoryLink>();
    test_link->open();
    auto test_mavlink = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    test_link->set_receive_callback([mav = test_mavlink.get()](const uint8_t* data, size_t size) {
        mav->feed_raw(data, size);
    });
    test_mavlink->set_send_callback([lk = test_link.get()](const uint8_t* data, size_t size) -> bool {
        return lk->send(data, size);
    });
    conn.wire_for_test(
        std::unique_ptr<mg::core::transport::ILink>(test_link.release()),
        std::move(test_mavlink));
    // Note: manual wiring bypasses connect() state machine.
    // Real usage always goes through connect().
    // Skip connected assertion for manually-wired test.
    const auto adapter = conn.shared_mavlink();
    CHECK(adapter != nullptr, "shared_mavlink");
    CHECK(conn.get_link() != nullptr, "get_link");

    auto* inmem = dynamic_cast<mg::core::transport::InMemoryLink*>(conn.get_link());
    CHECK(inmem != nullptr, "dynamic_cast InMemoryLink");
    if (inmem) {
        inmem->inject_incoming(hb_bytes.data(), hb_bytes.size());
        auto hb2 = adapter->latest_heartbeat();
        CHECK(hb2.has_value() && hb2->system_id == 1, "heartbeat via conn");
        CHECK(hb2.has_value() && hb2->armed, "armed via conn");
        CHECK(hb2.has_value() && std::string(hb2->flight_mode) == "Stabilize", "mode via conn");
    }

    // Send heartbeat GCS -> FC (tests the send path)
    adapter->send_heartbeat();
    auto outgoing = inmem->take_outgoing();
    CHECK(outgoing.size() > 6, "GCS heartbeat sent");

    conn.disconnect();
    CHECK(!conn.is_connected(), "disconnected");
    // T2：destroy 之后不能断言（对象已释放），把断言放在销毁之前，并给出真实条件。
    CHECK(core != nullptr, "core exists before destroy");
    mg_destroy(core);

    std::cout << "\n=== " << tests_passed << " passed, " << tests_failed << " failed ===" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
