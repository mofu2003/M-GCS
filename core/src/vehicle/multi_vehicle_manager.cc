/*
 * multi_vehicle_manager.cc - 多飞行器运行时管理器实现
 *
 * 使用 shared_ptr 管理 Vehicle 生命周期。
 * 返回 raw pointer 给 FFI 层（同步调用安全，shared_ptr 保证存活）。
 */
#include "core/vehicle/multi_vehicle_manager.h"
#include "core/firmware/firmware_adapter.h"
#include "core/transport/link_factory.h"
#include "core/log/logging.h"

#include "ardupilotmega/mavlink.h"
#include <algorithm>
#include <chrono>
#include <thread>

namespace mg::core::vehicle {

MultiVehicleManager::MultiVehicleManager(NowMs now_ms,
                                         uint64_t rediscovery_suppression_ms)
    : now_ms_(std::move(now_ms)),
      rediscovery_suppression_ms_(rediscovery_suppression_ms) {}

MultiVehicleManager::~MultiVehicleManager() { set_gcs_heartbeat_enabled(false); }

uint64_t MultiVehicleManager::now_ms() const {
    if (now_ms_) return now_ms_();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

Vehicle* MultiVehicleManager::open_vehicle(const mg::core::transport::LinkConfig& config) {
    std::shared_ptr<Vehicle> vehicle;
    int vid = -1;
    {
        mg::core::platform::LockGuard lk(mtx_);
        vid = next_vehicle_id();
        vehicle = std::make_shared<Vehicle>(vid);
    }


    // 在锁外执行 connect()
    // Keep the full URI so Vehicle::connect can dispatch on scheme and baud.
    bool ok = vehicle->connect(config.to_uri());
    if (!ok) {
        MGC_LOG_ERROR("[MultiVehicle] Failed to open vehicle %d on %s", vid, config.endpoint.c_str());
        vehicle->disconnect();
        bool failed_empty = false;
        {
            mg::core::platform::LockGuard lk(mtx_);
            for (auto it = vehicles_.begin(); it != vehicles_.end(); ++it) {
                if ((*it)->vehicle_id() == vid) {
                    vehicles_.erase(it);
                    failed_empty = vehicles_.empty();
                    break;
                }
            }
        }
        if (failed_empty) {
            set_gcs_heartbeat_enabled(false);
        }
        return nullptr;
    }

    // Publish only after connect() has established a live transport state.
    // This prevents the heartbeat reaper from seeing a freshly-created
    // Vehicle while it is still in the Disconnected pre-connect window.
    bool first_vehicle = false;
    VehicleAddedCallback added_cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        first_vehicle = vehicles_.empty();
        vehicles_.push_back(vehicle);
        added_cb = on_vehicle_added_;
    }

    if (first_vehicle) {
        set_gcs_heartbeat_enabled(true);
    }

    if (config.type == mg::core::transport::LinkType::Serial) {
        mg::core::transport::SerialPortDiscovery discovery;
        const auto ports = discovery.enumerate_ports();
        for (const auto& port : ports) {
            if (port.port_name == vehicle->info().serial_port) {
                vehicle->set_usb_board_name(port.board_name);
                break;
            }
        }
    }

    MGC_LOG_INFO("[MultiVehicle] Opened vehicle %d on %s (%s)", vid, config.endpoint.c_str(), config.type_string().c_str());

    Vehicle* raw = vehicle.get();
    if (added_cb) added_cb(raw);
    return raw;
}

Vehicle* MultiVehicleManager::open_shared_vehicle(
    const mg::core::transport::LinkConfig& config) {
    const std::string endpoint = config.to_uri();
    auto link = mg::core::transport::LinkFactory::create(config);
    if (!link) {
        MGC_LOG_ERROR("[MultiVehicle] Failed to create shared link %s",
                      endpoint.c_str());
        return nullptr;
    }

    auto session = std::make_shared<mg::core::transport::LinkSession>(
        std::shared_ptr<mg::core::transport::ILink>(std::move(link)), endpoint);
    const std::weak_ptr<mg::core::transport::LinkSession> weak_session = session;
    session->set_discovery_callback(
        [this, weak_session](
            const mg::core::mavlink::HeartbeatInfo& heartbeat,
            const mg::core::mavlink::MavlinkPeer& peer)
            -> std::optional<mg::core::mavlink::MavlinkDiscoveredTarget> {
            if (const auto session = weak_session.lock()) {
                on_shared_heartbeat(session, heartbeat, peer);
            }
            return std::nullopt;
        });

    std::shared_ptr<Vehicle> vehicle;
    bool first_vehicle = false;
    VehicleAddedCallback added_cb;
    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        if (link_sessions_.count(endpoint) != 0) return nullptr;
        ignored_shared_identities_.erase(endpoint);
        const int vid = next_vehicle_id();
        link_sessions_[endpoint] = session;

        vehicle = std::make_shared<Vehicle>(vid);
        if (!vehicle->attach_shared_session(session, endpoint, 0, 0)) {
            MGC_LOG_ERROR("[MultiVehicle] Failed to attach placeholder %d on %s",
                          vid, endpoint.c_str());
            link_sessions_.erase(endpoint);
            return nullptr;
        }

        first_vehicle = vehicles_.empty();
        vehicles_.push_back(vehicle);
        vehicle_sessions_[vid] = session;
        added_cb = on_vehicle_added_;
    }

