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
#include <thread>
#include <chrono>

namespace mg::core::vehicle {

MultiVehicleManager::MultiVehicleManager() = default;
MultiVehicleManager::~MultiVehicleManager() { set_gcs_heartbeat_enabled(false); }

void MultiVehicleManager::init() { MGC_LOG_INFO("[MultiVehicle] Manager initialized"); }

Vehicle* MultiVehicleManager::open_vehicle(const mg::core::transport::LinkConfig& config,
                                                  Vehicle::EventCallback event_cb) {
    std::shared_ptr<Vehicle> vehicle;
    int vid = -1;
    {
        mg::core::platform::LockGuard lk(mtx_);
        vid = next_vehicle_id();
        vehicle = std::make_shared<Vehicle>(vid);
    }

    // 先设置事件回调，再 connect（避免心跳到达时 event_cb 为空的竞态窗口）
    if (event_cb) {
        vehicle->set_event_callback(std::move(event_cb));
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

bool MultiVehicleManager::close_vehicle(int vehicle_id) {
    std::shared_ptr<Vehicle> vehicle_to_close;
    std::shared_ptr<Vehicle> replacement_active;
    VehicleRemovedCallback removed_cb;
    ActiveVehicleChangedCallback changed_cb;
    bool active_changed = false;

    {
        mg::core::platform::LockGuard lk(mtx_);
        for (auto it = vehicles_.begin(); it != vehicles_.end(); ++it) {
            if ((*it)->vehicle_id() == vehicle_id) {
                // Keep the object alive, but never wait for transport threads
                // while holding the manager mutex.
                vehicle_to_close = *it;
                vehicles_.erase(it);
                MGC_LOG_INFO("[MultiVehicle] Closed vehicle %d", vehicle_id);
                removed_cb = on_vehicle_removed_;
                changed_cb = on_active_vehicle_changed_;
                if (active_vehicle_id_.load() == vehicle_id) {
                    if (!vehicles_.empty()) {
                        replacement_active = vehicles_.front();
                        active_vehicle_id_.store(replacement_active->vehicle_id());
                    } else {
                        active_vehicle_id_.store(-1);
                    }
                    active_changed = true;
                }
                break;
            }
        }
    }

    if (vehicle_to_close) {
        vehicle_to_close->disconnect();
    }

    if (removed_cb) removed_cb(vehicle_id);
    if (active_changed && changed_cb) {
        changed_cb(replacement_active ? replacement_active.get() : nullptr);
    }
    return vehicle_to_close != nullptr;
}

size_t MultiVehicleManager::remove_disconnected_vehicles() {
    std::vector<int> disconnected_ids;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (const auto& vehicle : vehicles_) {
            if (vehicle && vehicle->conn().state() ==
                    mg::core::transport::ConnectionState::Disconnected) {
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
    for (auto& v : vehicles_) {
        if (v->system_id() == system_id) return v.get();
    }
    return nullptr;
}

std::vector<Vehicle*> MultiVehicleManager::get_all_vehicles() const {
    mg::core::platform::LockGuard lk(mtx_);
    std::vector<Vehicle*> result;
    for (auto& v : vehicles_) result.push_back(v.get());
    return result;
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
    ActiveVehicleChangedCallback cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (auto& ve : vehicles_) {
            if (ve->vehicle_id() == vehicle_id) { selected = ve; break; }
        }
        if (selected) {
            active_vehicle_id_.store(vehicle_id);
            cb = on_active_vehicle_changed_;
        }
    }
    if (selected && cb) {
        cb(selected.get());
        MGC_LOG_INFO("[MultiVehicle] Active vehicle set to %d (sysid=%d)",
                     vehicle_id, selected->system_id());
    }
}

Vehicle* MultiVehicleManager::active_vehicle() const {
    int id = active_vehicle_id_.load();
    if (id < 0) return nullptr;
    return get_vehicle(id);
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
void MultiVehicleManager::set_vehicle_removed_callback(VehicleRemovedCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    on_vehicle_removed_ = std::move(cb);
}
void MultiVehicleManager::set_active_vehicle_changed_callback(ActiveVehicleChangedCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    on_active_vehicle_changed_ = std::move(cb);
}

} // namespace mg::core::vehicle
