/**
 * test_mavlink_router.cc - shared-link MAVLink demultiplexing tests.
 *
 * These tests use an in-memory transport only. They verify that one physical
 * link can route complete MAVLink frames to independent adapters by
 * sysid/compid, discover unknown heartbeat systems, and retain a peer address
 * for replies without touching UDP port 14550 or a real vehicle.
 */

#include "core/transport/in_memory_link.h"
#include "core/transport/connection_manager.h"
#include "core/vehicle/multi_vehicle_manager.h"
#include "core/transport/link_session.h"

#include "ardupilotmega/mavlink.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

int g_passed = 0;
int g_failed = 0;

#define CHECK(cond, message) do { \
    if (cond) { g_passed++; fprintf(stderr, "  [PASS] %s\n", message); } \
    else { g_failed++; fprintf(stderr, "  [FAIL] %s\n", message); } \
} while (0)

std::vector<uint8_t> pack_msg(mavlink_message_t& message) {
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
    return std::vector<uint8_t>(buffer, buffer + length);
}

void pack_heartbeat(uint8_t system_id,
                    uint8_t component_id,
                    uint8_t vehicle_type,
                    mavlink_message_t& message) {
    mavlink_msg_heartbeat_pack(system_id, component_id, &message,
        vehicle_type, MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0, MAV_STATE_ACTIVE);
}

void pack_attitude(uint8_t system_id,
                   uint8_t component_id,
                   float roll,
                   mavlink_message_t& message) {
    mavlink_msg_attitude_pack(system_id, component_id, &message,
        0, roll, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

class PeerMemoryLink final : public mg::core::transport::ILink {
public:
    bool open() override {
        open_ = true;
        return true;
    }
    void close() override { open_ = false; }
    bool is_open() const override { return open_; }
    bool send(const uint8_t* data, size_t size) override {
        (void)data;
        (void)size;
        fallback_send_count_++;
        return true;
    }
    void set_receive_callback(ReceiveCallback cb) override {
        receive_cb_ = std::move(cb);
    }
    bool supports_peer_routing() const override { return true; }
    void set_peer_receive_callback(PeerReceiveCallback cb) override {
        peer_receive_cb_ = std::move(cb);
    }
    bool send_to(const mg::core::transport::LinkPeer& peer,
                 const uint8_t* data,
                 size_t size) override {
        (void)data;
        (void)size;
        send_to_peer_ = peer;
        send_to_count_++;
        return true;
    }
    void inject_incoming(const mg::core::transport::LinkPeer& peer,
                         const uint8_t* data,
                         size_t size) {
        if (peer_receive_cb_) peer_receive_cb_(peer, data, size);
    }
    void inject_regular(const uint8_t* data, size_t size) {
        if (receive_cb_) receive_cb_(data, size);
    }
    mg::core::transport::LinkPeer send_to_peer() const { return send_to_peer_; }
    int send_to_count() const { return send_to_count_; }
    int fallback_send_count() const { return fallback_send_count_; }

private:
    bool open_ = false;
    ReceiveCallback receive_cb_;
    PeerReceiveCallback peer_receive_cb_;
    mg::core::transport::LinkPeer send_to_peer_;
    int send_to_count_ = 0;
    int fallback_send_count_ = 0;
};

void test_router_demuxes_two_systems() {
    fprintf(stderr, "\n[Group] MavlinkRouter - two systems on one link\n");
    auto link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "shared session opens in-memory link");

    auto first = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    auto second = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    CHECK(session->add_vehicle(1, 1, 1, first), "register sysid 1");
    CHECK(session->add_vehicle(2, 2, 1, second), "register sysid 2");

    int first_heartbeats = 0;
    int second_heartbeats = 0;
    int first_attitudes = 0;
    int second_attitudes = 0;
    first->on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo&) {
        first_heartbeats++;
    });
    second->on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo&) {
        second_heartbeats++;
    });
    first->on_attitude([&](const mg::core::mavlink::AttitudeInfo&) {
        first_attitudes++;
    });
    second->on_attitude([&](const mg::core::mavlink::AttitudeInfo&) {
        second_attitudes++;
    });

    mavlink_message_t message;
    pack_heartbeat(1, 1, MAV_TYPE_QUADROTOR, message);
    auto bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());
    pack_attitude(1, 1, 1.25f, message);
    bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());

    pack_heartbeat(2, 1, MAV_TYPE_FIXED_WING, message);
    bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());
    pack_attitude(2, 1, -0.75f, message);
    bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());

    CHECK(first_heartbeats == 1, "sysid 1 receives its heartbeat");
    CHECK(second_heartbeats == 1, "sysid 2 receives its heartbeat");
    CHECK(first_attitudes == 1, "sysid 1 receives only its attitude");
    CHECK(second_attitudes == 1, "sysid 2 receives only its attitude");
}

