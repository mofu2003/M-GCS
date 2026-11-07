#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "ffi_vehicle_session.h"
#include "mock_vehicle_test_env.h"
#include "ffi_mission_internal.h"

#include <cstdio>
#include "cJSON.h"
#include "core/util/json.h"
#include "core/transport/link_session.h"
#include "mgcs_test_check.h"


// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


static void test_fallback_session_is_available() {
    fprintf(stderr, "\n[Group] fallback session\n");
    MockVehicleTestEnv env;

    auto session = mg::bridge::ffi::resolve_vehicle_session(env.core);
    CHECK(session.vehicle != nullptr, "fallback vehicle is available");
    CHECK(session.mavlink_lease != nullptr, "fallback mavlink lease is available");
    CHECK(session.connected == false, "fallback session starts disconnected");
}

static void test_live_vehicle_session_wins_when_selected() {
    fprintf(stderr, "\n[Group] live session routing\n");
    MockVehicleTestEnv env;

    auto selected_vehicle = std::make_shared<mg::core::vehicle::Vehicle>(7);
    env.core->vehicles.add_for_test(selected_vehicle);
    env.core->vehicles.set_active_vehicle(7);

    auto* selected = env.core->vehicles.get_vehicle(7);
    CHECK(selected != nullptr, "selected vehicle exists");

    auto session = mg::bridge::ffi::resolve_vehicle_session(env.core);
    CHECK(session.vehicle == selected, "selected live vehicle wins over fallback");
}

static void test_vehicle_registry_does_not_implicitly_change_focus() {
    fprintf(stderr, "\n[Group] vehicle registry focus isolation\n");
    MockVehicleTestEnv env;

    auto first = std::make_shared<mg::core::vehicle::Vehicle>(7);
    auto second = std::make_shared<mg::core::vehicle::Vehicle>(8);
    env.core->vehicles.add_for_test(first);
    env.core->vehicles.add_for_test(second);

    CHECK(mg_vehicle_get_active(env.core) == -1,
          "registering vehicle sessions does not select an active vehicle");

    env.core->vehicles.set_active_vehicle(7);
    CHECK(mg_vehicle_get_active(env.core) == 7,
          "active vehicle changes only after explicit selection");
    env.core->vehicles.add_for_test(std::make_shared<mg::core::vehicle::Vehicle>(9));
    CHECK(mg_vehicle_get_active(env.core) == 7,
          "registering another session does not steal existing focus");

    env.core->vehicles.close_vehicle(7);
    CHECK(mg_vehicle_get_active(env.core) == 8,
          "closing the focused vehicle selects a remaining session");
}

static void test_closing_focused_vehicle_selects_remaining_vehicle() {
    fprintf(stderr, "\n[Group] focused vehicle fallback\n");
    MockVehicleTestEnv env;

    auto first = std::make_shared<mg::core::vehicle::Vehicle>(21);
    auto second = std::make_shared<mg::core::vehicle::Vehicle>(22);
    env.core->vehicles.add_for_test(first);
    env.core->vehicles.add_for_test(second);
    env.core->vehicles.set_active_vehicle(21);

    CHECK(env.core->vehicles.close_vehicle(21),
          "closing focused vehicle succeeds");
    CHECK(mg_vehicle_get_active(env.core) == 22,
          "remaining vehicle receives focus after close");
}

// MV1 integration: independent Vehicle/ConnectionManager runtimes remain
// isolated even when input frames arrive interleaved and UI focus changes.
static void test_session_status_includes_position() {
    fprintf(stderr, "\n[Group] session status position fields\n");
    MockVehicleTestEnv env;
    auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(91);
    auto link = std::make_unique<mg::core::transport::InMemoryLink>();
    link->open();
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    auto* raw_link = link.get();
    mav->set_send_callback([raw_link](const uint8_t* bytes, size_t size) {
        return raw_link->send(bytes, size);
    });
    vehicle->conn().wire_for_test(std::move(link), std::move(mav));
    vehicle->init_subsystems();
    vehicle->wire_telemetry_callbacks();
    env.core->vehicles.add_for_test(vehicle);

    mavlink_global_position_int_t position{};
    position.lat = 399042000;
    position.lon = 1163970000;
    position.alt = 50000;
    position.relative_alt = 30000;
    position.hdg = 9000;
    mavlink_message_t message{};
    mavlink_msg_global_position_int_encode(1, 1, &message, &position);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(bytes, &message);
    vehicle->conn().shared_mavlink()->feed_raw(bytes, length);

    mavlink_home_position_t home_position{};
    home_position.latitude = 399000000;
    home_position.longitude = 1163900000;
    home_position.altitude = 45000;
    mavlink_message_t home_message{};
    mavlink_msg_home_position_encode(1, 1, &home_message, &home_position);
    const uint16_t home_length =
        mavlink_msg_to_send_buffer(bytes, &home_message);
    vehicle->conn().shared_mavlink()->feed_raw(bytes, home_length);

    char* status = mg_vehicle_get_session_status(env.core, 91);
    CHECK(status && strstr(status, "\"position_valid\":true"),
          "session status reports valid position");
    CHECK(status && strstr(status, "\"lat_deg\":39.9042"),
          "session status reports latitude");
    CHECK(status && strstr(status, "\"lon_deg\":116.397"),
          "session status reports longitude");
    CHECK(status && strstr(status, "\"heading_deg\":90"),
          "session status reports heading");
    CHECK(status && strstr(status, "\"home_valid\":true"),
          "session status reports home validity");
    {
        auto& cache = vehicle->cache();
        const auto saved_freshness = cache.freshness;
        cache.freshness = mg::core::telemetry::TelemetryFreshness{};
        char* retained = mg_vehicle_get_session_status(env.core, 91);
        CHECK(retained && strstr(retained, "\"home_valid\":true"),
              "received Home persists when its message freshness expires");
        if (retained) mg_free_string(retained);
        cache.freshness = saved_freshness;
    }
    CHECK(status && strstr(status, "\"home_lat_deg\":39.9"),
          "session status reports home latitude");
    CHECK(status && strstr(status, "\"mission_total\""),
          "session status reports mission summary fields");
    if (status) mg_free_string(status);

    mg::core::mission::MissionPlan plan;
    plan.valid = true;
    for (uint16_t index = 0; index < 2; ++index) {
        mg::core::mission::MissionItem item;
        item.seq = index;
        item.frame = 3;
        item.command = 16;
        item.x = 399000000 + static_cast<int32_t>(index) * 1000;
        item.y = 1163900000 + static_cast<int32_t>(index) * 1000;
        item.z = 50.0f + index;
        plan.items.push_back(item);
    }
    CHECK(vehicle->conn().shared_mavlink()->mission_upload(1, 1, plan),
          "vehicle mission upload starts for fleet route query");
    char* mission = mg_vehicle_get_session_mission(env.core, 91);
    CHECK(mission && strstr(mission, "\"success\":true") &&
              strstr(mission, "\"items\"") &&
              strstr(mission, "\"lat_deg\":39.9"),
          "per-vehicle mission route query returns geographic items");
    mg_free_string(mission);
}