    if (!session->open()) {
        vehicle->disconnect();
        session->close();
        {
            mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
            mg::core::platform::LockGuard lk(mtx_);
            vehicles_.erase(
                std::remove(vehicles_.begin(), vehicles_.end(), vehicle),
                vehicles_.end());
            vehicle_sessions_.erase(vehicle->vehicle_id());
            link_sessions_.erase(endpoint);
        }
        if (first_vehicle) set_gcs_heartbeat_enabled(false);
        return nullptr;
    }

    if (first_vehicle) set_gcs_heartbeat_enabled(true);
    MGC_LOG_INFO("[MultiVehicle] Opened shared endpoint %s (placeholder %d)",
                 endpoint.c_str(), vehicle->vehicle_id());
    Vehicle* raw = vehicle.get();
    if (added_cb) added_cb(raw);
    return raw;
}

void MultiVehicleManager::on_shared_heartbeat(
    const std::shared_ptr<mg::core::transport::LinkSession>& session,
    const mg::core::mavlink::HeartbeatInfo& heartbeat,
    const mg::core::mavlink::MavlinkPeer& peer) {
    mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
    if (!session ||
        closing_shared_sessions_.count(session.get()) != 0 ||
        heartbeat.system_id == 0 ||
        heartbeat.component_id != MAV_COMP_ID_AUTOPILOT1) {
        return;
    }

    if (const auto ignored = ignored_shared_identities_.find(session->endpoint());
        ignored != ignored_shared_identities_.end()) {
        const auto sysid = ignored->second.find(heartbeat.system_id);
        if (sysid != ignored->second.end()) {
            if (now_ms() < sysid->second) return;
            ignored->second.erase(sysid);
            if (ignored->second.empty()) {
                ignored_shared_identities_.erase(ignored);
            }
        }
    }

    // QGC assumes ArduCopter when a fresh ArduPilot image reports GENERIC
    // before FRAME_CLASS is available.
    uint8_t vehicle_type = heartbeat.vehicle_type;
    if (vehicle_type == MAV_TYPE_GENERIC &&
        heartbeat.autopilot_type == MAV_AUTOPILOT_ARDUPILOTMEGA) {
        vehicle_type = MAV_TYPE_QUADROTOR;
    }

    switch (vehicle_type) {
        case MAV_TYPE_GCS:
        case MAV_TYPE_ONBOARD_CONTROLLER:
        case MAV_TYPE_GIMBAL:
        case MAV_TYPE_ADSB:
            return;
        default:
            break;
    }

    const mg::core::transport::LinkPeer link_peer{peer.host, peer.port};
    std::shared_ptr<Vehicle> existing;
    std::shared_ptr<Vehicle> placeholder;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (const auto& vehicle : vehicles_) {
            if (!vehicle || vehicle->conn().shared_session() != session) continue;
            if (vehicle->system_id() == heartbeat.system_id) {
                existing = vehicle;
                break;
            }
            if (vehicle->system_id() == 0 && !placeholder) {
                placeholder = vehicle;
            }
        }
    }

    if (existing) {
        // Re-register the route after a watchdog reset or identity refresh.
        session->set_peer_for_route(existing->vehicle_id(), link_peer);
        if (const auto mavlink = existing->conn().shared_mavlink()) {
            if (!session->add_vehicle(existing->vehicle_id(),
                                      heartbeat.system_id,
                                      heartbeat.component_id,
                                      mavlink)) {
                MGC_LOG_ERROR(
                    "[MultiVehicle] Failed to re-register vehicle %d on its link "
                    "(sysid=%u compid=%u)",
                    existing->vehicle_id(),
                    static_cast<unsigned>(heartbeat.system_id),
                    static_cast<unsigned>(heartbeat.component_id));
            }
        }
        return;
    }

    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(heartbeat.autopilot_type, vehicle_type);

    if (placeholder) {
        session->set_peer_for_route(placeholder->vehicle_id(), link_peer);
        if (!placeholder->conn().bind_shared_identity(
                heartbeat.system_id, heartbeat.component_id)) {
            return;
        }
        placeholder->set_identity(heartbeat.system_id,
                                  heartbeat.component_id,
                                  adapter.firmware_name(),
                                  adapter.normalized_vehicle_name());
        return;
    }

    int vid = -1;
    {
        mg::core::platform::LockGuard lk(mtx_);
        vid = next_vehicle_id();
    }

    auto vehicle = std::make_shared<Vehicle>(vid);
    session->set_peer_for_route(vid, link_peer);
    if (!vehicle->attach_shared_session(session,
                                        session->endpoint(),
                                        heartbeat.system_id,
                                        heartbeat.component_id)) {
        MGC_LOG_ERROR("[MultiVehicle] Failed to attach discovered vehicle %d",
                      vid);
        return;
    }
    vehicle->set_identity(heartbeat.system_id,
                          heartbeat.component_id,
                          adapter.firmware_name(),
                          adapter.normalized_vehicle_name());

    bool published = false;
    VehicleAddedCallback added_cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        bool conflict = false;
        for (const auto& candidate : vehicles_) {
            if (candidate &&
                candidate->conn().shared_session() == session &&
                candidate->system_id() == heartbeat.system_id) {
                conflict = true;
                break;
            }
        }
        if (!conflict) {
            vehicles_.push_back(vehicle);
            vehicle_sessions_[vid] = session;
            added_cb = on_vehicle_added_;
            published = true;
        }
    }
    if (published) {
        MGC_LOG_INFO("[MultiVehicle] Auto-discovered shared vehicle sysid=%d (vid=%d)",
                     heartbeat.system_id, vid);
        if (added_cb) added_cb(vehicle.get());
    } else {
        vehicle->disconnect();
    }
}

