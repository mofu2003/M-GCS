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


    // ================================================================
    // Mode switching
    // ================================================================

    /// Get available modes for current vehicle type
    std::vector<std::string> available_modes() const;

    /// 全部模式（含不可切换的），供 UI 展示模式表。MP/QGC 都是按机型给完整列表，
    /// 不可切换的项保留但置灰，而不是从列表里删掉。
    std::vector<mg::core::firmware::FlightModeEntry> mode_entries() const;


    /// 本轮进过任务模式的飞行是否已上锁结束。
    /// 这不代表任务完成；调用方还必须确认最后一项已到达。
    bool mission_flight_ended() const;

    /// 进过任务模式后，当前是否仍处于中断状态。恢复任务模式时清除。
    ///
    /// 光看这一位不能判定"中止"：飞控跑完任务后自己也会切 RTL/降落。调用方要结合
    /// "最后一项是否到达"来判断，见 `mg_mission_get_execution_progress` 的 `aborted`。
    bool mission_mode_exited_while_armed() const;

    /// 解析一个按名字切换的请求：命中且可切换时填充 base_mode/custom_mode。
    /// base_mode 规则是 CUSTOM_MODE_ENABLED（已解锁时附带 SAFETY_ARMED），
    /// 供走 ACK 队列的调用方使用，避免两处各算一遍。
    ///
    /// 这里**不提供发送入口**：解锁/上锁/切模式的生产路径是 FFI 的命令队列
    /// （`mg_command_submit_for_vehicle` → `arm_disarm_queued`/`set_mode_queued`），
    /// 由它绑定 ticket 并等 COMMAND_ACK。以前这里有一套只发送、不等 ACK 的
    /// `arm()`/`disarm()`/`set_mode()`，而且在“已是目标状态”时静默返回 false，
    /// 调用方无法区分“无需动作”和“被拒绝”（S1）；它们没有任何调用者，已删除。
    bool resolve_mode_request(const std::string& mode_name,
                              uint8_t& base_mode, uint32_t& custom_mode) const;

    // ================================================================
    // Mode-specific parameters
    // ================================================================





    // ================================================================
    // Mode history
    // ================================================================

    /// Get mode change history (last N changes)
    std::vector<ModeChangeEvent> get_history(size_t max_entries = 20) const;

private:
    /// resolve_mode_request() 的持锁版本，调用方必须已持有 mtx_。
    bool resolve_mode_request_locked(const std::string& mode_name,
                                     uint8_t& base_mode,
                                     uint32_t& custom_mode) const;

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

    // 任务完成判定的历史状态（详见 mission_flight_ended()）。
    bool was_armed_ = false;
    bool flight_armed_ = false;
    bool flight_in_mission_mode_ = false;
    bool mission_flight_ended_ = false;
    bool mission_mode_exited_ = false;

    // Mode parameters

    // History
    std::vector<ModeChangeEvent> history_;
    static constexpr size_t kMaxHistory = 50;

    // Callback
    ModeChangeCallback mode_change_cb_;
};

} // namespace mg::core::flight