void test_router_discovers_unknown_heartbeat() {
    fprintf(stderr, "\n[Group] MavlinkRouter - heartbeat discovery\n");
    auto link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "shared session opens for discovery");

    auto adapter = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    int discovered = 0;
    int heartbeats = 0;
    int attitudes = 0;
    session->set_discovery_callback(
        [&](const mg::core::mavlink::HeartbeatInfo& heartbeat,
            const mg::core::mavlink::MavlinkPeer& sender)
            -> std::optional<mg::core::mavlink::MavlinkDiscoveredTarget> {
            discovered++;
            CHECK(heartbeat.system_id == 3, "discovery reports sysid 3");
            CHECK(heartbeat.component_id == 1, "discovery reports autopilot component");
            return mg::core::mavlink::MavlinkDiscoveredTarget{3, adapter};
        });
    adapter->on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo&) {
        heartbeats++;
    });
    adapter->on_attitude([&](const mg::core::mavlink::AttitudeInfo&) {
        attitudes++;
    });

    mavlink_message_t message;
    pack_heartbeat(3, 1, MAV_TYPE_QUADROTOR, message);
    auto bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());
    pack_attitude(3, 1, 0.5f, message);
    bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());
    pack_heartbeat(3, 1, MAV_TYPE_QUADROTOR, message);
    bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());

    CHECK(discovered == 1, "unknown heartbeat discovers exactly once");
    CHECK(heartbeats == 2, "discovered adapter receives subsequent heartbeat");
    CHECK(attitudes == 1, "discovered adapter receives routed attitude");
}

void test_router_fragmented_and_coalesced_frames() {
    fprintf(stderr, "\n[Group] MavlinkRouter - fragmented frames\n");
    auto link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "shared session opens for fragmented frames");

    auto adapter = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    CHECK(session->add_vehicle(4, 4, 1, adapter), "register fragmented-test target");

    int heartbeats = 0;
    int attitudes = 0;
    adapter->on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo&) {
        heartbeats++;
    });
    adapter->on_attitude([&](const mg::core::mavlink::AttitudeInfo&) {
        attitudes++;
    });

    mavlink_message_t heartbeat;
    mavlink_message_t attitude;
    pack_heartbeat(4, 1, MAV_TYPE_QUADROTOR, heartbeat);
    pack_attitude(4, 1, 2.0f, attitude);
    auto heartbeat_bytes = pack_msg(heartbeat);
    auto attitude_bytes = pack_msg(attitude);
    std::vector<uint8_t> combined = heartbeat_bytes;
    combined.insert(combined.end(), attitude_bytes.begin(), attitude_bytes.end());

    const size_t split = heartbeat_bytes.size() / 2;
    link->inject_incoming(combined.data(), split);
    CHECK(heartbeats == 0 && attitudes == 0,
          "partial first frame does not route prematurely");
    link->inject_incoming(combined.data() + split, combined.size() - split);
    CHECK(heartbeats == 1, "fragmented heartbeat routes after completion");
    CHECK(attitudes == 1, "coalesced attitude routes in the same byte stream");
}

void test_router_rejects_duplicate_identity_and_removes_routes() {
    fprintf(stderr, "\n[Group] MavlinkRouter - identity ownership\n");
    auto link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "shared session opens for identity ownership");

    auto first = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    auto second = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    CHECK(session->add_vehicle(10, 5, 1, first), "first owner registers sysid 5");
    CHECK(!session->add_vehicle(11, 5, 1, second),
          "duplicate sysid/compid is rejected");

    session->remove_vehicle(10);
    CHECK(session->add_vehicle(11, 5, 1, second),
          "identity becomes available after owner removal");

    int first_count = 0;
    int second_count = 0;
    first->on_attitude([&](const mg::core::mavlink::AttitudeInfo&) {
        first_count++;
    });
    second->on_attitude([&](const mg::core::mavlink::AttitudeInfo&) {
        second_count++;
    });
    mavlink_message_t message;
    pack_attitude(5, 1, 1.0f, message);
    auto bytes = pack_msg(message);
    link->inject_incoming(bytes.data(), bytes.size());
    CHECK(first_count == 0, "removed owner no longer receives traffic");
    CHECK(second_count == 1, "new owner receives traffic after rebinding");
}