int MultiVehicleManager::open_shared_link(
    const mg::core::transport::LinkConfig& config) {
    const std::string endpoint = config.to_uri();
    auto link = mg::core::transport::LinkFactory::create(config);
    if (!link) return -1;

    const int link_id = next_vehicle_id();
    auto session = std::make_shared<mg::core::transport::LinkSession>(
        std::shared_ptr<mg::core::transport::ILink>(std::move(link)), endpoint);
    const std::weak_ptr<mg::core::transport::LinkSession> weak_session = session;
    session->set_discovery_callback(
        [this, link_id, weak_session](
            const mg::core::mavlink::HeartbeatInfo& heartbeat,
            const mg::core::mavlink::MavlinkPeer& peer)
            -> std::optional<mg::core::mavlink::MavlinkDiscoveredTarget> {
            if (const auto session = weak_session.lock()) {
                on_managed_link_heartbeat(link_id, session, heartbeat, peer);
            }
            return std::nullopt;
        });

    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        if (link_sessions_.count(endpoint) != 0) return -1;
        for (const auto& [existing_id, record] : managed_links_) {
            (void)existing_id;
            if (record.endpoint == endpoint) return -1;
        }
        ManagedLinkRecord record;
        record.link_id = link_id;
        record.session = session;
        record.endpoint = endpoint;
        managed_links_[link_id] = std::move(record);
        managed_link_by_session_[session.get()] = link_id;
        link_sessions_[endpoint] = session;
        ignored_shared_identities_.erase(endpoint);
    }

    if (!session->open()) {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        managed_links_.erase(link_id);
        managed_link_by_session_.erase(session.get());
        link_sessions_.erase(endpoint);
        return -1;
    }
    return link_id;
}

std::shared_ptr<mg::core::transport::LinkSession>
MultiVehicleManager::link_session(int link_id) const {
    mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
    const auto it = managed_links_.find(link_id);
    return it == managed_links_.end() ? nullptr : it->second.session;
}

