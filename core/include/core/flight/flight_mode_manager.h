/**
 * flight_mode_manager.h - Flight mode management and control
 *
 * Provides high-level flight mode operations:
 *   - Mode switching with confirmation
 *   - Mode-specific commands (takeoff, land, RTL, etc.)
 *   - Mode status tracking
 *   - Flight mode history
 *
 * Sits above MavlinkAdapter's raw set_mode/commands.
 * Handles mode validation, pre-flight checks, and state tracking.
 *
 * Design: inspired by QGC VehicleFlightModeCoordinator.
 */
#pragma once

#include "core/core_types.h"
#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"
#include "core/platform/platform_mutex.h"
#include "core/mavlink/mavlink_adapter.h"
#include <functional>
#include <string>
#include <vector>

namespace mg::core::flight {

/// Flight mode operation result
enum class ModeResult : uint8_t {
    Success = 0,
    Failed,
    Timeout,
    Rejected,
    InvalidMode,
    NotConnected,
    AlreadyInMode,
};

/// Flight mode change event
struct ModeChangeEvent {
    std::string old_mode;
    std::string new_mode;
    ModeResult result;
    std::string error;
};

/// Flight mode status
struct FlightModeStatus {
    std::string current_mode;
    bool armed = false;
    bool in_flight = false;
    uint8_t system_status = 0;
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
};

/// Callback for mode changes
using ModeChangeCallback = std::function<void(const ModeChangeEvent&)>;

class FlightModeManager {
public:
    FlightModeManager();
    ~FlightModeManager();

    // Non-copyable
    FlightModeManager(const FlightModeManager&) = delete;
    FlightModeManager& operator=(const FlightModeManager&) = delete;

    // ================================================================
    // Mode detection and status
    // ================================================================

    /// Update mode from heartbeat info
    void update_from_heartbeat(uint8_t autopilot_type, uint8_t vehicle_type,
                               uint8_t base_mode, uint32_t custom_mode,
                               bool armed, uint8_t system_status);

    /// Get current flight mode status
    FlightModeStatus get_status() const;

    /// Get current mode name
    std::string current_mode() const;

    /// Check if vehicle is armed
    bool is_armed() const;

    /// Check if vehicle is in flight
    bool is_in_flight() const;

    // ================================================================
    // Mode switching
    // ================================================================

    /// Get available modes for current vehicle type
    std::vector<std::string> available_modes() const;

    /// 全部模式（含不可切换的），供 UI 展示模式表。MP/QGC 都是按机型给完整列表，
    /// 不可切换的项保留但置灰，而不是从列表里删掉。
    std::vector<mg::core::firmware::FlightModeEntry> mode_entries() const;

    /// 把 custom_mode 数值翻译成模式名（HEARTBEAT 反查、模式映射页都用它）。
    std::string decode_mode_name(uint32_t custom_mode) const;

    /// 本轮飞行是否跑完了任务。
    ///
    /// 判定逻辑与 QGC `FlyViewMissionCompleteDialog.qml` 一致：**曾经在任务模式
    /// （AUTO / Auto Mission）下解锁飞行，然后上锁**才叫完成。只看"Heartbeat 最后一项
    /// 到达"会偏早——固定翼到达最后一项后还要返航降落。
    /// 下一次解锁会清掉这个标记，所以它表示"最近一次飞行"的结果。
    bool mission_fly_completed() const;

    /// 本轮解锁飞行中是否**离开过任务模式**（任务模式下解锁后又切到别的模式）。
    ///
    /// 光看这一位不能判定"中止"：飞控跑完任务后自己也会切 RTL/降落。调用方要结合
    /// "最后一项是否到达"来判断，见 `mg_mission_get_execution_progress` 的 `aborted`。
    bool mission_mode_exited_while_armed() const;

    /// 按名字切换模式。MP 的 CMB_modes 与 QGC 的 flightMode 属性都是按名字切。
    /// 名字不识别或未绑定时返回 false。
    bool set_mode(const std::string& mode_name);

    bool arm(bool force = false);
    bool disarm(bool force = false);
    /// 按语义角色切模式（模式号来自固件插件，见 S6）。见 .cc 中的说明。
    bool set_mode_role(
        mg::core::firmware::FirmwareAdapter::ModeRole role, const char* label);

    bool return_to_launch();
    bool land();
    bool loiter();

    // ================================================================
    // Mode-specific parameters
    // ================================================================

    /// Set takeoff altitude for guided mode
    void set_takeoff_altitude(float altitude_m);

    /// Get takeoff altitude
    float takeoff_altitude() const;

    /// Set loiter radius
    void set_loiter_radius(float radius_m);

    /// Get loiter radius
    float loiter_radius() const;

    // ================================================================
    // Mode history
    // ================================================================

    /// Get mode change history (last N changes)
    std::vector<ModeChangeEvent> get_history(size_t max_entries = 20) const;

    // ================================================================
    // Binding
    // ================================================================

    /// Bind to a MavlinkAdapter for sending commands
    void set_mavlink(mg::core::mavlink::MavlinkAdapter* mav, uint8_t sys_id = 1, uint8_t comp_id = 0);

private:
    // NON-RECURSIVE: never call back into FlightModeManager while holding this lock.
    // Callbacks (mode_change_cb_) are always invoked outside the lock.
    mutable mg::core::platform::Mutex mtx_;
    mg::core::firmware::FirmwareAdapter fw_adapter_;
    std::unique_ptr<mg::core::firmware::IFirmwarePlugin> fw_plugin_;

    // Current state
    std::string current_mode_;
    bool armed_ = false;
    bool in_flight_ = false;
    uint8_t system_status_ = 0;
    uint8_t base_mode_ = 0;
    uint32_t custom_mode_ = 0;
    uint8_t autopilot_type_ = 0;
    uint8_t vehicle_type_ = 0;

    // 任务完成判定的历史状态（详见 mission_fly_completed()）。
    bool was_armed_ = false;
    bool flight_armed_ = false;
    bool flight_in_mission_mode_ = false;
    bool mission_fly_completed_ = false;
    bool mission_mode_exited_ = false;

    // Mode parameters
    float takeoff_altitude_ = 20.0f;
    float loiter_radius_ = 50.0f;

    // History
    std::vector<ModeChangeEvent> history_;
    static constexpr size_t kMaxHistory = 50;

    // Callback
    ModeChangeCallback mode_change_cb_;
    mg::core::mavlink::MavlinkAdapter* mavlink_ = nullptr;
    uint8_t target_sys_ = 1;
    uint8_t target_comp_ = 0;
};

} // namespace mg::core::flight