void test_link_session_remembers_peer_for_replies() {
    fprintf(stderr, "\n[Group] LinkSession - peer-aware replies\n");
    auto link = std::make_shared<PeerMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "peer-aware session opens");

    auto adapter = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    CHECK(session->add_vehicle(20, 6, 1, adapter), "register peer-aware route");

    const mg::core::transport::LinkPeer sender{"10.0.0.2", 14550};
    mavlink_message_t heartbeat;
    pack_heartbeat(6, 1, MAV_TYPE_FIXED_WING, heartbeat);
    auto bytes = pack_msg(heartbeat);
    link->inject_incoming(sender, bytes.data(), bytes.size());

    const uint8_t reply[] = {1, 2, 3, 4};
    CHECK(session->send(20, reply, sizeof(reply)), "reply send succeeds");
    CHECK(link->send_to_count() == 1, "peer-aware route uses send_to");
    CHECK(link->send_to_peer().host == sender.host &&
              link->send_to_peer().port == sender.port,
          "reply is addressed to the learned sender");
    CHECK(link->fallback_send_count() == 0,
          "peer-aware route does not fall back to a shared remote");
}

void test_router_preserves_mag_cal_progress_compatibility() {
    fprintf(stderr, "\n[Group] MavlinkRouter - ArduPilot compatibility frame\n");
    auto link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "shared session opens for compatibility frame");

    auto adapter = std::make_shared<mg::core::mavlink::MavlinkAdapter>();
    CHECK(session->add_vehicle(33, 9, 1, adapter),
          "register compatibility-frame target");
    int progress_count = 0;
    adapter->on_mag_cal_progress(
        [&](const mg::core::mavlink::MagCalibrationProgressInfo&) {
            progress_count++;
        });

    uint8_t completion_mask[10] = {};
    mavlink_message_t progress{};
    mavlink_msg_mag_cal_progress_pack(
        9, MAV_COMP_ID_AUTOPILOT1, &progress,
        0, 1, MAG_CAL_RUNNING_STEP_ONE, 1, 63, completion_mask,
        0.0f, 0.0f, 1.0f);
    auto bytes = pack_msg(progress);
    link->inject_incoming(bytes.data(), bytes.size());
    CHECK(progress_count == 1,
          "MAG_CAL_PROGRESS reaches the routed adapter");
}

void test_connection_manager_attaches_shared_session() {
    fprintf(stderr, "\n[Group] ConnectionManager - explicit shared route\n");
    auto link = std::make_shared<PeerMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "session opens before manager attach");

    mg::core::transport::ConnectionManager manager;
    CHECK(manager.attach_shared(session, 30, 7, 1),
          "manager attaches explicit shared identity");
    CHECK(manager.is_shared(), "manager reports shared lifecycle");
    CHECK(manager.state() == mg::core::transport::ConnectionState::WaitingHeartbeat,
          "shared manager waits for its first heartbeat");
    CHECK(manager.get_mavlink() != nullptr, "shared manager exposes its adapter");

    const mg::core::transport::LinkPeer sender{"10.0.0.3", 14551};
    mavlink_message_t heartbeat;
    pack_heartbeat(7, 1, MAV_TYPE_QUADROTOR, heartbeat);
    auto bytes = pack_msg(heartbeat);
    link->inject_incoming(sender, bytes.data(), bytes.size());
    CHECK(manager.state() == mg::core::transport::ConnectionState::Connected,
          "shared manager becomes connected on heartbeat");
    const auto first_heartbeat = manager.get_mavlink()->latest_heartbeat();
    CHECK(first_heartbeat.has_value() &&
              first_heartbeat->custom_mode == 0,
          "shared manager receives its routed heartbeat");
    CHECK(manager.send_heartbeat(), "shared manager sends GCS heartbeat");
    CHECK(link->send_to_peer().host == sender.host &&
              link->send_to_peer().port == sender.port,
          "shared reply uses the learned peer");

    manager.disconnect();
    CHECK(session->is_open(), "detaching one shared manager keeps link open");
    mavlink_msg_heartbeat_pack(7, 1, &heartbeat,
        MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 17, MAV_STATE_ACTIVE);
    bytes = pack_msg(heartbeat);
    link->inject_incoming(sender, bytes.data(), bytes.size());
    const auto after_detach = manager.get_mavlink() == nullptr
        ? std::optional<mg::core::mavlink::HeartbeatInfo>{}
        : manager.get_mavlink()->latest_heartbeat();
    CHECK(!after_detach.has_value() ||
              after_detach->custom_mode != 17,
          "detached shared manager stops receiving frames");
}

