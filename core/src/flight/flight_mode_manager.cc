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

void FlightModeManager::set_mavlink(mg::core::mavlink::MavlinkAdapter* mav, uint8_t sys_id, uint8_t comp_id) {
    mavlink_ = mav;
    target_sys_ = sys_id;
    target_comp_ = comp_id;
}

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

bool FlightModeManager::set_mode(const std::string& mode_name) {
    mg::core::platform::LockGuard lk(mtx_);
    if (!fw_plugin_ || !mavlink_ || mode_name.empty()) return false;
    const int custom =
        fw_plugin_->encode_flight_mode(fw_adapter_.vehicle_kind(), mode_name);
    if (custom < 0) return false;
    // QGC Vehicle::setFlightMode：base_mode 带 CUSTOM_MODE_ENABLED，并按需保留
    // SAFETY_ARMED 位，ArduPilot 只看 custom_mode。
    uint8_t base_mode = 0x01;             // MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
    if (armed_) base_mode |= 0x80;        // MAV_MODE_FLAG_SAFETY_ARMED
    return mavlink_->set_mode(target_sys_, target_comp_, base_mode,
                              static_cast<uint32_t>(custom));
}

bool FlightModeManager::arm(bool force) {
    if (!armed_) {
        MGC_LOG_INFO("[FlightMode] Arm requested (force=%d)", force);
        if (mavlink_) return mavlink_->arm_disarm(target_sys_, target_comp_, true, force);
        return false;
    }
    return false;
}

bool FlightModeManager::disarm(bool force) {
    if (armed_) {
        MGC_LOG_INFO("[FlightMode] Disarm requested (force=%d)", force);
        if (mavlink_) return mavlink_->arm_disarm(target_sys_, target_comp_, false, force);
        return false;
    }
    return false;
}

bool FlightModeManager::return_to_launch() {
    mg::core::platform::LockGuard lk(mtx_);
    MGC_LOG_INFO("[FlightMode] RTL requested");
    if (mavlink_) return mavlink_->set_mode(target_sys_, target_comp_, 0, 6);
    return false;
}

bool FlightModeManager::land() {
    mg::core::platform::LockGuard lk(mtx_);
    MGC_LOG_INFO("[FlightMode] Land requested");
    if (mavlink_) return mavlink_->set_mode(target_sys_, target_comp_, 0, 9);
    return false;
}

bool FlightModeManager::loiter() {
    mg::core::platform::LockGuard lk(mtx_);
    MGC_LOG_INFO("[FlightMode] Loiter requested");
    if (mavlink_) return mavlink_->set_mode(target_sys_, target_comp_, 0, 5);
    return false;
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
