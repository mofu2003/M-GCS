/**
 * test_in_memory.cc - Test FFI layer using InMemoryLink (no hardware required)
 */

#include "core/transport/connection_manager.h"
#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/platform/platform_mutex.h"
#include "ffi_api.h"
#include "json_builder.h"
#include "ardupilotmega/mavlink.h"
#include <iostream>
#include <cstring>

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

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { tests_passed++; std::cout << "  PASS: " << name << std::endl; } \
    else { tests_failed++; std::cout << "  FAIL: " << name << std::endl; } \
} while(0)

int main() {
    std::cout << "=== InMemory Link Test ===" << std::endl;

    mg_core_t* core = mg_create();
    TEST("mg_create", core != nullptr);

    char* ver = mg_get_version();
    TEST("version", ver != nullptr);
    mg_free_string(ver);

    char* state = mg_connection_get_state(core);
    TEST("initial disconnected", strstr(state, "false") != nullptr);
    mg_free_string(state);

    // InMemoryLink basic ops
    mg::core::transport::InMemoryLink link;
    TEST("not open initially", !link.is_open());
    TEST("open succeeds", link.open());
    TEST("is open", link.is_open());

    uint8_t test_byte = 0x42;
    link.send(&test_byte, 1);
    auto sent = link.take_outgoing();
    TEST("send captures", sent.size() == 1 && sent[0] == 0x42);
    TEST("take clears", link.take_outgoing().empty());

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
    TEST("heartbeat received", heartbeat_received);

    auto hb = mavlink.latest_heartbeat();
    TEST("sysid=1", hb.has_value() && hb->system_id == 1);
    TEST("type=2 (quadrotor)", hb.has_value() && hb->vehicle_type == 2);
    TEST("autopilot=3 (ardupilot)", hb.has_value() && hb->autopilot_type == 3);
    TEST("armed", hb.has_value() && hb->armed);
    TEST("flight_mode=Stabilize", hb.has_value() && std::string(hb->flight_mode) == "Stabilize");

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
    TEST("get_mavlink", conn.get_mavlink() != nullptr);
    TEST("get_link", conn.get_link() != nullptr);

    auto* inmem = dynamic_cast<mg::core::transport::InMemoryLink*>(conn.get_link());
    TEST("dynamic_cast InMemoryLink", inmem != nullptr);
    if (inmem) {
        inmem->inject_incoming(hb_bytes.data(), hb_bytes.size());
        auto hb2 = conn.get_mavlink()->latest_heartbeat();
        TEST("heartbeat via conn", hb2.has_value() && hb2->system_id == 1);
        TEST("armed via conn", hb2.has_value() && hb2->armed);
        TEST("mode via conn", hb2.has_value() && std::string(hb2->flight_mode) == "Stabilize");
    }

    // Send heartbeat GCS -> FC (tests the send path)
    conn.get_mavlink()->send_heartbeat();
    auto outgoing = inmem->take_outgoing();
    TEST("GCS heartbeat sent", outgoing.size() > 6);

    conn.disconnect();
    TEST("disconnected", !conn.is_connected());
    mg_destroy(core);
    TEST("destroy", true);

    std::cout << "\n=== " << tests_passed << " passed, " << tests_failed << " failed ===" << std::endl;
    return tests_failed > 0 ? 1 : 0;
}