void MultiVehicleManager::on_managed_link_heartbeat(
    int link_id,
    const std::shared_ptr<mg::core::transport::LinkSession>& session,
    const mg::core::mavlink::HeartbeatInfo& heartbeat,
    const mg::core::mavlink::MavlinkPeer& peer) {
    mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
    if (!session || heartbeat.system_id == 0 ||
        heartbeat.component_id != MAV_COMP_ID_AUTOPILOT1 ||
        closing_shared_sessions_.count(session.get()) != 0) {
        return;
    }

    const auto record_it = managed_links_.find(link_id);
    if (record_it == managed_links_.end()) return;
    auto& record = record_it->second;

    if (const auto ignored = ignored_shared_identities_.find(record.endpoint);
        ignored != ignored_shared_identities_.end()) {
        const auto sysid = ignored->second.find(heartbeat.system_id);
        if (sysid != ignored->second.end()) {
            if (now_ms() < sysid->second) return;
            ignored->second.erase(sysid);
            if (ignored->second.empty()) ignored_shared_identities_.erase(ignored);
        }
    }

    uint8_t vehicle_type = heartbeat.vehicle_type;
    if (vehicle_type == MAV_TYPE_GENERIC &&
        heartbeat.autopilot_type == MAV_AUTOPILOT_ARDUPILOTMEGA) {
        vehicle_type = MAV_TYPE_QUADROTOR;
    }
    switch (vehicle_type) {
        case MAV_TYPE_GCS:
        case MAV_TYPE_ONBOARD_CONTROLLER:
        case MAV_TYPE_GIMBAL:
        case MAV_TYPE_ADSB:
            return;
        default:
            break;
    }

    const mg::core::transport::LinkPeer link_peer{peer.host, peer.port};
    std::shared_ptr<Vehicle> existing;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (const auto& vehicle : vehicles_) {
            if (vehicle && vehicle->conn().shared_session() == session &&
                vehicle->system_id() == heartbeat.system_id) {
                existing = vehicle;
                break;
            }
        }
    }

    if (existing) {
        for (const auto& [existing_route, vehicle_id] : record.route_vehicles) {
            if (vehicle_id == existing->vehicle_id()) {
                session->set_peer_for_route(existing_route, link_peer);
                return;
            }
        }
        return;
    }

    const bool first_vehicle_on_link = record.route_vehicles.empty();
    const int vehicle_id = first_vehicle_on_link
        ? record.link_id
        : next_vehicle_id();
    const int route_id = first_vehicle_on_link
        ? record.link_id
        : vehicle_id;

    session->set_peer_for_route(route_id, link_peer);
    auto vehicle = std::make_shared<Vehicle>(vehicle_id);
    if (!vehicle->attach_shared_primary_route(
            session, route_id, record.endpoint,
            heartbeat.system_id, heartbeat.component_id)) {
        return;
    }
    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(heartbeat.autopilot_type, vehicle_type);
    vehicle->set_identity(heartbeat.system_id,
                          heartbeat.component_id,
                          adapter.firmware_name(),
                          adapter.normalized_vehicle_name());

    bool first_vehicle = false;
    VehicleAddedCallback added_cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        first_vehicle = vehicles_.empty();
        vehicles_.push_back(vehicle);
        vehicle_sessions_[vehicle_id] = session;
        added_cb = on_vehicle_added_;
    }
    record.route_vehicles[route_id] = vehicle_id;
    if (first_vehicle) set_gcs_heartbeat_enabled(true);
    MGC_LOG_INFO("[MultiVehicle] Auto-discovered managed vehicle sysid=%d (vid=%d) on link %d",
                 heartbeat.system_id, vehicle_id, link_id);
    if (added_cb) added_cb(vehicle.get());
}