static void test_independent_vehicle_telemetry_and_registry_lease() {
    fprintf(stderr, "\n[Group] multi-session telemetry and lifetime lease\n");
    MockVehicleTestEnv env;
    const auto attach = [&](int id) {
        auto v = std::make_shared<mg::core::vehicle::Vehicle>(id);
        auto link = std::make_unique<mg::core::transport::InMemoryLink>();
        link->open();
        auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
        auto* raw_link = link.get();
        mav->set_send_callback([raw_link](const uint8_t* bytes, size_t size) {
            return raw_link->send(bytes, size);
        });
        v->conn().wire_for_test(std::move(link), std::move(mav));
        v->init_subsystems();
        v->wire_telemetry_callbacks();
        env.core->vehicles.add_for_test(v);
        return v;
    };
    auto a = attach(31), b = attach(32);
    const auto feed_heartbeat = [](const std::shared_ptr<mg::core::vehicle::Vehicle>& v,
                                   uint8_t sysid) {
        mavlink_message_t msg{};
        mavlink_msg_heartbeat_pack(sysid, 1, &msg, MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
            4, MAV_STATE_ACTIVE);
        uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
        const auto size = mavlink_msg_to_send_buffer(bytes, &msg);
        v->conn().shared_mavlink()->feed_raw(bytes, size);
    };
    feed_heartbeat(a, 1);
    feed_heartbeat(b, 2);
    CHECK(mg_vehicle_get_active(env.core) == -1,
          "heartbeats on independent sessions do not steal focus");
    char* selected = mg_vehicle_select(env.core, 31);
    mg_free_string(selected);
    char* first = mg_vehicle_get_summary(env.core);
    CHECK(first && strstr(first, "\"system_id\":1"), "first session summary has sysid 1");
    mg_free_string(first);
    char* status_b = mg_connection_get_session_state(env.core, 32);
    CHECK(status_b && strstr(status_b, "\"session_id\":32"),
          "non-active session status is addressable");
    mg_free_string(status_b);
    CHECK(mg_vehicle_get_active(env.core) == 31, "non-active status read preserves UI focus");
    selected = mg_vehicle_select(env.core, 32);
    mg_free_string(selected);
    char* second = mg_vehicle_get_summary(env.core);
    CHECK(second && strstr(second, "\"system_id\":2"), "second session summary has sysid 2");
    mg_free_string(second);
    auto lease = env.core->vehicles.get_all_vehicles_shared();
    env.core->vehicles.close_vehicle(31);
    CHECK(lease.size() == 2 && lease[0]->vehicle_id() == 31,
          "registry snapshot retains vehicle while watchdog removes it");
    CHECK(mg_vehicle_get_active(env.core) == 32, "closing another link preserves active");
    CHECK(b->is_connected(), "closing first session does not disconnect second");
}

// Two independent Vehicle runtimes with mock autopilots. Targeted dispatch must
// enqueue commands on the selected vehicle without changing UI active focus.
struct TwoVehicleMockEnv {
    struct End {
        std::shared_ptr<mg::core::vehicle::Vehicle> vehicle;
        mg::core::transport::InMemoryLink* link = nullptr;
        std::unique_ptr<MockFlightController> mock;
    };
    mg_core_t* core = mg_create();
    std::vector<End> ends;
    std::atomic<bool> pumping{false};
    std::thread pump_thread;