void test_connection_manager_binds_placeholder_identity() {
    fprintf(stderr, "\n[Group] ConnectionManager - placeholder identity\n");
    auto link = std::make_shared<PeerMemoryLink>();
    auto session = std::make_shared<mg::core::transport::LinkSession>(link);
    CHECK(session->open(), "session opens for placeholder bind");

    mg::core::transport::ConnectionManager manager;
    CHECK(manager.attach_shared(session, 31, 0, 0),
          "placeholder manager attaches without an identity");
    CHECK(manager.bind_shared_identity(8, 1),
          "placeholder binds its first heartbeat identity");

    const mg::core::transport::LinkPeer sender{"10.0.0.4", 14552};
    mavlink_message_t heartbeat;
    pack_heartbeat(8, 1, MAV_TYPE_FIXED_WING, heartbeat);
    auto bytes = pack_msg(heartbeat);
    link->inject_incoming(sender, bytes.data(), bytes.size());
    CHECK(manager.state() == mg::core::transport::ConnectionState::Connected,
          "bound placeholder becomes connected");
    CHECK(manager.get_link() == link.get(),
          "shared manager exposes the shared transport pointer");
}

void test_vehicle_merges_secondary_shared_route() {
    fprintf(stderr, "\n[Group] Vehicle - secondary shared route merge\n");
    auto primary_link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto secondary_link = std::make_shared<mg::core::transport::InMemoryLink>();
    auto primary_session = std::make_shared<mg::core::transport::LinkSession>(
        primary_link, "memory://primary");
    auto secondary_session = std::make_shared<mg::core::transport::LinkSession>(
        secondary_link, "memory://secondary");
    CHECK(primary_session->open(), "primary shared session opens");
    CHECK(secondary_session->open(), "secondary shared session opens");

    auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(50);
    CHECK(vehicle->attach_shared_session(primary_session, "memory://primary", 1, 1),
          "vehicle attaches primary shared route");
    CHECK(vehicle->attach_shared_route(secondary_session, 99, 1, 1),
          "vehicle attaches secondary shared route");
    CHECK(vehicle->link_manager().link_count() == 2,
          "both shared routes belong to the same vehicle");

    mavlink_message_t heartbeat;
    pack_heartbeat(1, 1, MAV_TYPE_QUADROTOR, heartbeat);
    auto bytes = pack_msg(heartbeat);
    primary_link->inject_incoming(bytes.data(), bytes.size());
    secondary_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(vehicle->is_connected(),
          "vehicle is connected when a shared route reports heartbeat");

    mavlink_message_t attitude;
    mavlink_msg_attitude_pack(1, 1, &attitude, 0, 1.25f, 0.0f, 0.0f,
                              0.0f, 0.0f, 0.0f);
    bytes = pack_msg(attitude);
    secondary_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(vehicle->cache().attitude.data.roll == 1.25f,
          "telemetry from secondary route reaches the shared vehicle cache");
    CHECK(vehicle->link_manager().statuses().size() == 2,
          "secondary route remains represented in link health");
    CHECK(vehicle->primary_route_id() == 50,
          "first shared route starts as command primary");
    const auto secondary_connection = vehicle->connection_for_route(99);
    CHECK(secondary_connection != nullptr,
          "secondary route has its own connection manager");
    vehicle->link_manager().remove_link(50);
    vehicle->refresh_primary_connection();
    CHECK(vehicle->primary_route_id() == 99,
          "command primary follows the healthy link after removal");
    CHECK(&vehicle->conn() == secondary_connection.get(),
          "conn() switches to the secondary command source");
}

