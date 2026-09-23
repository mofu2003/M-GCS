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
    bool first_vehicle = false;
    {
        mg::core::platform::LockGuard lk(mtx_);
        vid = next_vehicle_id();
        first_vehicle = vehicles_.empty();
        vehicle = std::make_shared<Vehicle>(vid);
        vehicles_.push_back(vehicle);
    }

    if (first_vehicle) {
        set_gcs_heartbeat_enabled(true);
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

    VehicleAddedCallback added_cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        added_cb = on_vehicle_added_;
    }

    Vehicle* raw = vehicle.get();
    if (added_cb) added_cb(raw);
    return raw;
}

void MultiVehicleManager::close_vehicle(int vehicle_id) {
    std::shared_ptr<Vehicle> vehicle_to_close;
    VehicleRemovedCallback removed_cb;
    ActiveVehicleChangedCallback changed_cb;
    bool focus_cleared = false;
    bool empty_after_close = false;

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
                    active_vehicle_id_.store(-1);
                    focus_cleared = true;
                }
                empty_after_close = vehicles_.empty();
                break;
            }
        }
    }

    if (vehicle_to_close) {
        vehicle_to_close->disconnect();
    }

    if (empty_after_close) {
        set_gcs_heartbeat_enabled(false);
    }

    if (removed_cb) removed_cb(vehicle_id);
    if (focus_cleared && changed_cb) changed_cb(nullptr);
}

Vehicle* MultiVehicleManager::get_vehicle(int vehicle_id) const {
    mg::core::platform::LockGuard lk(mtx_);
    for (auto& v : vehicles_) {
        if (v->vehicle_id() == vehicle_id) return v.get();
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
    Vehicle* v = nullptr;
    ActiveVehicleChangedCallback cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        for (auto& ve : vehicles_) {
            if (ve->vehicle_id() == vehicle_id) { v = ve.get(); break; }
        }
        if (v) {
            active_vehicle_id_.store(vehicle_id);
            cb = on_active_vehicle_changed_;
        }
    }
    if (v && cb) {
        cb(v);
        MGC_LOG_INFO("[MultiVehicle] Active vehicle set to %d (sysid=%d)", vehicle_id, v->system_id());
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
        if (gcs_heartbeat_thread_.joinable()) gcs_heartbeat_thread_.join();
        MGC_LOG_INFO("[MultiVehicle] GCS heartbeat stopped");
    }
}

void MultiVehicleManager::gcs_heartbeat_loop() {
    while (gcs_heartbeat_enabled_.load(std::memory_order_acquire)) {
        {
            mg::core::platform::LockGuard lk(mtx_);
            for (auto& v : vehicles_) {
                const auto state = v->conn().state();
                if (state == mg::core::transport::ConnectionState::WaitingHeartbeat ||
                    state == mg::core::transport::ConnectionState::Connected) {
                    auto* mav = v->mavlink();
                    if (mav) mav->send_heartbeat();
                }
            }
        }
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