std::vector<ManagedLinkInfo> MultiVehicleManager::managed_links() const {
    std::vector<ManagedLinkInfo> result;
    std::vector<std::pair<ManagedLinkRecord, std::map<int, std::shared_ptr<Vehicle>>>>
        records;
    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        for (const auto& [link_id, record] : managed_links_) {
            std::map<int, std::shared_ptr<Vehicle>> vehicles;
            for (const auto& [route_id, vehicle_id] : record.route_vehicles) {
                for (const auto& vehicle : vehicles_) {
                    if (vehicle && vehicle->vehicle_id() == vehicle_id) {
                        vehicles[route_id] = vehicle;
                        break;
                    }
                }
            }
            records.emplace_back(record, std::move(vehicles));
        }
    }

    for (const auto& [record, vehicles] : records) {
        if (vehicles.empty()) {
            ManagedLinkInfo info;
            info.link_id = record.link_id;
            info.endpoint = record.endpoint;
            info.state = "waiting_heartbeat";
            result.push_back(std::move(info));
            continue;
        }
        for (const auto& [route_id, vehicle] : vehicles) {
            ManagedLinkInfo info;
            info.link_id = record.link_id;
            info.route_id = route_id;
            info.vehicle_id = vehicle->vehicle_id();
            info.endpoint = record.endpoint;
            info.system_id = vehicle->system_id();
            const auto connection = vehicle->connection_for_route(route_id);
            if (connection) {
                info.connected = connection->is_connected();
                info.heartbeat_alive = connection->heartbeat_alive(5000);
                info.heartbeat_age_ms = connection->heartbeat_age_ms();
                info.active_focus =
                    active_vehicle_id_.load() == vehicle->vehicle_id();
            info.primary = vehicle->primary_route_id() == route_id;
                info.state = mg::core::transport::ConnectionManager::state_string(
                    connection->state());
            }
            result.push_back(std::move(info));
        }
    }
    std::sort(result.begin(), result.end(),
              [](const ManagedLinkInfo& a, const ManagedLinkInfo& b) {
                  if (a.link_id != b.link_id) return a.link_id < b.link_id;
                  return a.route_id < b.route_id;
              });
    return result;
}

bool MultiVehicleManager::close_link(int link_id, bool suppress_rediscovery) {
    std::shared_ptr<mg::core::transport::LinkSession> session;
    std::string endpoint;
    std::map<int, int> routes;
    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        const auto it = managed_links_.find(link_id);
        if (it == managed_links_.end()) return false;
        session = it->second.session;
        endpoint = it->second.endpoint;
        routes = it->second.route_vehicles;
        closing_shared_sessions_.insert(session.get());
        managed_links_.erase(it);
        managed_link_by_session_.erase(session.get());
        link_sessions_.erase(endpoint);
    }

    std::vector<std::pair<std::shared_ptr<Vehicle>, int>> vehicles_to_remove;
    for (const auto& [route_id, vehicle_id] : routes) {
        std::shared_ptr<Vehicle> vehicle;
        {
            mg::core::platform::LockGuard lk(mtx_);
            for (const auto& candidate : vehicles_) {
                if (candidate && candidate->vehicle_id() == vehicle_id) {
                    vehicle = candidate;
                    break;
                }
            }
        }
        if (!vehicle) continue;
        vehicle->remove_shared_route(route_id);
        if (vehicle->connection_count() == 0) {
            vehicles_to_remove.emplace_back(vehicle, vehicle->system_id());
        }
    }

    if (session) session->close();

    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        closing_shared_sessions_.erase(session.get());
        if (suppress_rediscovery) {
            for (const auto& [vehicle, system_id] : vehicles_to_remove) {
                (void)vehicle;
                if (system_id > 0) {
                    ignored_shared_identities_[endpoint][system_id] =
                        now_ms() + rediscovery_suppression_ms_;
                }
            }
        }
    }

    for (const auto& [vehicle, system_id] : vehicles_to_remove) {
        (void)system_id;
        bool found = false;
        {
            mg::core::platform::LockGuard lk(mtx_);
            for (auto it = vehicles_.begin(); it != vehicles_.end(); ++it) {
                if ((*it)->vehicle_id() == vehicle->vehicle_id()) {
                    vehicles_.erase(it);
                    found = true;
                    break;
                }
            }
            if (found && active_vehicle_id_.load() == vehicle->vehicle_id()) {
                active_vehicle_id_.store(vehicles_.empty()
                                             ? -1
                                             : vehicles_.front()->vehicle_id());
            }
        }
        vehicle->disconnect();
    }
    return true;
}

