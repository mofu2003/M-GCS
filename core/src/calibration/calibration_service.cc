/**
 * calibration_service.cc - Unified sensor calibration service
 * 校准服务实现 — 完整校准生命周期管理
 *
 * Manages the complete calibration lifecycle:
 *   1. Start ? send MAVLink command via strategy
 *   2. Process STATUSTEXT ? parse progress/orientation via strategy
 *   3. Process ACK ? interpret result via strategy
 *   4. Verify ? read FC params + assess quality via validator
 *   5. Retry on failure (up to max_retries)
 *   6. Save result to history buffer
 */

#include "core/calibration/calibration_service.h"
#include "core/calibration/strategies/gyro_calibration.h"
#include "core/calibration/strategies/accel_calibration.h"
#include "core/calibration/strategies/compass_calibration.h"
#include "core/calibration/strategies/baro_calibration.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/transport/connection_manager.h"
#include "core/log/logging.h"
#include <chrono>
#include <thread>

namespace mg::core::calibration {

CalibrationService::CalibrationService() {
    strategies_[0] = std::make_unique<GyroCalibration>();
    strategies_[1] = std::make_unique<AccelCalibration>();
    strategies_[2] = std::make_unique<CompassCalibration>();
    strategies_[3] = std::make_unique<BaroCalibration>();
}

CalibrationService::~CalibrationService() {
    // Ensure no active calibration on destruction
    mg::core::platform::LockGuard lk(mtx_);
    if (status_.active) {
        status_.active = false;
        status_.stage = CalibrationStage::Cancelled;
    }
}

void CalibrationService::set_event_callback(CalibrationEventCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    event_cb_ = std::move(cb);
}

bool CalibrationService::start(CalibrationType type,
                                AccelCalMode accel_mode) {
    mg::core::platform::LockGuard lk(mtx_);

    if (status_.active) {
        MGC_LOG_WARN("[CalService] Calibration already in progress, cannot start new one");
        return false;
    }

    auto* strategy = get_strategy(type);
    if (!strategy) {
        MGC_LOG_ERROR("[CalService] Unknown calibration type: %d", static_cast<int>(type));
        return false;
    }

    accel_mode_ = accel_mode;

    if (!conn_ || !conn_->get_mavlink()) {
        MGC_LOG_ERROR("[CalService] Null MavlinkAdapter");
        return false;
    }

    // Initialize state
    active_strategy_ = strategy;


    status_.active = true;
    status_.current_type = type;
    status_.stage = CalibrationStage::WaitingForStart;
    status_.progress = 0;
    status_.current_step = 0;
    status_.total_steps = strategy->total_steps();
    status_.user_message = std::string("Starting ") + strategy->type_name() + " calibration...";
    status_.failure_reason.clear();
    status_.retry_count = 0;
    status_.max_retries = retry_info_.max_retries;
    status_.start_time_ms = now_ms();

    current_result_ = CalibrationResult{};
    current_result_.type = type;
    current_result_.stage = CalibrationStage::WaitingForStart;
    current_result_.timestamp_ms = status_.start_time_ms;

    retry_info_.retry_count = 0;
    retry_info_.last_failure.clear();

    // Save pre-calibration parameter values for comparison
    validator_.read_calibration_params(type, conn_->get_mavlink(), current_result_);
    current_result_.prev_offset_x = current_result_.offset_x;
    current_result_.prev_offset_y = current_result_.offset_y;
    current_result_.prev_offset_z = current_result_.offset_z;

    MGC_LOG_INFO("[CalService] Starting %s calibration (sys=%u comp=%u)",
                 strategy->type_name(), target_sys_, target_comp_);

    fire_event();

    // Send the calibration command
    auto* mav = conn_->get_mavlink();
    if (!strategy->send_calibration_command(mav, target_sys_, target_comp_, accel_mode)) {
        handle_failure("Failed to send calibration command");
        return false;
    }

    // Transition to Calibrating
    transition_to(CalibrationStage::Calibrating);
    return true;
}

bool CalibrationService::cancel() {
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active) return false;

    MGC_LOG_INFO("[CalService] Calibration cancelled by user");
    status_.stage = CalibrationStage::Cancelled;
    status_.active = false;
    current_result_.stage = CalibrationStage::Cancelled;
    current_result_.failure_reason = "Cancelled by user";
    save_to_history();
    fire_event();
    return true;
}

