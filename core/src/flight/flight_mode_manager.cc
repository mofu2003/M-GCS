/**
 * flight_mode_manager.cc - Flight mode management implementation
 *
 * Provides high-level flight mode operations with mode validation,
 * state tracking, and history.
 */
#include "core/flight/flight_mode_manager.h"
#include "core/log/logging.h"
#include <algorithm>

namespace mg::core::flight {

// ================================================================
// Constructor / Destructor
// ================================================================

FlightModeManager::FlightModeManager() = default;
FlightModeManager::~FlightModeManager() = default;

// ================================================================
// Mode detection and status
// ================================================================

void FlightModeManager::update_from_heartbeat(uint8_t autopilot_type, uint8_t vehicle_type,
                                               uint8_t base_mode, uint32_t custom_mode,
                                               bool armed, uint8_t system_status) {
    ModeChangeEvent pending_event;
    bool has_event = false;
    ModeChangeCallback cb_copy;

    {
        mg::core::platform::LockGuard lk(mtx_);

        // Detect firmware/vehicle type
        if (autopilot_type != autopilot_type_ || vehicle_type != vehicle_type_) {
            fw_adapter_.detect(autopilot_type, vehicle_type);
            autopilot_type_ = autopilot_type;
            vehicle_type_ = vehicle_type;
            // Create firmware plugin for this firmware type
            fw_plugin_ = mg::core::firmware::create_firmware_plugin(fw_adapter_.firmware_kind());
        }

        std::string old_mode = current_mode_;

        // Decode flight mode
        auto mode_info = fw_adapter_.decode_flight_mode(custom_mode);
        current_mode_ = mode_info.mode_name;
        armed_ = armed;
        base_mode_ = base_mode;
        custom_mode_ = custom_mode;
        system_status_ = system_status;

        // Detect if in flight (based on system status and armed state)
        // MAV_STATE_ACTIVE = 4, MAV_STATE_IN_FLIGHT = 4
        in_flight_ = armed && (system_status == 4);

        // 任务完成判定（QGC FlyViewMissionCompleteDialog 的等价实现）：
        // 记录"本轮解锁期间是否进过任务模式"，解锁状态消失时据此置完成标记。
        const char* auto_mode = fw_plugin_ ? fw_plugin_->auto_mode_name() : nullptr;
        const bool in_mission_mode =
            auto_mode != nullptr && current_mode_ == auto_mode;
        if (armed) {
            if (!was_armed_) {
                // 新一轮飞行：清掉上一轮的结果，重新累计。
                mission_fly_completed_ = false;
                mission_mode_exited_ = false;
                flight_armed_ = false;
                flight_in_mission_mode_ = false;
            }
            flight_armed_ = true;
            if (in_mission_mode) flight_in_mission_mode_ = true;
            // 解锁状态下从任务模式切走 = 人为中断。正常收尾（AUTO → RTL）由调用方
            // 结合"最后一项是否到达"再判断，这里只记录"曾经切走"这个事实。
            else if (flight_in_mission_mode_) mission_mode_exited_ = true;
        } else if (was_armed_) {
            if (flight_armed_ && flight_in_mission_mode_) {
                mission_fly_completed_ = true;
            }
            flight_armed_ = false;
            flight_in_mission_mode_ = false;
        }
        was_armed_ = armed;

        // Record mode change and copy callback under lock
        if (!old_mode.empty() && old_mode != current_mode_) {
            pending_event.old_mode = old_mode;
            pending_event.new_mode = current_mode_;
            pending_event.result = ModeResult::Success;
            history_.push_back(pending_event);
            if (history_.size() > kMaxHistory) {
                history_.erase(history_.begin());
            }
            has_event = true;
        }

        cb_copy = mode_change_cb_;
    }
    // Invoke callback OUTSIDE the lock to prevent deadlock if the
    // callback re-enters FlightModeManager (std::mutex is not recursive).
    if (has_event && cb_copy) {
        cb_copy(pending_event);
    }
}

FlightModeStatus FlightModeManager::get_status() const {
    mg::core::platform::LockGuard lk(mtx_);
    FlightModeStatus status;
    status.current_mode = current_mode_;
    status.armed = armed_;
    status.in_flight = in_flight_;
    status.system_status = system_status_;
    status.base_mode = base_mode_;
    status.custom_mode = custom_mode_;
    return status;
}

std::string FlightModeManager::current_mode() const {
    mg::core::platform::LockGuard lk(mtx_);
    return current_mode_;
}

bool FlightModeManager::is_armed() const {
    mg::core::platform::LockGuard lk(mtx_);
    return armed_;
}

bool FlightModeManager::is_in_flight() const {
    mg::core::platform::LockGuard lk(mtx_);
    return in_flight_;
}

// ================================================================
// Mode switching
// ================================================================

std::vector<std::string> FlightModeManager::available_modes() const {
    mg::core::platform::LockGuard lk(mtx_);
    std::vector<std::string> modes;

    if (fw_plugin_) {
        // Delegate to firmware plugin (QGC FirmwarePlugin pattern)
        auto entries = fw_plugin_->available_modes(fw_adapter_.vehicle_kind());
        modes.reserve(entries.size());
        for (const auto& e : entries) {
            if (e.can_be_set) {
                modes.push_back(e.name);
            }
        }
    }
    return modes;
}

std::vector<mg::core::firmware::FlightModeEntry>
FlightModeManager::mode_entries() const {
    mg::core::platform::LockGuard lk(mtx_);
    if (!fw_plugin_) return {};
    return fw_plugin_->available_modes(fw_adapter_.vehicle_kind());
}

std::string FlightModeManager::decode_mode_name(uint32_t custom_mode) const {
    mg::core::platform::LockGuard lk(mtx_);
    if (!fw_plugin_) return {};
    return fw_plugin_->decode_flight_mode(fw_adapter_.vehicle_kind(), custom_mode);
}

bool FlightModeManager::mission_mode_exited_while_armed() const {
    mg::core::platform::LockGuard lk(mtx_);
    return mission_mode_exited_;
}

bool FlightModeManager::mission_fly_completed() const {
    mg::core::platform::LockGuard lk(mtx_);
    return mission_fly_completed_;
}

bool FlightModeManager::resolve_mode_request_locked(
    const std::string& mode_name, uint8_t& base_mode, uint32_t& custom_mode) const {
    if (!fw_plugin_ || mode_name.empty()) return false;
    for (const auto& entry :
         fw_plugin_->available_modes(fw_adapter_.vehicle_kind())) {
        if (entry.name != mode_name) continue;
        if (!entry.can_be_set) return false;
        custom_mode = entry.custom_mode;
        // QGC Vehicle::setFlightMode：base_mode 带 CUSTOM_MODE_ENABLED，并按需保留
        // SAFETY_ARMED 位，ArduPilot 只看 custom_mode。
        base_mode = 0x01;                 // MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
        if (armed_) base_mode |= 0x80;    // MAV_MODE_FLAG_SAFETY_ARMED
        return true;
    }
    return false;
}

bool FlightModeManager::resolve_mode_request(
    const std::string& mode_name, uint8_t& base_mode, uint32_t& custom_mode) const {
    mg::core::platform::LockGuard lk(mtx_);
    return resolve_mode_request_locked(mode_name, base_mode, custom_mode);
}

// ================================================================
// Mode-specific parameters
// ================================================================

void FlightModeManager::set_takeoff_altitude(float altitude_m) {
    mg::core::platform::LockGuard lk(mtx_);
    takeoff_altitude_ = altitude_m;
}

float FlightModeManager::takeoff_altitude() const {
    mg::core::platform::LockGuard lk(mtx_);
    return takeoff_altitude_;
}

void FlightModeManager::set_loiter_radius(float radius_m) {
    mg::core::platform::LockGuard lk(mtx_);
    loiter_radius_ = radius_m;
}

float FlightModeManager::loiter_radius() const {
    mg::core::platform::LockGuard lk(mtx_);
    return loiter_radius_;
}

// ================================================================
// Mode history
// ================================================================

std::vector<ModeChangeEvent> FlightModeManager::get_history(size_t max_entries) const {
    mg::core::platform::LockGuard lk(mtx_);
    size_t start = (history_.size() > max_entries) ? (history_.size() - max_entries) : 0;
    return std::vector<ModeChangeEvent>(history_.begin() + start, history_.end());
}

} // namespace mg::core::flight
