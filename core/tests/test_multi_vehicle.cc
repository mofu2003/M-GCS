/**
 * test_multi_vehicle.cc - Unit tests for MultiVehicleManager
 *
 * Tests: open/close vehicle lifecycle, active vehicle switching,
 *        auto-discovery via heartbeat, concurrent access
 * Uses InMemoryLink for testing without hardware.
 */
#include "core/vehicle/multi_vehicle_manager.h"
#include "core/vehicle/vehicle_link_manager.h"
#include "core/transport/in_memory_link.h"
#include "core/transport/connection_manager.h"
#include "core/transport/link_session.h"
#include "core/platform/platform_mutex.h"
#include "ardupilotmega/mavlink.h"
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


int main() {
    fprintf(stderr, "=== MultiVehicleManager Unit Tests ===\n\n");

    // 1. Construction
    {
        fprintf(stderr, "[Group] Construction\n");
        mg::core::vehicle::MultiVehicleManager mgr;
        CHECK(mgr.vehicle_count() == 0, "initial vehicle count is 0");
        CHECK(mgr.active_vehicle() == nullptr, "no active vehicle initially");
    }

    // 2. Direct vehicle creation and lifecycle
    {
        fprintf(stderr, "[Group] Direct Vehicle lifecycle\n");
        mg::core::vehicle::MultiVehicleManager mgr;

        // Manually create a vehicle with InMemoryLink
        mg::core::transport::LinkDescriptor desc;
        desc.id = "test";
        desc.type = "in_memory";
        desc.endpoint = "test://mock";

        auto link = std::make_unique<mg::core::transport::InMemoryLink>();
        auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();

        auto vehicle = std::make_unique<mg::core::vehicle::Vehicle>(0);
        vehicle->conn().wire_for_test(std::move(link), std::move(mav));
        vehicle->set_identity(1, 1, "ArduPilot", "Copter");

        CHECK(vehicle->system_id() == 1, "vehicle system_id is 1");
        CHECK(vehicle->vehicle_id() == 0, "vehicle_id is 0");
        CHECK(vehicle->is_connected() == false, "not connected (no heartbeat yet)");

        // Note: record_heartbeat changes ConnectionManager state, not Vehicle state
        // Vehicle state is set by wire_telemetry_callbacks which needs a real link
        CHECK(vehicle->state() == mg::core::vehicle::VehicleState::Disconnected,
              "vehicle state remains Disconnected without full wire-up");
    }

    // 3. Multi-vehicle isolation
    {
        fprintf(stderr, "[Group] Multi-vehicle cache isolation\n");
        mg::core::vehicle::Vehicle v1(1);
        mg::core::vehicle::Vehicle v2(2);

        v1.set_identity(1, 1, "ArduPilot", "Copter");
        v2.set_identity(2, 1, "PX4", "Copter");

        // Each vehicle has its own cache
        v1.cache().gps.data.lat = 399042000;  // 39.9042 deg * 1e7
        v2.cache().gps.data.lat = 312000000;  // 31.2 deg * 1e7
        CHECK(v1.cache().gps.data.lat == 399042000, "vehicle 1 GPS lat preserved (int32)");
        CHECK(v2.cache().gps.data.lat == 312000000, "vehicle 2 GPS lat preserved (int32)");

        // Each vehicle has its own subsystems
        CHECK(&v1.calibration() != &v2.calibration(), "different CalibrationService instances");
        CHECK(&v1.flight_modes() != &v2.flight_modes(), "different FlightModeManager instances");
        CHECK(&v1.geofence() != &v2.geofence(), "different GeofenceManager instances");
    }

    // 4. Concurrent vehicle access
    {
        fprintf(stderr, "[Group] Concurrent vehicle access\n");
        mg::core::vehicle::MultiVehicleManager mgr;
        std::atomic<int> count{0};

        // Simulate concurrent heartbeat processing
        auto worker = [&](int id) {
            mg::core::vehicle::Vehicle v(id);
            v.set_identity(static_cast<uint8_t>(id), 1, "AP", "Copter");
            v.cache().attitude.data.roll = (float)id;
            count++;
        };

        std::vector<std::thread> threads;
        for (int i = 1; i <= 8; i++) threads.emplace_back(worker, i);
        for (auto& t : threads) t.join();

        CHECK(count.load() == 8, "8 concurrent vehicle creations succeeded");
    }

    // 5. Vehicle state machine
    {
        fprintf(stderr, "[Group] Vehicle state transitions\n");
        mg::core::vehicle::Vehicle v(0);
        CHECK(v.state() == mg::core::vehicle::VehicleState::Disconnected,
              "initial state is Disconnected");
    }

    // 6. Subsystem per-vehicle independence
    {
        fprintf(stderr, "[Group] Per-vehicle subsystem independence\n");
        mg::core::vehicle::Vehicle v1(1);
        mg::core::vehicle::Vehicle v2(2);

        // Modify one vehicle's subsystem data
        // CalibrationService uses its own mutex, so modifications don't cross
        CHECK(&v1.ring_buffer() != &v2.ring_buffer(), "different ring buffer instances");
        CHECK(&v1.sensor_decoder() != &v2.sensor_decoder(), "different sensor decoder instances");
        CHECK(&v1.mavlink_log() != &v2.mavlink_log(), "different mavlink log instances");
    }

    // 7. Lightweight telemetry bootstrap after heartbeat identity
    {
        fprintf(stderr, "[Group] Telemetry bootstrap\n");

        auto link = std::make_unique<mg::core::transport::InMemoryLink>();
        link->open();
        auto* raw_link = link.get();
        auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();

        auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(7);
        vehicle->conn().wire_for_test(std::move(link), std::move(mav));
        vehicle->wire_telemetry_callbacks();

        mavlink_message_t hb{};
        mavlink_msg_heartbeat_pack(
            1,
            1,
            &hb,
            MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_ARDUPILOTMEGA,
            MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
            0,
            MAV_STATE_STANDBY);
        uint8_t hb_buf[MAVLINK_MAX_PACKET_LEN]{};
        const uint16_t hb_len = mavlink_msg_to_send_buffer(hb_buf, &hb);
        raw_link->inject_incoming(hb_buf, hb_len);

        auto outgoing = raw_link->take_outgoing();
        bool requested_attitude = false;
        bool requested_vfr_hud = false;
        bool requested_servo_output = false;
        int servo_output_interval_us = -1;
        bool requested_rc_channels = false;

        mavlink_message_t msg{};
        mavlink_status_t status{};
        for (uint8_t byte : outgoing) {
            if (!mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status)) continue;
            if (msg.msgid != MAVLINK_MSG_ID_COMMAND_LONG) continue;

            mavlink_command_long_t cmd{};
            mavlink_msg_command_long_decode(&msg, &cmd);
            if (cmd.command != MAV_CMD_SET_MESSAGE_INTERVAL) continue;

            const int requested_msg_id = static_cast<int>(cmd.param1);
            if (requested_msg_id == MAVLINK_MSG_ID_ATTITUDE) requested_attitude = true;
            if (requested_msg_id == MAVLINK_MSG_ID_VFR_HUD) requested_vfr_hud = true;
            if (requested_msg_id == MAVLINK_MSG_ID_SERVO_OUTPUT_RAW) {
                requested_servo_output = true;
                servo_output_interval_us = static_cast<int>(cmd.param2);
            }
            if (requested_msg_id == MAVLINK_MSG_ID_RC_CHANNELS) requested_rc_channels = true;
        }

        CHECK(requested_attitude, "heartbeat identity requests ATTITUDE stream");
        CHECK(requested_vfr_hud, "heartbeat identity requests VFR_HUD stream");
        CHECK(requested_servo_output, "heartbeat identity requests SERVO_OUTPUT_RAW stream");
        CHECK(servo_output_interval_us == 50000,
              "SERVO_OUTPUT_RAW stream interval is 50000 us (20 Hz)");
        CHECK(requested_rc_channels, "heartbeat identity requests RC_CHANNELS stream");
        CHECK(vehicle->state() == mg::core::vehicle::VehicleState::Connected,
              "heartbeat identity marks vehicle Connected");
    }

    // 8. Closing the active vehicle selects a remaining vehicle, and a
    //    watchdog-reset vehicle is reaped instead of remaining as a ghost.
    {
        fprintf(stderr, "[Group] Active fallback and ghost session pruning\n");
        mg::core::vehicle::MultiVehicleManager mgr;

        auto make_waiting_vehicle = [](int id) {
            auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(id);
            auto link = std::make_unique<mg::core::transport::InMemoryLink>();
            auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
            link->open();
            vehicle->conn().wire_for_test(std::move(link), std::move(mav));
            return vehicle;
        };

        auto first = make_waiting_vehicle(11);
        auto second = make_waiting_vehicle(12);
        mgr.add_for_test(first);
        mgr.add_for_test(second);
        mgr.set_active_vehicle(11);

        CHECK(mgr.close_vehicle(11), "closing active vehicle succeeds");
        CHECK(mgr.active_vehicle() != nullptr, "remaining vehicle becomes active");
        CHECK(mgr.active_vehicle()->vehicle_id() == 12,
              "the remaining vehicle is selected as replacement");

        auto connected = make_waiting_vehicle(13);
        mgr.add_for_test(connected);

        const size_t removed = mgr.remove_disconnected_vehicles();
        CHECK(removed == 0, "connected vehicle is not reaped while waiting for heartbeat");
        CHECK(mgr.vehicle_count() == 2, "connected vehicle remains registered");

        mg::core::vehicle::MultiVehicleManager ghost_mgr;
        ghost_mgr.add_for_test(std::make_shared<mg::core::vehicle::Vehicle>(14));
        CHECK(ghost_mgr.remove_disconnected_vehicles() == 1,
              "disconnected ghost session is reaped");
        CHECK(ghost_mgr.vehicle_count() == 0,
              "ghost session no longer blocks vehicle id reuse");
    }

    // 9. Per-vehicle link health and primary selection
    {
        fprintf(stderr, "[Group] Vehicle link health manager\n");
        uint64_t now = 1000;
        mg::core::vehicle::VehicleLinkManager links([&]() { return now; }, 3500);
        CHECK(links.add_link(101), "add primary candidate link 101");
        CHECK(links.add_link(102), "add secondary link 102");
        CHECK(!links.add_link(102), "duplicate route is not added twice");
        CHECK(links.record_heartbeat(101), "record heartbeat on link 101");
        now = 2000;
        CHECK(links.record_heartbeat(102), "record heartbeat on link 102");
        CHECK(links.primary_route_id() == 101,
              "healthy current primary is retained before failure");

        now = 5000;
        links.tick();
        CHECK(!links.communication_lost(),
              "one healthy route prevents total communication loss");
        CHECK(links.primary_route_id() == 102,
              "primary switches to the remaining healthy route");
        auto statuses = links.statuses();
        CHECK(statuses.size() == 2, "both link statuses are reported");
        CHECK(statuses[0].route_id == 101 && statuses[0].comm_lost,
              "failed route is marked communication-lost");
        CHECK(statuses[1].route_id == 102 && statuses[1].primary,
              "healthy route is marked primary");

        now = 7000;
        links.tick();
        CHECK(links.communication_lost(),
              "all routes lost produces total communication loss");
        CHECK(links.primary_route_id() == -1,
              "no primary remains when all routes are lost");

        now = 7100;
        CHECK(links.add_link(103), "add recovery route 103");
        CHECK(!links.communication_lost(),
              "new healthy route clears total communication loss");
        CHECK(links.primary_route_id() == 103,
              "recovery route becomes primary");
        CHECK(links.remove_link(103), "remove recovery route");
        CHECK(links.communication_lost(),
              "removing last healthy route restores communication loss");
        links.clear();
        CHECK(links.link_count() == 0 && !links.communication_lost(),
              "clear resets link health state");
    }

    // 9. KD-15: Vehicle::state 与传输层状态同源，断链后不残留 Connected
    {
        fprintf(stderr, "[Group] Vehicle state mirrors transport state (KD-15)\n");
        auto link = std::make_shared<mg::core::transport::InMemoryLink>();
        auto session = std::make_shared<mg::core::transport::LinkSession>(
            link, "memory://kd15");
        session->open();

        auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(99);
        CHECK(vehicle->attach_shared_primary_route(
                  session, 99, "memory://kd15", 1, 1),
              "attach shared primary route (installs state callback)");
        CHECK(vehicle->state() == mg::core::vehicle::VehicleState::WaitingHeartbeat,
              "attach leaves vehicle waiting for heartbeat");

        mavlink_message_t hb{};
        mavlink_msg_heartbeat_pack(1, 1, &hb, MAV_TYPE_QUADROTOR,
                                   MAV_AUTOPILOT_ARDUPILOTMEGA,
                                   MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0,
                                   MAV_STATE_STANDBY);
        uint8_t hb_buf[MAVLINK_MAX_PACKET_LEN]{};
        const uint16_t hb_len = mavlink_msg_to_send_buffer(hb_buf, &hb);
        link->inject_incoming(hb_buf, hb_len);
        CHECK(vehicle->state() == mg::core::vehicle::VehicleState::Connected,
              "heartbeat mirrors vehicle to Connected");

        // 显式断开只经过传输层状态回调这条路径；若 Vehicle 状态不与
        // 传输层同源，这里会残留 Connected（KD-15 描述的状态漂移）。
        vehicle->conn().disconnect();
        CHECK(vehicle->state() == mg::core::vehicle::VehicleState::Disconnected,
              "disconnect resets vehicle state (no stale Connected)");
    }

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