    End& add(int id, uint8_t sysid,
             MockFlightController::Config cfg = MockFlightController::Config()) {
        cfg.system_id = sysid; cfg.component_id = 1;
        auto link = std::make_unique<mg::core::transport::InMemoryLink>(); link->open();
        auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
        auto* raw_link = link.get();
        mav->set_send_callback([raw_link](const uint8_t* bytes, size_t size) {
            return raw_link->send(bytes, size);
        });
        auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(id);
        vehicle->conn().wire_for_test(std::move(link), std::move(mav));
        vehicle->init_subsystems();
        vehicle->wire_telemetry_callbacks();
        core->vehicles.add_for_test(vehicle);
        ends.push_back({vehicle, raw_link, std::make_unique<MockFlightController>(*raw_link, cfg)});
        return ends.back();
    }
    void tick() {
        for (auto& end : ends) {
            end.mock->tick(20);
            for (int drain = 0; drain < 20; drain++) {
                auto outgoing = end.link->take_outgoing();
                if (outgoing.empty()) break;
                end.mock->on_gcs_data(outgoing.data(), outgoing.size());
            }
        }
    }
    bool wait_connected(int timeout_ms = 5000) {
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start).count() < timeout_ms) {
            tick();
            bool connected = true;
            for (auto& end : ends) connected = connected && end.vehicle->is_connected();
            if (connected) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
    void start_pump() {
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
    ~TwoVehicleMockEnv() {
        stop_pump();
        mg_destroy(core);
    }
};

static void test_vehicle_list_uses_each_vehicle_quadplane_state() {
    fprintf(stderr, "\n[Group] per-vehicle QuadPlane classification\n");
    TwoVehicleMockEnv env;

    MockFlightController::Config quadplane;
    quadplane.vehicle = MockFlightController::Plane;
    quadplane.mav_type_override = MAV_TYPE_FIXED_WING;
    env.add(61, 1, quadplane);

    MockFlightController::Config fixed_wing;
    fixed_wing.vehicle = MockFlightController::Plane;
    fixed_wing.mav_type_override = MAV_TYPE_FIXED_WING;
    env.add(62, 2, fixed_wing);
    env.ends[0].mock->add_param("Q_ENABLE", 1);

    CHECK(env.wait_connected(), "two fixed-wing heartbeat vehicles connect");
    for (auto& end : env.ends) {
        const auto mav = end.vehicle->shared_mavlink();
        CHECK(mav && mav->request_param_list(end.mock == env.ends[0].mock ? 1 : 2, 1),
              "each vehicle starts its parameter download");
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
    while (std::chrono::steady_clock::now() < deadline) {
        env.tick();
        bool ready = true;
        for (const auto& end : env.ends) {
            const auto mav = end.vehicle->shared_mavlink();
            ready = ready && mav && !mav->param_download_active() && mav->param_count() > 0;
        }
        if (ready) break;
    }

    env.core->vehicles.set_active_vehicle(61);
    char* list = mg_vehicle_list(env.core);
    CHECK(list && strstr(list, R"("vehicle_id":61)"),
          "vehicle list contains QuadPlane session");
    CHECK(list && strstr(list, R"("vehicle_id":62)"),
          "vehicle list contains fixed-wing session");
    CHECK(list && strstr(list, R"("vehicle_id":61,"system_id":1,"name":"","firmware_type":"ArduPilot","vehicle_type":"VTOL")"),
          "QuadPlane keeps its own VTOL classification");
    CHECK(list && strstr(list, R"("vehicle_id":62,"system_id":2,"name":"","firmware_type":"ArduPilot","vehicle_type":"FixedWing")"),
          "fixed-wing keeps its own FixedWing classification");
    mg_free_string(list);

    env.core->vehicles.set_active_vehicle(62);
    list = mg_vehicle_list(env.core);
    CHECK(list && strstr(list, R"("vehicle_id":61,"system_id":1,"name":"","firmware_type":"ArduPilot","vehicle_type":"VTOL")"),
          "switching focus does not reclassify QuadPlane");
    CHECK(list && strstr(list, R"("vehicle_id":62,"system_id":2,"name":"","firmware_type":"ArduPilot","vehicle_type":"FixedWing")"),
          "switching focus does not reclassify fixed-wing");
    mg_free_string(list);
}

static uint64_t command_ticket(char* result) {
    cJSON* json = result ? mg::core::json::parse(result) : nullptr;
    cJSON* ticket = json ? cJSON_GetObjectItemCaseSensitive(json, "ticket") : nullptr;
    const uint64_t id = cJSON_IsNumber(ticket) ? static_cast<uint64_t>(ticket->valuedouble) : 0;
    cJSON_Delete(json); mg_free_string(result); return id;
}

static std::string poll_command(mg_core_t* core, uint64_t ticket, int timeout_ms = 12000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        char* raw = mg_command_poll(core, ticket);
        const std::string response = raw ? raw : "";
        mg_free_string(raw);
        if (response.find("\"state\":\"complete\"") != std::string::npos) return response;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return "poll timed out";
}

static void test_nonblocking_commands_and_mission_rejection() {
    TwoVehicleMockEnv env;
    env.add(41, 1); env.add(42, 2);
    CHECK(env.wait_connected(), "async vehicles identified");
    env.core->vehicles.set_active_vehicle(41);
    const auto start = std::chrono::steady_clock::now();
    const auto ticket = command_ticket(mg_command_submit_for_vehicle(env.core, 42, R"({"command":"rtl"})"));
    CHECK(ticket > 0, "async submission returns ticket");
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(200),
          "submission does not wait for ACK");
    char* raw = mg_command_poll(env.core, ticket);
    CHECK(raw && strstr(raw, "\"state\":\"pending\""), "unanswered job is pending, not success");
    mg_free_string(raw);
    // 不同命令允许并发（QGC MavCommandQueue.cc:289-296 的 isPending(compId, command)
    // 规则）：等引导到位时再点返航不该被硬拒。
    raw = mg_command_submit_for_vehicle(env.core, 42, R"({"command":"set_mode","args":{"mode":"LOITER"}})");
    CHECK(raw && strstr(raw, "\"success\":true"), "different command may run concurrently");
    const auto mode_ticket = command_ticket(raw);  // 该 helper 内部已释放 raw
    CHECK(mode_ticket > 0, "concurrent command returns its own ticket");

    // 同一条命令重复下发仍然要拦：两条相同的命令无法分辨 ACK 归属。
    raw = mg_command_submit_for_vehicle(env.core, 42, R"({"command":"rtl"})");
    CHECK(raw && strstr(raw, "\"success\":false"), "duplicate command is still refused");
    CHECK(raw && strstr(raw, "same command") != nullptr,
          "duplicate refusal names the reason");
    mg_free_string(raw);
    env.start_pump();
    poll_command(env.core, mode_ticket);
    env.stop_pump();
    env.start_pump();
    auto done = poll_command(env.core, ticket);
    env.stop_pump();
    CHECK(done.find("\"result\":{\"success\":true") != std::string::npos, "accepted ACK completes async command");
    CHECK(mg_vehicle_get_active(env.core) == 41, "async command never steals focus");
    raw = mg_command_poll(env.core, ticket);
    CHECK(raw && strstr(raw, "\"success\":false"), "complete ticket consumed once"); mg_free_string(raw);
    env.ends[1].mock->set_next_cmd_result(MAV_RESULT_DENIED);
    const auto denied = command_ticket(mg_command_submit_for_vehicle(env.core, 42, R"({"command":"mission_start"})"));
    env.start_pump(); done = poll_command(env.core, denied); env.stop_pump();
    CHECK(done.find("denied") != std::string::npos && done.find("\"result_str\":\"denied\"") != std::string::npos,
          "MISSION_START rejection is propagated instead of send-success");
    env.ends[1].mock->set_cmd_no_response(true);
    const auto timeout = command_ticket(mg_command_submit_for_vehicle(env.core, 42, R"({"command":"mission_start"})"));
    env.start_pump(); done = poll_command(env.core, timeout); env.stop_pump();
    CHECK(done.find("timeout") != std::string::npos, "MISSION_START missing ACK times out");
}

static void test_async_mission_clear_targets_vehicle() {
    TwoVehicleMockEnv env;
    env.add(41, 1); env.add(42, 2);
    CHECK(env.wait_connected(), "clear vehicles identified");
    env.core->vehicles.set_active_vehicle(41);
    const auto ticket = command_ticket(mg_command_submit_for_vehicle(
        env.core, 42, R"({"command":"clear_mission"})"));
    CHECK(ticket > 0, "clear submission returns a ticket");
    env.ends[1].mock->set_mission_ack_type(MAV_MISSION_DENIED);
    env.start_pump();
    const auto done = poll_command(env.core, ticket);
    env.stop_pump();
    CHECK(env.ends[1].mock->mission_clear_received, "clear reaches the explicit target");
    CHECK(!env.ends[0].mock->mission_clear_received, "clear never reaches focused vehicle");
    CHECK(done.find("rejected") != std::string::npos, "clear preserves mission rejection");
    CHECK(mg_vehicle_get_active(env.core) == 41, "clear does not change focus");
}

static void test_mission_workflow_by_vehicle_type() {
    for (const auto type : {MAV_TYPE_QUADROTOR, MAV_TYPE_FIXED_WING, MAV_TYPE_VTOL_FIXEDROTOR}) {
        {
            TwoVehicleMockEnv env;
            env.add(41, 1);
            CHECK(env.wait_connected(), "workflow vehicle identified");
            auto& end = env.ends[0];
            bool armed = false;
            uint32_t mode = 0;
            std::vector<uint16_t> commands;
            std::vector<uint32_t> modes;
            auto heartbeat = [&] {
                mavlink_message_t msg{};
                mavlink_msg_heartbeat_pack(1, 1, &msg, type, MAV_AUTOPILOT_ARDUPILOTMEGA,
                    MAV_MODE_FLAG_CUSTOM_MODE_ENABLED | (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0), mode, MAV_STATE_ACTIVE);
                uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
                const auto n = mavlink_msg_to_send_buffer(bytes, &msg);
                end.link->inject_incoming(bytes, n);
            };
            heartbeat();
            const auto ticket = command_ticket(mg_command_submit_for_vehicle(env.core, 41,
                R"({"command":"start_mission","args":{"arm_if_needed":1}})"));
            std::atomic<bool> running{true};
            std::thread responder([&] {
                mavlink_message_t rx{}, msg{}; mavlink_status_t status{}, out{};
                while (running) {
                    const auto bytes = end.link->take_outgoing();
                    for (const auto byte : bytes) {
                        if (mavlink_frame_char_buffer(&rx, &status, byte, &msg, &out) != MAVLINK_FRAMING_OK ||
                            msg.msgid != MAVLINK_MSG_ID_COMMAND_LONG) continue;
                        mavlink_command_long_t command{}; mavlink_msg_command_long_decode(&msg, &command);
                        commands.push_back(command.command);
                        if (command.command == MAV_CMD_DO_SET_MODE) { mode = static_cast<uint32_t>(command.param2); modes.push_back(mode); }
                        if (command.command == MAV_CMD_COMPONENT_ARM_DISARM) armed = command.param1 != 0;
                        if (command.command == MAV_CMD_MISSION_START) mode = 3;
                        mavlink_message_t ack{};
                        mavlink_msg_command_ack_pack(1, 1, &ack, command.command, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
                        uint8_t packet[MAVLINK_MAX_PACKET_LEN];
                        const auto n = mavlink_msg_to_send_buffer(packet, &ack);
                        end.link->inject_incoming(packet, n);
                        heartbeat();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
            const auto done = poll_command(env.core, ticket);
            running = false; responder.join();
            CHECK(done.find("\"result\":{\"success\":true") != std::string::npos, "workflow completes with ACK and state");
            const auto expected = type == MAV_TYPE_QUADROTOR ? 4U : 10U;
            CHECK(!modes.empty() && modes.front() == expected, "vehicle type chooses correct mode before arm");
            CHECK(commands.size() >= 2 && commands[0] == MAV_CMD_DO_SET_MODE && commands[1] == MAV_CMD_COMPONENT_ARM_DISARM,
                  "mode is confirmed before arming");
            const bool sent_start = std::find(commands.begin(), commands.end(), MAV_CMD_MISSION_START) != commands.end();
            CHECK(sent_start == (type == MAV_TYPE_QUADROTOR),
                  "Plane/VTOL do not take the Copter MISSION_START path");
        }
    }
}

static void test_redundant_routes_inherit_signing_and_keep_operation_owner() {
    auto* core = mg_create();
    auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(91);
    auto a = std::make_shared<mg::core::transport::InMemoryLink>();
    auto b = std::make_shared<mg::core::transport::InMemoryLink>();
    auto first = std::make_shared<mg::core::transport::LinkSession>(a, "memory://first");
    auto second = std::make_shared<mg::core::transport::LinkSession>(b, "memory://second");
    first->open(); second->open();
    CHECK(vehicle->attach_shared_primary_route(first, 101, "memory://first", 1, 1), "attach first route");
    core->vehicles.add_for_test(vehicle); core->vehicles.set_active_vehicle(91);
    const auto old = vehicle->command_connection()->shared_mavlink();
    CHECK(old->mission_download(1, 1), "start transaction on first route");
    const auto owner = vehicle->operation_mavlink("mission");
    std::array<uint8_t, 32> key{}; key.fill(0x45);
    mg::core::mavlink::SigningCandidate candidate; candidate.name = "one"; candidate.key = key;
    CHECK(vehicle->set_signing_candidates({candidate}), "set candidates before second route");
    CHECK(vehicle->set_signing_key(key.data(), key.size(), true, 0, "one"), "set key before second route");
    CHECK(vehicle->attach_shared_route(second, 102, 1, 1), "attach second route after signing");
    vehicle->link_manager().remove_link(101); // deterministic primary switch, keep R1 registered
    vehicle->refresh_primary_connection();
    const auto current = vehicle->command_connection()->shared_mavlink();
    CHECK(current && current != old, "primary switched to second adapter");
    CHECK(current->signing_enabled() && current->signing_key_name() == "one" && current->signing_candidate_count() == 1,
          "new route inherits key and candidate policy");
    CHECK(vehicle->operation_mavlink("mission") == owner, "transaction remains on original adapter");
    char* raw = mg_operations_cancel_for_vehicle(core, 91, "mission");
    CHECK(raw && strstr(raw, "\"canceled\":true"), "cancel after failover reaches original owner"); mg_free_string(raw);
    raw = mg_operations_get_snapshot(core);
    CHECK(raw && strstr(raw, "\"cancelled\":true"), "terminal cancelled state visible after failover"); mg_free_string(raw);
    const auto connection = vehicle->command_connection();
    connection->record_heartbeat();
    // Keep a disconnected old primary and connected backup without a new heartbeat.
    old->clear_signing_key();
    vehicle->link_manager().add_link(101); vehicle->link_manager().remove_link(102);
    vehicle->refresh_primary_connection();
    vehicle->command_connection()->reset();
    CHECK(core->vehicles.remove_disconnected_vehicles() == 0,
          "disconnected primary cannot reap healthy backup vehicle");
    mg_destroy(core);
}

static bool has_command(const MockFlightController& mock, uint16_t command) {
    for (const auto& received : mock.received_cmds) {
        if (received.command == command) return true;
    }
    return false;
}

static void test_targeted_command_dispatch_isolated(uint8_t second_system_id) {
    fprintf(stderr, "\n[Group] explicit target command dispatch\n");
    TwoVehicleMockEnv env;
    env.add(41, 1);
    env.add(42, second_system_id);
    CHECK(env.wait_connected(), "two mock vehicles connect independently");
    CHECK(mg_vehicle_get_active(env.core) == -1, "mock connections do not steal focus");

    env.core->vehicles.set_active_vehicle(41);
    const int focus_before = mg_vehicle_get_active(env.core);
    env.start_pump();
    char* result = mg_command_execute_for_vehicle(
        env.core, 42, R"({"command":"rtl"})");
    env.stop_pump();

    CHECK(result && strstr(result, "\"success\":true"), "targeted RTL is accepted");
    mg_free_string(result);
    CHECK(mg_vehicle_get_active(env.core) == focus_before,
          "targeted command preserves UI active focus");
    CHECK(!has_command(*env.ends[0].mock, MAV_CMD_NAV_RETURN_TO_LAUNCH),
          "non-target vehicle receives no command");
    CHECK(has_command(*env.ends[1].mock, MAV_CMD_NAV_RETURN_TO_LAUNCH),
          "target vehicle receives command");

    result = mg_command_execute_for_vehicle(env.core, 999, R"({"command":"rtl"})");
    CHECK(result && strstr(result, "\"success\":false"), "missing target is rejected");
    mg_free_string(result);
    CHECK(mg_vehicle_get_active(env.core) == focus_before,
          "invalid target does not alter UI focus");
}

static void test_per_vehicle_mission_document_isolation() {
    fprintf(stderr, "\n[Group] per-session mission document ownership\n");
    MockVehicleTestEnv env;
    auto result = mg_mission_doc_create(env.core, R"({"metadata":{"name":"Offline draft"}})");
    mg_free_string(result);
    env.core->vehicles.add_for_test(std::make_shared<mg::core::vehicle::Vehicle>(71));
    env.core->vehicles.add_for_test(std::make_shared<mg::core::vehicle::Vehicle>(72));
    env.core->vehicles.set_active_vehicle(71);
    result = mg_mission_doc_get(env.core);
    CHECK(result && strstr(result, "Offline draft"), "first connection copies offline draft");
    mg_free_string(result);
    auto a = mg::bridge::mission_detail::ensure_document(env.core);
    result = mg_mission_doc_create(env.core, R"({"metadata":{"name":"Aircraft A"}})");
    mg_free_string(result);
    a = mg::bridge::mission_detail::ensure_document(env.core);
    env.core->vehicles.set_active_vehicle(72);
    result = mg_mission_doc_get(env.core);
    CHECK(result && !strstr(result, "Aircraft A"), "B does not inherit A task");
    mg_free_string(result);
    result = mg_mission_doc_create(env.core, R"({"metadata":{"name":"Aircraft B"}})");
    mg_free_string(result);
    auto b = mg::bridge::mission_detail::ensure_document(env.core);
    CHECK(a != b, "native documents have distinct ownership and undo history");
    env.core->vehicles.set_active_vehicle(71);
    result = mg_mission_doc_export(env.core);
    CHECK(result && strstr(result, "Aircraft A") && !strstr(result, "Aircraft B"),
          "A-B-A preserves original A document");
    mg_free_string(result);
    CHECK(mg::bridge::mission_detail::ensure_document(env.core) == a,
          "returning to A preserves its service/revision/undo history");
    result = mg_connection_close(env.core, 71);
    mg_free_string(result);
    CHECK(env.core->vehicle_mission_documents.count(71) == 0,
          "closing A releases only A document registry entry");
    result = mg_mission_doc_export(env.core);
    CHECK(result && strstr(result, "Aircraft B"), "closing A leaves B plan intact");
    mg_free_string(result);
    result = mg_connection_close(env.core, 72);
    mg_free_string(result);
    result = mg_mission_doc_export(env.core);
    CHECK(result && strstr(result, "Offline draft"), "offline draft remains separate after disconnect");
    mg_free_string(result);
}

static void test_target_scope_is_core_local_and_nested() {
    MockVehicleTestEnv first, second;
    auto v = std::make_shared<mg::core::vehicle::Vehicle>(81);
    first.core->vehicles.add_for_test(v);
    auto* second_default = mg::bridge::ffi::require_vehicle(second.core);
    {
        mg::bridge::ffi::VehicleTargetScope outer(first.core, v);
        CHECK(mg::bridge::ffi::require_vehicle(first.core) == v.get(), "scope selects target on owning core");
        CHECK(mg::bridge::ffi::require_vehicle(second.core) == second_default, "scope does not leak into other core");
        {
            mg::bridge::ffi::VehicleTargetScope inner(second.core, second.core->fallback_vehicle);
            CHECK(mg::bridge::ffi::require_vehicle(second.core) == second_default, "nested scope resolves its own core");
        }
        CHECK(mg::bridge::ffi::require_vehicle(first.core) == v.get(), "nested scope restores outer target");
    }
    CHECK(mg::bridge::ffi::require_vehicle(first.core) != v.get(), "scope restores default after exit");
    for (const auto* json : {
        R"({"command":"arm","args":{"force":1e100}})",
        R"({"command":"arm","args":{"force":1.5}})",
        R"({"command":"takeoff","args":{"altitude_m":1e309}})",
        R"({"command":"takeoff","args":{"altitude_m":1e100}})"}) {
        char* result = mg_command_execute_for_vehicle(first.core, 81, json);
        CHECK(result && strstr(result, "\"success\":false"), "out-of-range command numbers rejected without integer cast overflow");
        mg_free_string(result);
    }
}

static void test_session_scope_pins_identity_and_lifetime() {
    fprintf(stderr, "\n[Group] session snapshot identity and lifetime\n");
    MockVehicleTestEnv env;

    const auto attach = [&](int vehicle_id, uint8_t system_id, bool connected) {
        auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(vehicle_id);
        auto link = std::make_unique<mg::core::transport::InMemoryLink>();
        link->open();
        auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
        auto* raw_link = link.get();
        mav->set_send_callback([raw_link](const uint8_t* bytes, size_t size) {
            return raw_link->send(bytes, size);
        });
        vehicle->conn().wire_for_test(std::move(link), std::move(mav));
        vehicle->init_subsystems();
        if (connected) {
            vehicle->wire_telemetry_callbacks();
        } else {
            vehicle->conn().shared_mavlink()->on_heartbeat(
                [](const mg::core::mavlink::HeartbeatInfo&) {});
        }
        env.core->vehicles.add_for_test(vehicle);

        mavlink_message_t message{};
        mavlink_msg_heartbeat_pack(system_id, 1, &message, MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
            4, MAV_STATE_ACTIVE);
        uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
        const auto size = mavlink_msg_to_send_buffer(bytes, &message);
        vehicle->conn().shared_mavlink()->feed_raw(bytes, size);
        return vehicle;
    };

    auto first = attach(101, 11, true);
    auto second = attach(102, 22, false);
    env.core->vehicles.set_active_vehicle(101);

    {
        mg::bridge::ffi::VehicleSessionScope pinned(env.core);
        const auto initial = mg::bridge::ffi::resolve_vehicle_session(env.core);
        CHECK(initial.vehicle == first.get(), "scope captures the focused vehicle");
        CHECK(initial.vehicle_lease != nullptr, "scope owns the focused vehicle");
        CHECK(initial.mavlink_lease != nullptr, "scope owns the adapter lease");
        CHECK(initial.target_sys == 11 && initial.target_comp == 1,
              "scope captures target identity with the adapter");
        CHECK(initial.connected, "scope captures connected state with the adapter");

        env.core->vehicles.set_active_vehicle(102);
        const auto after_focus_change = mg::bridge::ffi::resolve_vehicle_session(env.core);
        CHECK(after_focus_change.vehicle == first.get(),
              "active vehicle changes do not replace a pinned session");
        CHECK(after_focus_change.target_sys == 11 && after_focus_change.target_comp == 1,
              "target identity remains from the pinned session");
        CHECK(after_focus_change.connected,
              "connection state remains from the pinned session");

        {
            mg::bridge::ffi::VehicleTargetScope target(env.core, second);
            const auto explicit_target = mg::bridge::ffi::resolve_vehicle_session(env.core);
            CHECK(explicit_target.vehicle == second.get(),
                  "explicit target scope selects the requested vehicle");
            CHECK(explicit_target.target_sys == 22 && explicit_target.target_comp == 1,
                  "explicit target scope captures its target identity");
            CHECK(!explicit_target.connected,
                  "explicit target scope captures its own connection state");
        }
        const auto restored = mg::bridge::ffi::resolve_vehicle_session(env.core);
        CHECK(restored.vehicle == first.get(),
              "explicit target scope restores the outer session");

        CHECK(env.core->vehicles.close_vehicle(101),
              "focused vehicle can be removed while the snapshot is live");
        const auto after_close = mg::bridge::ffi::resolve_vehicle_session(env.core);
        CHECK(after_close.vehicle == first.get() && after_close.mavlink_lease,
              "vehicle and adapter leases survive registry removal");
        CHECK(after_close.target_sys == 11 && after_close.connected,
              "removed session still exposes its captured identity and state");
    }

    CHECK(mg::bridge::ffi::resolve_vehicle_session(env.core).vehicle == second.get(),
          "session scope restores active focus after exit");
}

static void test_targeted_operation_cancel_isolated() {
    fprintf(stderr, "\n[Group] targeted operation cancellation\n");
    TwoVehicleMockEnv env;
    env.add(41, 1);
    env.add(42, 2);
    CHECK(env.wait_connected(), "two mock vehicles connect for cancellation");
    char* fleet_summary = mg_vehicle_list(env.core);
    CHECK(fleet_summary && strstr(fleet_summary, "\"flight_mode\"") &&
              strstr(fleet_summary, "\"battery_remaining_pct\"") &&
              strstr(fleet_summary, "\"gps_fix_type\"") &&
              strstr(fleet_summary, "\"link_count\"") &&
              strstr(fleet_summary, "\"healthy_link_count\"") &&
              strstr(fleet_summary, "\"communication_lost\""),
          "fleet summary exposes mode, battery, GPS and link health fields");
    CHECK(fleet_summary && strstr(fleet_summary, "\"heartbeat_fresh\":"),
          "fleet summary exposes heartbeat freshness state");
    mg_free_string(fleet_summary);

    const auto first_mav = env.ends[0].vehicle->shared_mavlink();
    const auto second_mav = env.ends[1].vehicle->shared_mavlink();
    CHECK(first_mav->request_param_list(1, 1),
          "first vehicle starts parameter download");
    CHECK(first_mav->param_download_active(),
          "first vehicle parameter download is active");
    char* result = mg_operations_get_snapshot(env.core);
    CHECK(result && strstr(result, "\"retry_count\"") &&
              strstr(result, "\"missing_count\"") &&
              strstr(result, "\"failed_count\""),
          "parameter operation exposes retry and recovery counters");
    mg_free_string(result);
    env.core->vehicles.set_active_vehicle(42);

    result = mg_operations_cancel_for_vehicle(
        env.core, 41, "params");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "targeted cancel reports success for the explicit vehicle");
    mg_free_string(result);
    CHECK(first_mav->param_download_cancelled(),
          "target vehicle parameter download is cancelled");
    CHECK(!second_mav->param_download_cancelled(),
          "non-target vehicle operation is untouched");

    env.core->vehicles.set_active_vehicle(41);
    result = mg_operations_get_snapshot(env.core);
    CHECK(result && strstr(result, "\"params\"") &&
              strstr(result, "\"cancelled\":true"),
          "targeted snapshot reads the inactive vehicle operation");
    mg_free_string(result);
    env.core->vehicles.set_active_vehicle(42);

    CHECK(first_mav->mission_download(1, 1),
          "first vehicle starts mission download");
    CHECK(first_mav->mission_get_progress().state !=
              mg::core::mission::MissionState::Idle,
          "first vehicle mission transfer is active");
    result = mg_operations_cancel_for_vehicle(env.core, 41, "mission");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "targeted mission cancel succeeds");
    mg_free_string(result);
    CHECK(first_mav->mission_get_progress().result ==
              MAV_MISSION_OPERATION_CANCELLED,
          "target mission transfer is cancelled");
    CHECK(second_mav->mission_get_progress().state ==
              mg::core::mission::MissionState::Idle,
          "non-target mission transfer remains idle");

    CHECK(first_mav->rally_download(1, 1),
          "first vehicle starts rally download");
    result = mg_operations_cancel_for_vehicle(env.core, 41, "rally");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "targeted rally cancel succeeds");
    mg_free_string(result);
    CHECK(first_mav->rally_get_progress().result ==
              MAV_MISSION_OPERATION_CANCELLED,
          "target rally transfer is cancelled");
    CHECK(second_mav->rally_get_progress().state ==
              mg::core::mission::MissionState::Idle,
          "non-target rally transfer remains idle");

    CHECK(first_mav->fence_download(1, 1),
          "first vehicle starts fence download");
    result = mg_operations_cancel_for_vehicle(env.core, 41, "fence");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "targeted fence cancel succeeds");
    mg_free_string(result);
    CHECK(first_mav->fence_get_progress().result ==
              MAV_MISSION_OPERATION_CANCELLED,
          "target fence transfer is cancelled");
    CHECK(second_mav->fence_get_progress().state ==
              mg::core::mission::MissionState::Idle,
          "non-target fence transfer remains idle");

    CHECK(env.ends[0].vehicle->calibration().start(
              mg::core::calibration::CalibrationType::Gyroscope),
          "first vehicle starts calibration");
    CHECK(env.ends[0].vehicle->calibration().is_active(),
          "first vehicle calibration is active");
    result = mg_operations_cancel_for_vehicle(env.core, 41, "calibration");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "targeted calibration cancel succeeds");
    mg_free_string(result);
    // 取消是异步的：请求受理后到飞控确认前仍是 cancelling（S8）。
    CHECK(env.ends[0].vehicle->calibration().is_active() &&
              env.ends[0].vehicle->calibration().get_status().stage ==
                  mg::core::calibration::CalibrationStage::Cancelling,
          "target calibration stays active in cancelling state until the ACK");
    env.tick();
    env.tick();
    CHECK(!env.ends[0].vehicle->calibration().is_active(),
          "target calibration stops after the vehicle confirms the cancel");
    CHECK(env.ends[0].vehicle->calibration().get_status().stage ==
              mg::core::calibration::CalibrationStage::Cancelled,
          "target calibration reports cancelled after the ACK");
    CHECK(!env.ends[1].vehicle->calibration().is_active(),
          "non-target calibration remains idle");

    CHECK(first_mav->log_download_start(1, 1, 0, ""),
          "first vehicle starts log download");
    CHECK(first_mav->log_download_active(),
          "first vehicle log download is active");
    result = mg_operations_cancel_for_vehicle(env.core, 41, "log");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "targeted log cancellation succeeds");
    mg_free_string(result);
    CHECK(!first_mav->log_download_active(),
          "target log download stops");
    CHECK(!second_mav->log_download_active(),
          "non-target log download remains idle");

    result = mg_operations_cancel_for_vehicle(env.core, 999, "params");
    CHECK(result && strstr(result, "\"success\":false"),
          "targeted cancel rejects a missing vehicle");
    mg_free_string(result);
}