bool MultiVehicleManager::close_route(int route_id, bool suppress_rediscovery) {
    if (route_id < 0) return false;

    std::shared_ptr<mg::core::transport::LinkSession> session;
    std::string endpoint;
    int vehicle_id = -1;
    bool close_session = false;
    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        for (auto link_it = managed_links_.begin();
             link_it != managed_links_.end(); ++link_it) {
            auto route_it = link_it->second.route_vehicles.find(route_id);
            if (route_it == link_it->second.route_vehicles.end()) continue;
            session = link_it->second.session;
            endpoint = link_it->second.endpoint;
            vehicle_id = route_it->second;
            link_it->second.route_vehicles.erase(route_it);
            close_session = link_it->second.route_vehicles.empty();
            if (close_session) {
                closing_shared_sessions_.insert(session.get());
                managed_link_by_session_.erase(session.get());
                link_sessions_.erase(link_it->second.endpoint);
                managed_links_.erase(link_it);
            }
            break;
        }
    }
    if (!session) return false;

    std::shared_ptr<Vehicle> vehicle;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (const auto& candidate : vehicles_) {
            if (candidate && candidate->vehicle_id() == vehicle_id) {
                vehicle = candidate;
                break;
            }
        }
    }

    if (vehicle) vehicle->remove_shared_route(route_id);

    const bool remove_vehicle = vehicle && vehicle->connection_count() == 0;
    if (close_session) session->close();

    std::shared_ptr<Vehicle> replacement;
    bool found = false;
    if (remove_vehicle) {
        mg::core::platform::LockGuard lk(mtx_);
        for (auto it = vehicles_.begin(); it != vehicles_.end(); ++it) {
            if ((*it)->vehicle_id() == vehicle_id) {
                vehicles_.erase(it);
                found = true;
                break;
            }
        }
        if (found) {
            vehicle_sessions_.erase(vehicle_id);
            if (active_vehicle_id_.load() == vehicle_id) {
                if (!vehicles_.empty()) {
                    replacement = vehicles_.front();
                    active_vehicle_id_.store(replacement->vehicle_id());
                } else {
                    active_vehicle_id_.store(-1);
                }
            }
        }
    } else if (vehicle && close_session) {
        // Rebind this vehicle's session lookup to one of its surviving routes.
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        auto map_it = vehicle_sessions_.find(vehicle_id);
        if (map_it != vehicle_sessions_.end() && map_it->second == session) {
            vehicle_sessions_.erase(map_it);
            for (const auto& [existing_link, record] : managed_links_) {
                (void)existing_link;
                for (const auto& [existing_route, existing_vehicle_id] :
                     record.route_vehicles) {
                    (void)existing_route;
                    if (existing_vehicle_id != vehicle_id) continue;
                    vehicle_sessions_[vehicle_id] = record.session;
                    break;
                }
                if (vehicle_sessions_.find(vehicle_id) !=
                    vehicle_sessions_.end()) {
                    break;
                }
            }
        }
    }

    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        closing_shared_sessions_.erase(session.get());
        if (suppress_rediscovery && remove_vehicle && vehicle &&
            vehicle->system_id() > 0) {
            ignored_shared_identities_[endpoint][vehicle->system_id()] =
                now_ms() + rediscovery_suppression_ms_;
        }
    }

    if (remove_vehicle && vehicle) vehicle->disconnect();
    return true;
}