void CalibrationService::process_statustext(const std::string& text, uint8_t severity) {
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active || !active_strategy_) return;

    // Store raw text for debugging
    current_result_.raw_status_text = text;

    // Let the strategy parse the text
    auto parse_result = active_strategy_->parse_statustext(
        text, severity, status_.stage);

    if (!parse_result.is_calibration_text) return;

    MGC_LOG_DEBUG("[CalService] STATUSTEXT: \"%s\" -> stage=%d progress=%u step=%u/%u",
                  text.c_str(), static_cast<int>(parse_result.new_stage),
                  parse_result.progress, parse_result.current_step,
                  parse_result.total_steps);

    // Update progress/step
    if (parse_result.progress > 0) {
        status_.progress = parse_result.progress;
        current_result_.progress = parse_result.progress;
    }
    if (parse_result.current_step > 0) {
        status_.current_step = parse_result.current_step;
        current_result_.current_step = parse_result.current_step;
    }
    if (parse_result.total_steps > 0) {
        status_.total_steps = parse_result.total_steps;
        current_result_.total_steps = parse_result.total_steps;
    }

    // Update user message
    if (!parse_result.user_message.empty()) {
        status_.user_message = parse_result.user_message;
    }

    // Handle stage transition
    if (parse_result.stage_changed) {
        transition_to(parse_result.new_stage);
    } else {
        // Even without stage change, fire event for progress updates
        fire_event();
    }
}

void CalibrationService::process_command_ack(uint16_t command, uint8_t result,
                                               uint8_t progress) {
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active || !active_strategy_) return;

    // Only process ACK for our calibration command
    if (command != active_strategy_->mav_command()) return;

    auto new_stage = active_strategy_->interpret_ack(result, progress, status_.stage);

    MGC_LOG_DEBUG("[CalService] ACK: cmd=%u result=%u progress=%u -> stage=%d",
                  command, result, progress, static_cast<int>(new_stage));

    // Update progress from ACK if available
    if (progress > 0 && progress <= 100) {
        status_.progress = progress;
        current_result_.progress = progress;
    }

    transition_to(new_stage);
}

CalibrationStatus CalibrationService::get_status() const {
    mg::core::platform::LockGuard lk(mtx_);
    return status_;
}

std::vector<CalibrationResult> CalibrationService::get_history(CalibrationType type) const {
    mg::core::platform::LockGuard lk(mtx_);
    auto idx = static_cast<size_t>(type);
    if (idx < history_.size()) {
        return history_[idx];
    }
    return {};
}

bool CalibrationService::is_active() const {
    mg::core::platform::LockGuard lk(mtx_);
    return status_.active;
}

// ================================================================
// Internal helpers
// ================================================================

void CalibrationService::transition_to(CalibrationStage new_stage) {
    // NOTE: caller must hold mtx_

    CalibrationStage old_stage = status_.stage;
    status_.stage = new_stage;
    current_result_.stage = new_stage;

    MGC_LOG_DEBUG("[CalService] Stage transition: %s -> %s",
                  CalibrationValidator::stage_string(old_stage),
                  CalibrationValidator::stage_string(new_stage));

    switch (new_stage) {
        case CalibrationStage::Success:
            handle_success();
            break;
        case CalibrationStage::Failed:
            handle_failure(current_result_.failure_reason);
            break;
        case CalibrationStage::Cancelled:
            status_.active = false;
            save_to_history();
            break;
        default:
            break;
    }

    fire_event();
}

void CalibrationService::fire_event() {
    // NOTE: caller must hold mtx_.
    // Callbacks route to FFI post_event() which never re-enters CalibrationService,
    // so the held lock does not cause deadlock in practice.
    if (event_cb_) event_cb_(status_);
}

void CalibrationService::fire_event_safe() {
    // Self-locks, copies callback+state, invokes outside lock.
    // Safe for re-entrant callbacks. Callers must NOT hold mtx_.
    CalibrationStatus copy;
    CalibrationEventCallback cb_fn;
    {
        mg::core::platform::LockGuard lk(mtx_);
        copy = status_;
        cb_fn = event_cb_;
    }
    if (cb_fn) cb_fn(copy);
}