static void test_targeted_operation_timeout_isolated() {
    fprintf(stderr, "\n[Group] targeted operation timeout isolation\n");
    TwoVehicleMockEnv env;
    env.add(51, 1);
    env.add(52, 2);
    CHECK(env.wait_connected(), "two mock vehicles connect for timeout isolation");

    const auto first_mav = env.ends[0].vehicle->shared_mavlink();
    const auto second_mav = env.ends[1].vehicle->shared_mavlink();
    CHECK(first_mav->mission_download(1, 1),
          "first vehicle starts mission download");
    CHECK(first_mav->mission_get_progress().state !=
              mg::core::mission::MissionState::Idle,
          "first mission transfer is active");

    std::this_thread::sleep_for(std::chrono::milliseconds(1600));
    first_mav->tick();
    CHECK(first_mav->mission_transfer_retry_count() == 1,
          "first mission transfer retries after its own timeout");

    CHECK(second_mav->rally_download(2, 1),
          "second vehicle starts rally download after first timeout");
    second_mav->tick();
    CHECK(second_mav->rally_transfer_retry_count() == 0,
          "second rally transfer does not consume first vehicle's timeout");
    CHECK(second_mav->rally_get_progress().state !=
              mg::core::mission::MissionState::Idle,
          "second rally transfer remains active");

    char* result = mg_operations_cancel_for_vehicle(env.core, 51, "mission");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "timed-out first mission is cancelled explicitly");
    mg_free_string(result);
    CHECK(second_mav->rally_get_progress().state !=
              mg::core::mission::MissionState::Idle,
          "cancelling first vehicle leaves second rally active");

    result = mg_operations_cancel_for_vehicle(env.core, 52, "rally");
    CHECK(result && strstr(result, "\"success\":true") &&
              strstr(result, "\"canceled\":true"),
          "second rally is cancelled independently");
    mg_free_string(result);
}