bool MultiVehicleManager::close_vehicle(int vehicle_id,
                                        bool suppress_rediscovery) {
    std::shared_ptr<Vehicle> vehicle_to_close;
    std::shared_ptr<Vehicle> replacement_active;
    std::shared_ptr<mg::core::transport::LinkSession> session_to_close;
    std::vector<std::shared_ptr<mg::core::transport::LinkSession>>
        managed_sessions_to_close;
    std::string session_endpoint;
    bool close_session = false;
    uint8_t ignored_system_id = 0;

    {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        for (auto it = vehicles_.begin(); it != vehicles_.end(); ++it) {
            if ((*it)->vehicle_id() == vehicle_id) {
                // Keep the object alive, but never wait for transport threads
                // while holding the manager mutex.
                vehicle_to_close = *it;
                ignored_system_id = vehicle_to_close->system_id();
                vehicles_.erase(it);
                MGC_LOG_INFO("[MultiVehicle] Closed vehicle %d", vehicle_id);
                if (active_vehicle_id_.load() == vehicle_id) {
                    if (!vehicles_.empty()) {
                        replacement_active = vehicles_.front();
                        active_vehicle_id_.store(replacement_active->vehicle_id());
                    } else {
                        active_vehicle_id_.store(-1);
                    }
                }
                break;
            }
        }

        if (vehicle_to_close) {
            // Remove this vehicle from every explicit managed-link route.
            // Empty physical links are closed; shared links remain open for
            // their other vehicle routes.
            for (auto link_it = managed_links_.begin();
                 link_it != managed_links_.end();) {
                auto& routes = link_it->second.route_vehicles;
                bool removed_route = false;
                for (auto route_it = routes.begin(); route_it != routes.end();) {
                    if (route_it->second == vehicle_id) {
                        route_it = routes.erase(route_it);
                        removed_route = true;
                    } else {
                        ++route_it;
                    }
                }
                if (!removed_route) {
                    ++link_it;
                    continue;
                }
                if (!routes.empty()) {
                    ++link_it;
                    continue;
                }
                const auto session = link_it->second.session;
                managed_sessions_to_close.push_back(session);
                closing_shared_sessions_.insert(session.get());
                managed_link_by_session_.erase(session.get());
                link_sessions_.erase(link_it->second.endpoint);
                link_it = managed_links_.erase(link_it);
            }

            const auto session_it = vehicle_sessions_.find(vehicle_id);
            if (session_it != vehicle_sessions_.end()) {
                session_to_close = session_it->second;
                session_endpoint = session_to_close->endpoint();
                vehicle_sessions_.erase(session_it);
                close_session = std::none_of(
                    vehicle_sessions_.begin(), vehicle_sessions_.end(),
                    [&](const auto& entry) {
                        return entry.second == session_to_close;
                    });
                // Discovery must not recreate this identity while detach is
                // in flight, but we must not hold this lock while waiting for
                // the UDP read thread to stop.
                closing_shared_sessions_.insert(session_to_close.get());
                if (suppress_rediscovery && ignored_system_id > 0) {
                    ignored_shared_identities_[session_endpoint][ignored_system_id] =
                        now_ms() + rediscovery_suppression_ms_;
                }
            }
        }
    }

    if (vehicle_to_close) vehicle_to_close->disconnect();

    std::set<const mg::core::transport::LinkSession*> managed_closed;
    for (const auto& session : managed_sessions_to_close) {
        if (!session) continue;
        managed_closed.insert(session.get());
        session->close();
    }

    if (session_to_close && close_session) {
        if (managed_closed.count(session_to_close.get()) == 0) {
            session_to_close->close();
        }
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        mg::core::platform::LockGuard lk(mtx_);
        const auto it = link_sessions_.find(session_endpoint);
        if (it != link_sessions_.end() && it->second == session_to_close) {
            link_sessions_.erase(it);
        }
        closing_shared_sessions_.erase(session_to_close.get());
    } else if (session_to_close) {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        closing_shared_sessions_.erase(session_to_close.get());
    }
    if (!managed_sessions_to_close.empty()) {
        mg::core::platform::LockGuard lifecycle_lock(shared_lifecycle_mtx_);
        for (const auto& session : managed_sessions_to_close) {
            if (session) closing_shared_sessions_.erase(session.get());
        }
    }

    return vehicle_to_close != nullptr;
}

size_t MultiVehicleManager::remove_disconnected_vehicles() {
    std::vector<int> disconnected_ids;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (const auto& vehicle : vehicles_) {
            if (vehicle && vehicle->all_routes_disconnected()) {
                disconnected_ids.push_back(vehicle->vehicle_id());
            }
        }
    }

    size_t removed = 0;
    for (const int vehicle_id : disconnected_ids) {
        if (close_vehicle(vehicle_id)) ++removed;
    }
    return removed;
}

Vehicle* MultiVehicleManager::get_vehicle(int vehicle_id) const {
    mg::core::platform::LockGuard lk(mtx_);
    for (auto& v : vehicles_) {
        if (v->vehicle_id() == vehicle_id) return v.get();
    }
    return nullptr;
}

std::shared_ptr<Vehicle> MultiVehicleManager::get_vehicle_shared(int vehicle_id) const {
    mg::core::platform::LockGuard lk(mtx_);
    for (const auto& vehicle : vehicles_) {
        if (vehicle->vehicle_id() == vehicle_id) return vehicle;
    }
    return nullptr;
}

Vehicle* MultiVehicleManager::get_vehicle_by_system_id(uint8_t system_id) const {
    mg::core::platform::LockGuard lk(mtx_);
    Vehicle* result = nullptr;
    for (const auto& vehicle : vehicles_) {
        if (vehicle->system_id() != system_id) continue;
        if (result) return nullptr;
        result = vehicle.get();
    }
    return result;
}

std::vector<std::shared_ptr<Vehicle>> MultiVehicleManager::get_all_vehicles_shared() const {
    mg::core::platform::LockGuard lk(mtx_);
    return vehicles_;
}

size_t MultiVehicleManager::vehicle_count() const {
    mg::core::platform::LockGuard lk(mtx_);
    return vehicles_.size();
}

void MultiVehicleManager::add_for_test(std::shared_ptr<Vehicle> vehicle) {
    if (!vehicle) return;
    mg::core::platform::LockGuard lk(mtx_);
    vehicles_.push_back(std::move(vehicle));
}

