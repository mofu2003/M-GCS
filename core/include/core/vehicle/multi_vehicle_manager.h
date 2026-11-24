/*
 * multi_vehicle_manager.h - 多飞行器运行时管理器
 */
#ifndef MGCORE_MULTI_VEHICLE_MANAGER_H
#define MGCORE_MULTI_VEHICLE_MANAGER_H

#include "core/vehicle/vehicle.h"
#include "core/mavlink/mavlink_router.h"
#include "core/transport/link_config.h"
#include "core/transport/link_factory.h"
#include "core/platform/platform_mutex.h"
#include <memory>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include <optional>
#include <atomic>
#include <thread>

namespace mg::core::vehicle {

// 飞行器添加回调
using VehicleAddedCallback = std::function<void(Vehicle* vehicle)>;
// 飞行器移除回调
// 活跃飞行器切换回调

struct ManagedLinkInfo {
    int link_id = -1;
    int route_id = -1;
    int vehicle_id = -1;
    std::string endpoint;
    uint8_t system_id = 0;
    bool connected = false;
    bool heartbeat_alive = false;
    uint64_t heartbeat_age_ms = 0;
    bool active_focus = false;
    bool primary = false;
    std::string state;
};

// 多飞行器管理器，负责飞行器的创建、销毁和活跃飞行器切换
class MultiVehicleManager {
public:
    using NowMs = std::function<uint64_t()>;

    explicit MultiVehicleManager(
        NowMs now_ms = {},
        uint64_t rediscovery_suppression_ms = 30000);
    ~MultiVehicleManager();

    MultiVehicleManager(const MultiVehicleManager&) = delete;
    MultiVehicleManager& operator=(const MultiVehicleManager&) = delete;

    // 初始化管理器
    void init();

    // 根据配置打开一个新的飞行器连接；不会改变 active vehicle focus。
    Vehicle* open_vehicle(const mg::core::transport::LinkConfig& config);

    /// Opens one physical link shared by multiple MAVLink systems. The
    /// returned Vehicle is a placeholder until its first heartbeat; additional
    /// sysid identities on the same endpoint are added as they are discovered.
    Vehicle* open_shared_vehicle(const mg::core::transport::LinkConfig& config);

    /// Opens an explicit link handle. Vehicles are created only after valid
    /// heartbeats. Vehicles with the same sysid on independent physical links
    /// retain separate vehicle identities and command sessions.
    int open_shared_link(const mg::core::transport::LinkConfig& config);
    bool close_link(int link_id, bool suppress_rediscovery = false);
    /// Closes one (link, vehicle) route. The physical link remains open when
    /// other vehicles are still routed over it.
    bool close_route(int route_id, bool suppress_rediscovery = false);
    std::shared_ptr<mg::core::transport::LinkSession> link_session(
        int link_id) const;
    std::vector<ManagedLinkInfo> managed_links() const;
    // 关闭指定 ID 的飞行器
    bool close_vehicle(int vehicle_id, bool suppress_rediscovery = false);

    // 清理已经丢失链路并回到 Disconnected 的飞行器，避免幽灵 session
    // 继续占用 vehicle_id 并阻止同名端点的重连。
    size_t remove_disconnected_vehicles();

    // 按飞行器 ID 获取
    Vehicle* get_vehicle(int vehicle_id) const;
    /// Acquire a lifetime lease for work which may race with vehicle removal.
    std::shared_ptr<Vehicle> get_vehicle_shared(int vehicle_id) const;
    /// Acquire the focused vehicle and its lifetime lease atomically.
    std::shared_ptr<Vehicle> active_vehicle_shared() const;
    // 按系统 ID 获取；多个独立链路同号时返回 nullptr，调用方应使用 vehicle_id
    Vehicle* get_vehicle_by_system_id(uint8_t system_id) const;
    // Lifetime-safe registry snapshot for FFI readers racing watchdog removal.
    std::vector<std::shared_ptr<Vehicle>> get_all_vehicles_shared() const;
    // 获取飞行器数量
    size_t vehicle_count() const;

    // 仅供测试注入预构造 Vehicle，避免依赖真实连接路径。
    void add_for_test(std::shared_ptr<Vehicle> vehicle);

    // 设置活跃飞行器
    void set_active_vehicle(int vehicle_id);
    // 获取当前活跃飞行器
    Vehicle* active_vehicle() const;

    // 处理接收到的心跳（用于自动发现新飞行器）
    void on_heartbeat(uint8_t system_id, uint8_t component_id,
                      uint8_t firmware_type, uint8_t vehicle_type,
                      void* link_context);

    // 发现指定类型的设备列表
    std::vector<mg::core::transport::DiscoveredDevice> discover_devices(
        mg::core::transport::LinkType type = mg::core::transport::LinkType::Serial);

    /// 启停 GCS 心跳定时器，启用后每秒向所有已连接飞行器发送心跳
    void set_gcs_heartbeat_enabled(bool enabled);

    // 设置飞行器添加/移除/切换回调
    void set_vehicle_added_callback(VehicleAddedCallback cb);

private:
    // 分配下一个飞行器 ID
    int next_vehicle_id() { return next_id_.fetch_add(1); }
    // GCS 心跳发送循环
    void gcs_heartbeat_loop();
    // Shared endpoint heartbeat discovery callback.
    void on_shared_heartbeat(
        const std::shared_ptr<mg::core::transport::LinkSession>& session,
        const mg::core::mavlink::HeartbeatInfo& heartbeat,
        const mg::core::mavlink::MavlinkPeer& peer);
    void on_managed_link_heartbeat(
        int link_id,
        const std::shared_ptr<mg::core::transport::LinkSession>& session,
        const mg::core::mavlink::HeartbeatInfo& heartbeat,
        const mg::core::mavlink::MavlinkPeer& peer);
    uint64_t now_ms() const;

    struct ManagedLinkRecord {
        int link_id = -1;
        std::shared_ptr<mg::core::transport::LinkSession> session;
        std::string endpoint;
        std::map<int, int> route_vehicles;
    };

    // Serializes shared-session discovery with open/close lifecycle.
    // It must not be held while waiting for a transport thread to stop.
    mutable mg::core::platform::Mutex shared_lifecycle_mtx_;
    std::set<const mg::core::transport::LinkSession*> closing_shared_sessions_;
    std::map<std::string, std::map<uint8_t, uint64_t>>
        ignored_shared_identities_;
    NowMs now_ms_;
    uint64_t rediscovery_suppression_ms_ = 30000;

    mutable mg::core::platform::Mutex mtx_;  // 加锁保护飞行器列表
    std::vector<std::shared_ptr<Vehicle>> vehicles_;
    std::map<std::string, std::shared_ptr<mg::core::transport::LinkSession>>
        link_sessions_;
    std::map<int, ManagedLinkRecord> managed_links_;
    std::map<const mg::core::transport::LinkSession*, int>
        managed_link_by_session_;
    std::map<int, std::shared_ptr<mg::core::transport::LinkSession>>
        vehicle_sessions_;
    std::atomic<int> next_id_{1};
    std::atomic<int> active_vehicle_id_{-1};

    VehicleAddedCallback on_vehicle_added_;

    // GCS 心跳定时器
    std::atomic<bool> gcs_heartbeat_enabled_{false};
    std::thread       gcs_heartbeat_thread_;
};

} // namespace mg::core::vehicle

#endif /* MGCORE_MULTI_VEHICLE_MANAGER_H */