void CalibrationService::handle_success() {
    // NOTE: caller must hold mtx_

    // Verify calibration parameters
    status_.stage = CalibrationStage::Verifying;
    current_result_.stage = CalibrationStage::Verifying;
    fire_event();

    if (active_strategy_ && conn_) {
        // Release lock for parameter reading (may take time)
        // Actually, we hold the lock - this is fine for synchronous operation
        bool verified = active_strategy_->verify_calibration(
            conn_->get_mavlink(), current_result_, validator_);
        current_result_.quality = validator_.assess(current_result_);

        if (!verified && current_result_.quality == CalibrationQuality::Poor) {
            MGC_LOG_WARN("[CalService] Calibration verification failed - quality is Poor");
            current_result_.stage = CalibrationStage::Failed;
            current_result_.failure_reason = "Calibration quality is poor. "
                                             "Parameters verified but values are out of acceptable range.";
            status_.stage = CalibrationStage::Failed;
            status_.failure_reason = current_result_.failure_reason;
            status_.active = false;
            save_to_history();
            return;
        }
    }

    // Success
    status_.stage = CalibrationStage::Success;
    status_.progress = 100;
    status_.active = false;
    status_.failure_reason.clear();
    status_.user_message = std::string(CalibrationValidator::type_string(status_.current_type)) +
                           " calibration successful (quality: " +
                           CalibrationValidator::quality_string(current_result_.quality) + ")";
    current_result_.stage = CalibrationStage::Success;
    current_result_.failure_reason.clear();
    current_result_.timestamp_ms = now_ms();
    save_to_history();

    MGC_LOG_INFO("[CalService] Calibration successful: %s quality=%s",
                 CalibrationValidator::type_string(current_result_.type),
                 CalibrationValidator::quality_string(current_result_.quality));
}

void CalibrationService::handle_failure(const std::string& reason) {
    // NOTE: caller must hold mtx_

    status_.stage = CalibrationStage::Failed;
    status_.failure_reason = reason.empty()
        ? validator_.describe_failure(current_result_)
        : reason;
    current_result_.stage = CalibrationStage::Failed;
    current_result_.failure_reason = status_.failure_reason;

    MGC_LOG_WARN("[CalService] Calibration failed: %s", status_.failure_reason.c_str());

    // Check if retry is possible
    if (validator_.should_retry(current_result_, retry_info_)) {
        attempt_retry();
    } else {
        status_.active = false;
        save_to_history();
    }
}

void CalibrationService::attempt_retry() {
    // NOTE: caller must hold mtx_

    retry_info_.retry_count++;
    retry_info_.last_failure = current_result_.failure_reason;
    status_.retry_count = retry_info_.retry_count;

    MGC_LOG_INFO("[CalService] Retrying calibration (attempt %d/%d) after %d ms",
                 retry_info_.retry_count, retry_info_.max_retries,
                 retry_info_.backoff_ms);

    status_.user_message = "Retrying (" + std::to_string(retry_info_.retry_count) +
                           "/" + std::to_string(retry_info_.max_retries) +
                           "): " + current_result_.failure_reason;

    fire_event();

    // Reset result for retry
    current_result_ = CalibrationResult{};
    current_result_.type = status_.current_type;
    current_result_.timestamp_ms = now_ms();

    // Re-send calibration command
    if (active_strategy_ && conn_) {
        // Brief delay before retry (user-visible feedback)
        // Note: In production this would be async, but for now synchronous
        status_.stage = CalibrationStage::Calibrating;
        current_result_.stage = CalibrationStage::Calibrating;
        bool sent = active_strategy_->send_calibration_command(
            conn_->get_mavlink(), target_sys_, target_comp_, accel_mode_);
        if (!sent) {
            status_.stage = CalibrationStage::Failed;
            status_.active = false;
            status_.failure_reason = "failed to send calibration command during retry";
            current_result_.stage = CalibrationStage::Failed;
            current_result_.failure_reason = status_.failure_reason;
        } else {
            fire_event();
        }
    }
}

void CalibrationService::save_to_history() {
    // NOTE: caller must hold mtx_
    auto idx = static_cast<size_t>(current_result_.type);
    if (idx < history_.size()) {
        history_[idx].push_back(current_result_);
        // Keep only last N results
        while (history_[idx].size() > static_cast<size_t>(kMaxCalibrationHistory)) {
            history_[idx].erase(history_[idx].begin());
        }
    }
}

void CalibrationService::update_status_from_result() {
    // NOTE: caller must hold mtx_
    status_.stage = current_result_.stage;
    status_.progress = current_result_.progress;
    status_.current_step = current_result_.current_step;
    status_.total_steps = current_result_.total_steps;
}

ICalibrationStrategy* CalibrationService::get_strategy(CalibrationType type) {
    auto idx = static_cast<size_t>(type);
    if (idx < strategies_.size()) {
        return strategies_[idx].get();
    }
    return nullptr;
}

uint64_t CalibrationService::now_ms() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

} // namespace mg::core::calibration



