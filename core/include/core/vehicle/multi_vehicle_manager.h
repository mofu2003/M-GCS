/*
 * multi_vehicle_manager.h - 多飞行器运行时管理器
 */
#ifndef MGCORE_MULTI_VEHICLE_MANAGER_H
#define MGCORE_MULTI_VEHICLE_MANAGER_H

#include "core/vehicle/vehicle.h"
#include "core/transport/link_config.h"
#include "core/transport/link_factory.h"
#include "core/platform/platform_mutex.h"
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>

namespace mg::core::vehicle {

// 飞行器添加回调
using VehicleAddedCallback = std::function<void(Vehicle* vehicle)>;
// 飞行器移除回调
using VehicleRemovedCallback = std::function<void(int vehicle_id)>;
// 活跃飞行器切换回调
using ActiveVehicleChangedCallback = std::function<void(Vehicle* vehicle)>;

// 多飞行器管理器，负责飞行器的创建、销毁和活跃飞行器切换
class MultiVehicleManager {
public:
    MultiVehicleManager();
    ~MultiVehicleManager();

    MultiVehicleManager(const MultiVehicleManager&) = delete;
    MultiVehicleManager& operator=(const MultiVehicleManager&) = delete;

    // 初始化管理器
    void init();

    // 根据配置打开一个新的飞行器连接；不会改变 active vehicle focus。
    Vehicle* open_vehicle(const mg::core::transport::LinkConfig& config,
                              Vehicle::EventCallback event_cb = nullptr);
    // 关闭指定 ID 的飞行器
    bool close_vehicle(int vehicle_id);

    // 清理已经丢失链路并回到 Disconnected 的飞行器，避免幽灵 session
    // 继续占用 vehicle_id 并阻止同名端点的重连。
    size_t remove_disconnected_vehicles();

    // 按飞行器 ID 获取
    Vehicle* get_vehicle(int vehicle_id) const;
    /// Acquire a lifetime lease for work which may race with vehicle removal.
    std::shared_ptr<Vehicle> get_vehicle_shared(int vehicle_id) const;
    // 按系统 ID 获取
    Vehicle* get_vehicle_by_system_id(uint8_t system_id) const;
    // 获取所有飞行器列表
    std::vector<Vehicle*> get_all_vehicles() const;
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
    void set_vehicle_removed_callback(VehicleRemovedCallback cb);
    void set_active_vehicle_changed_callback(ActiveVehicleChangedCallback cb);

private:
    // 分配下一个飞行器 ID
    int next_vehicle_id() { return next_id_.fetch_add(1); }
    // GCS 心跳发送循环
    void gcs_heartbeat_loop();

    mutable mg::core::platform::Mutex mtx_;  // 加锁保护飞行器列表
    std::vector<std::shared_ptr<Vehicle>> vehicles_;
    std::atomic<int> next_id_{1};
    std::atomic<int> active_vehicle_id_{-1};

    VehicleAddedCallback on_vehicle_added_;
    VehicleRemovedCallback on_vehicle_removed_;
    ActiveVehicleChangedCallback on_active_vehicle_changed_;

    // GCS 心跳定时器
    std::atomic<bool> gcs_heartbeat_enabled_{false};
    std::thread       gcs_heartbeat_thread_;
};

} // namespace mg::core::vehicle

#endif /* MGCORE_MULTI_VEHICLE_MANAGER_H */