void test_multi_vehicle_manager_shared_discovery() {
    fprintf(stderr, "\n[Group] MultiVehicleManager - shared sysid discovery\n");
    mg::core::vehicle::MultiVehicleManager manager;
    std::vector<mg::core::vehicle::Vehicle*> added;
    manager.set_vehicle_added_callback(
        [&](mg::core::vehicle::Vehicle* vehicle) { added.push_back(vehicle); });

    mg::core::transport::LinkConfig config;
    config.type = mg::core::transport::LinkType::InMemory;
    config.endpoint = "shared-test";
    auto* placeholder = manager.open_shared_vehicle(config);
    CHECK(placeholder != nullptr, "shared endpoint opens a placeholder vehicle");
    CHECK(manager.vehicle_count() == 1, "placeholder is registered before heartbeat");

    auto* memory_link = dynamic_cast<mg::core::transport::InMemoryLink*>(
        placeholder->conn().get_link());
    CHECK(memory_link != nullptr, "shared test uses the in-memory transport");

    mavlink_message_t heartbeat;
    pack_heartbeat(11, 1, MAV_TYPE_QUADROTOR, heartbeat);
    auto bytes = pack_msg(heartbeat);
    memory_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(placeholder->system_id() == 11,
          "placeholder binds the first discovered sysid");
    CHECK(manager.vehicle_count() == 1,
          "first heartbeat reuses the placeholder instead of creating a duplicate");

    pack_heartbeat(12, 1, MAV_TYPE_FIXED_WING, heartbeat);
    bytes = pack_msg(heartbeat);
    memory_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(manager.vehicle_count() == 2,
          "second sysid creates a second Vehicle on the same endpoint");
    CHECK(added.size() == 2,
          "vehicle-added callback fires for placeholder and discovered vehicle");
    CHECK(manager.get_vehicle_by_system_id(12) != nullptr,
          "discovered sysid is addressable by vehicle registry");

    const auto session = placeholder->conn().shared_session();
    CHECK(session != nullptr, "placeholder is attached to a shared LinkSession");
    CHECK(manager.close_vehicle(placeholder->vehicle_id()),
          "closing placeholder succeeds while another vehicle remains");
    CHECK(session->is_open(),
          "shared link stays open while another vehicle uses it");
    auto* second = manager.get_vehicle_by_system_id(12);
    CHECK(second != nullptr, "second vehicle survives placeholder close");
    CHECK(manager.close_vehicle(second->vehicle_id()),
          "closing last shared vehicle succeeds");
    CHECK(!session->is_open(),
          "shared link closes after the last vehicle detaches");
}

void test_managed_links_merge_same_sysid() {
    fprintf(stderr, "\n[Group] MultiVehicleManager - managed link merge\n");
    mg::core::vehicle::MultiVehicleManager manager;
    mg::core::transport::LinkConfig first_config;
    first_config.type = mg::core::transport::LinkType::InMemory;
    first_config.endpoint = "merge-primary";
    mg::core::transport::LinkConfig second_config;
    second_config.type = mg::core::transport::LinkType::InMemory;
    second_config.endpoint = "merge-secondary";

    const int first_link = manager.open_shared_link(first_config);
    const int second_link = manager.open_shared_link(second_config);
    CHECK(first_link > 0 && second_link > 0 && first_link != second_link,
          "managed links receive distinct link handles");
    const auto first_session = manager.link_session(first_link);
    const auto second_session = manager.link_session(second_link);
    CHECK(first_session && second_session, "managed link sessions are addressable");
    auto* first_wire = dynamic_cast<mg::core::transport::InMemoryLink*>(
        first_session->link());
    auto* second_wire = dynamic_cast<mg::core::transport::InMemoryLink*>(
        second_session->link());
    CHECK(first_wire && second_wire, "managed links use in-memory transport");

    mavlink_message_t heartbeat;
    pack_heartbeat(31, 1, MAV_TYPE_QUADROTOR, heartbeat);
    auto bytes = pack_msg(heartbeat);
    first_wire->inject_incoming(bytes.data(), bytes.size());
    CHECK(manager.vehicle_count() == 1,
          "first managed link creates one vehicle");
    second_wire->inject_incoming(bytes.data(), bytes.size());
    CHECK(manager.vehicle_count() == 1,
          "same sysid on second managed link merges into the existing vehicle");

    auto* vehicle = manager.get_vehicle_by_system_id(31);
    CHECK(vehicle != nullptr, "merged vehicle is registered");
    CHECK(vehicle->connection_count() == 2,
          "merged vehicle owns both managed connections");
    const auto links = manager.managed_links();
    CHECK(links.size() == 2, "both managed routes are reported to the UI seam");
    CHECK(links[0].vehicle_id == vehicle->vehicle_id() &&
              links[1].vehicle_id == vehicle->vehicle_id(),
          "both routes point at the canonical vehicle id");

    CHECK(manager.close_route(links[1].route_id),
          "closing one merged route succeeds");
    CHECK(manager.vehicle_count() == 1,
          "vehicle remains while one managed route is alive");
    CHECK(vehicle->connection_count() == 1,
          "closing one managed route leaves the other route attached");
    CHECK(manager.link_session(second_link) == nullptr,
          "the physical link closes when its last route is removed");
    CHECK(manager.close_link(first_link),
          "closing final managed link succeeds");
    CHECK(manager.vehicle_count() == 0,
          "vehicle is removed when its final managed link closes");
}

