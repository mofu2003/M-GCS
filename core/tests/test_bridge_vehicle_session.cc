#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "ffi_vehicle_session.h"
#include "mock_vehicle_test_env.h"
#include "ffi_mission_internal.h"

#include <cstdio>


static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static void test_fallback_session_is_available() {
    fprintf(stderr, "\n[Group] fallback session\n");
    MockVehicleTestEnv env;

    auto session = mg::bridge::ffi::resolve_vehicle_session(env.core);
    CHECK(session.vehicle != nullptr, "fallback vehicle is available");
    CHECK(session.mavlink != nullptr, "fallback mavlink is available");
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
    vehicle->conn().get_mavlink()->feed_raw(bytes, length);

    char* status = mg_vehicle_get_session_status(env.core, 91);
    CHECK(status && strstr(status, "\"position_valid\":true"),
          "session status reports valid position");
    CHECK(status && strstr(status, "\"lat_deg\":39.9042"),
          "session status reports latitude");
    CHECK(status && strstr(status, "\"lon_deg\":116.397"),
          "session status reports longitude");
    CHECK(status && strstr(status, "\"heading_deg\":90"),
          "session status reports heading");
    if (status) mg_free_string(status);
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
        v->conn().get_mavlink()->feed_raw(bytes, size);
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

    End& add(int id, uint8_t sysid) {
        MockFlightController::Config cfg; cfg.system_id = sysid; cfg.component_id = 1;
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

static bool has_command(const MockFlightController& mock, uint16_t command) {
    for (const auto& received : mock.received_cmds) {
        if (received.command == command) return true;
    }
    return false;
}

static void test_targeted_command_dispatch_isolated() {
    fprintf(stderr, "\n[Group] explicit target command dispatch\n");
    TwoVehicleMockEnv env;
    env.add(41, 1);
    env.add(42, 2);
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
        mg::bridge::ffi::VehicleTargetScope outer(first.core, v.get());
        CHECK(mg::bridge::ffi::require_vehicle(first.core) == v.get(), "scope selects target on owning core");
        CHECK(mg::bridge::ffi::require_vehicle(second.core) == second_default, "scope does not leak into other core");
        {
            mg::bridge::ffi::VehicleTargetScope inner(second.core, second_default);
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
    char* result = mg_vehicle_set_signing_key(core, 91, key_hex, 1, 0);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI enables signing on an explicit vehicle");
    mg_free_string(result);
    CHECK(vehicle->signing_enabled(), "vehicle reports signing enabled");

    result = mg_vehicle_get_signing_status(core, 91);
    CHECK(result && strstr(result, "\"enabled\":true") &&
              strstr(result, "\"status\":\"none\""),
          "signing status reports enabled state");
    mg_free_string(result);

    CHECK(vehicle->mavlink()->send_heartbeat(), "signed vehicle heartbeat sends");
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
    test_session_status_includes_position();
    test_independent_vehicle_telemetry_and_registry_lease();
    test_targeted_command_dispatch_isolated();
    test_fallback_session_is_available();
    test_live_vehicle_session_wins_when_selected();
    test_vehicle_registry_does_not_implicitly_change_focus();
    test_closing_focused_vehicle_selects_remaining_vehicle();
    test_ffi_signing_control();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