void MultiVehicleManager::set_active_vehicle(int vehicle_id) {
    std::shared_ptr<Vehicle> selected;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (auto& ve : vehicles_) {
            if (ve->vehicle_id() == vehicle_id) { selected = ve; break; }
        }
        if (selected) {
            active_vehicle_id_.store(vehicle_id);
        }
    }
    if (selected) {
        MGC_LOG_INFO("[MultiVehicle] Active vehicle set to %d (sysid=%d)",
                     vehicle_id, selected->system_id());
    }
}

Vehicle* MultiVehicleManager::active_vehicle() const {
    int id = active_vehicle_id_.load();
    if (id < 0) return nullptr;
    return get_vehicle(id);
}

std::shared_ptr<Vehicle> MultiVehicleManager::active_vehicle_shared() const {
    mg::core::platform::LockGuard lk(mtx_);
    const int id = active_vehicle_id_.load();
    if (id < 0) return nullptr;
    for (const auto& vehicle : vehicles_) {
        if (vehicle && vehicle->vehicle_id() == id) return vehicle;
    }
    return nullptr;
}

void MultiVehicleManager::on_heartbeat(uint8_t system_id, uint8_t component_id,
                                        uint8_t firmware_type, uint8_t vehicle_type,
                                        void* link_context) {
    (void)link_context;
    VehicleAddedCallback added_cb;
    Vehicle* raw = nullptr;

    {
        mg::core::platform::LockGuard lk(mtx_);
        for (auto& v : vehicles_) {
            if (v->system_id() == system_id) {
                mg::core::firmware::FirmwareAdapter adapter;
                adapter.detect(firmware_type, vehicle_type);
                v->set_identity(
                    system_id,
                    component_id,
                    adapter.firmware_name(),
                    adapter.normalized_vehicle_name());
                return;
            }
        }

        auto vehicle = std::make_shared<Vehicle>(next_vehicle_id());
        vehicle->set_identity(system_id, component_id, "", "");
        MGC_LOG_INFO("[MultiVehicle] Auto-discovered vehicle sysid=%d (vid=%d)", system_id, vehicle->vehicle_id());
        raw = vehicle.get();
        vehicles_.push_back(vehicle);
        added_cb = on_vehicle_added_;
    }

    if (added_cb && raw) added_cb(raw);
}

std::vector<mg::core::transport::DiscoveredDevice> MultiVehicleManager::discover_devices(
    mg::core::transport::LinkType type) {
    return mg::core::transport::LinkFactory::discover(type);
}

void MultiVehicleManager::set_gcs_heartbeat_enabled(bool enabled) {
    gcs_heartbeat_enabled_.store(enabled, std::memory_order_release);
    if (enabled) {
        if (gcs_heartbeat_thread_.joinable()) return;
        gcs_heartbeat_thread_ = std::thread(&MultiVehicleManager::gcs_heartbeat_loop, this);
        MGC_LOG_INFO("[MultiVehicle] GCS heartbeat started");
    } else {
        if (gcs_heartbeat_thread_.joinable()) {
            if (gcs_heartbeat_thread_.get_id() == std::this_thread::get_id()) {
                gcs_heartbeat_thread_.detach();
            } else {
                gcs_heartbeat_thread_.join();
            }
        }
        MGC_LOG_INFO("[MultiVehicle] GCS heartbeat stopped");
    }
}

void MultiVehicleManager::gcs_heartbeat_loop() {
    while (gcs_heartbeat_enabled_.load(std::memory_order_acquire)) {
        // Snapshot the vehicle list under the manager lock, but never hold
        // that lock while doing a blocking socket send.
        std::vector<std::shared_ptr<Vehicle>> snapshot;
        {
            mg::core::platform::LockGuard lk(mtx_);
            snapshot = vehicles_;
        }

        for (auto& v : snapshot) {
            if (!v) continue;
            v->link_manager().tick();
            v->refresh_primary_connection();
            const auto state = v->conn().state();
            if (state == mg::core::transport::ConnectionState::WaitingHeartbeat ||
                state == mg::core::transport::ConnectionState::Connected) {
                (void)v->conn().send_heartbeat();
            }
        }

        // A watchdog reset leaves the Vehicle in the registry as a ghost
        // session. Reap it here so the endpoint/session id can be reused.
        (void)remove_disconnected_vehicles();

        for (int i = 0; i < 10; ++i) {
            if (!gcs_heartbeat_enabled_.load(std::memory_order_acquire)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void MultiVehicleManager::set_vehicle_added_callback(VehicleAddedCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    on_vehicle_added_ = std::move(cb);
}
} // namespace mg::core::vehicle