static void test_ffi_signing_control() {
    fprintf(stderr, "\n[Group] FFI MAVLink signing control\n");
    mg_core_t* core = mg_create();
    auto link = std::make_unique<mg::core::transport::InMemoryLink>();
    link->open();
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    auto* raw_link = link.get();
    mav->set_send_callback([raw_link](const uint8_t* data, size_t size) {
        return raw_link->send(data, size);
    });
    auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(91);
    vehicle->conn().wire_for_test(std::move(link), std::move(mav));
    vehicle->init_subsystems();
    vehicle->wire_telemetry_callbacks();
    core->vehicles.add_for_test(vehicle);

    const char* key_hex =
        "00112233445566778899aabbccddeeff"
        "102030405060708090a0b0c0d0e0f001";
    char* result = mg_vehicle_set_signing_candidates(
        core, 91,
        R"([{"name":"KeyAlpha","key_hex":"00112233445566778899aabbccddeeff102030405060708090a0b0c0d0e0f001","timestamp_ticks":0}])");
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI installs named signing candidates");
    mg_free_string(result);
    result = mg_vehicle_get_signing_status(core, 91);
    CHECK(result && strstr(result, "\"candidate_count\":1"),
          "signing status reports candidate count");
    mg_free_string(result);

    result = mg_vehicle_set_signing_key(core, 91, key_hex, 1, 0);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI enables signing on an explicit vehicle");
    mg_free_string(result);
    CHECK(vehicle->signing_enabled(), "vehicle reports signing enabled");

    result = mg_vehicle_get_signing_status(core, 91);
    CHECK(result && strstr(result, "\"enabled\":true") &&
              strstr(result, "\"status\":\"none\""),
          "signing status reports enabled state");
    mg_free_string(result);

    CHECK(vehicle->shared_mavlink()->send_heartbeat(), "signed vehicle heartbeat sends");
    const auto outgoing = raw_link->take_outgoing();
    CHECK(outgoing.size() > 2 &&
              (outgoing[2] & MAVLINK_IFLAG_SIGNED) != 0,
          "FFI-configured vehicle emits signed MAVLink");

    result = mg_vehicle_send_setup_signing(core, 91, key_hex, 123456);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI sends SETUP_SIGNING without enabling signing first");
    mg_free_string(result);
    const auto setup_bytes = raw_link->take_outgoing();
    mavlink_message_t setup_message{};
    mavlink_status_t setup_status{};
    bool setup_parsed = false;
    for (const auto byte : setup_bytes) {
        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &setup_message,
                               &setup_status)) {
            setup_parsed = true;
            break;
        }
    }
    CHECK(setup_parsed && setup_message.msgid == MAVLINK_MSG_ID_SETUP_SIGNING,
          "SETUP_SIGNING frame is a valid MAVLink setup message");
    CHECK(setup_parsed &&
              (setup_message.incompat_flags & MAVLINK_IFLAG_SIGNED) == 0,
          "SETUP_SIGNING is sent unsigned");
    if (setup_parsed) {
        mavlink_setup_signing_t setup{};
        mavlink_msg_setup_signing_decode(&setup_message, &setup);
        bool key_matches = true;
        for (size_t index = 0; index < sizeof(setup.secret_key); ++index) {
            const int high = index * 2;
            const int low = high + 1;
            auto nibble = [](char value) -> uint8_t {
                if (value >= '0' && value <= '9') return value - '0';
                if (value >= 'a' && value <= 'f') return value - 'a' + 10;
                return value - 'A' + 10;
            };
            const uint8_t expected = static_cast<uint8_t>(
                (nibble(key_hex[high]) << 4) | nibble(key_hex[low]));
            if (setup.secret_key[index] != expected) key_matches = false;
        }
        CHECK(key_matches, "SETUP_SIGNING carries the requested key");
        CHECK(setup.initial_timestamp == 123456,
              "SETUP_SIGNING carries the requested timestamp");
    }

    result = mg_vehicle_clear_signing_key(core, 91);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI clears signing key");
    mg_free_string(result);
    CHECK(!vehicle->signing_enabled(), "vehicle reports signing disabled");

    result = mg_vehicle_clear_signing_candidates(core, 91);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI clears signing candidates");
    mg_free_string(result);
    result = mg_vehicle_get_signing_status(core, 91);
    CHECK(result && strstr(result, "\"candidate_count\":0"),
          "clearing candidates resets the detector");
    mg_free_string(result);

    result = mg_vehicle_set_signing_candidates(core, 91, R"({"bad":true})");
    CHECK(result && strstr(result, "\"success\":false"),
          "invalid signing candidate payload is rejected");
    mg_free_string(result);

    result = mg_vehicle_set_signing_key(core, 91, "bad-key", 1, 0);
    CHECK(result && strstr(result, "\"success\":false"),
          "invalid signing key is rejected");
    mg_free_string(result);

    mg_destroy(core);
}

int main() {
    fprintf(stderr, "=== Bridge Vehicle Session Tests ===\n");
    test_per_vehicle_mission_document_isolation();
    test_target_scope_is_core_local_and_nested();
    test_session_scope_pins_identity_and_lifetime();
    test_session_status_includes_position();
    test_independent_vehicle_telemetry_and_registry_lease();
    test_targeted_command_dispatch_isolated(2);
    test_targeted_command_dispatch_isolated(1);
    test_nonblocking_commands_and_mission_rejection();
    test_async_mission_clear_targets_vehicle();
    test_mission_workflow_by_vehicle_type();
    test_redundant_routes_inherit_signing_and_keep_operation_owner();
    test_fallback_session_is_available();
    test_live_vehicle_session_wins_when_selected();
    test_vehicle_registry_does_not_implicitly_change_focus();
    test_closing_focused_vehicle_selects_remaining_vehicle();
    test_ffi_signing_control();
    test_targeted_operation_cancel_isolated();
    test_targeted_operation_timeout_isolated();
    test_vehicle_list_uses_each_vehicle_quadplane_state();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