void test_shared_rediscovery_suppression() {
    fprintf(stderr, "\n[Group] MultiVehicleManager - rediscovery suppression\n");
    uint64_t now = 1000;
    mg::core::vehicle::MultiVehicleManager manager(
        [&]() { return now; }, 30000);

    mg::core::transport::LinkConfig config;
    config.type = mg::core::transport::LinkType::InMemory;
    config.endpoint = "suppression-test";
    auto* placeholder = manager.open_shared_vehicle(config);
    CHECK(placeholder != nullptr, "suppression test opens shared endpoint");
    auto* memory_link = dynamic_cast<mg::core::transport::InMemoryLink*>(
        placeholder->conn().get_link());
    CHECK(memory_link != nullptr, "suppression test uses in-memory link");

    mavlink_message_t heartbeat;
    pack_heartbeat(21, 1, MAV_TYPE_QUADROTOR, heartbeat);
    auto bytes = pack_msg(heartbeat);
    memory_link->inject_incoming(bytes.data(), bytes.size());
    pack_heartbeat(22, 1, MAV_TYPE_QUADROTOR, heartbeat);
    bytes = pack_msg(heartbeat);
    memory_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(manager.vehicle_count() == 2,
          "both sysids are discovered before suppression");

    auto* first = manager.get_vehicle_by_system_id(21);
    CHECK(first != nullptr, "first suppressed identity exists before close");
    CHECK(manager.close_vehicle(first->vehicle_id(), true),
          "manual close enables rediscovery suppression");

    // The second vehicle keeps the shared socket alive, so this heartbeat is
    // a real test of identity suppression rather than a closed-link test.
    pack_heartbeat(21, 1, MAV_TYPE_QUADROTOR, heartbeat);
    bytes = pack_msg(heartbeat);
    memory_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(manager.vehicle_count() == 1,
          "same sysid does not reappear inside the suppression window");
    CHECK(manager.get_vehicle_by_system_id(21) == nullptr,
          "suppressed identity stays absent");

    now += 30001;
    memory_link->inject_incoming(bytes.data(), bytes.size());
    CHECK(manager.vehicle_count() == 2,
          "suppressed identity can reappear after the window expires");
    CHECK(manager.get_vehicle_by_system_id(21) != nullptr,
          "expired suppression allows rediscovery");
}

} // namespace

int main() {
    fprintf(stderr, "=== Mavlink Router Tests ===\n");
    test_router_demuxes_two_systems();
    test_router_discovers_unknown_heartbeat();
    test_router_fragmented_and_coalesced_frames();
    test_router_rejects_duplicate_identity_and_removes_routes();
    test_link_session_remembers_peer_for_replies();
    test_router_preserves_mag_cal_progress_compatibility();
    test_connection_manager_attaches_shared_session();
    test_connection_manager_binds_placeholder_identity();
    test_vehicle_merges_secondary_shared_route();
    test_multi_vehicle_manager_shared_discovery();
    test_shared_rediscovery_suppression();
    test_managed_links_merge_same_sysid();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n",
            g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
