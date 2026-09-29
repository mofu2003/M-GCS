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
#include "core/calibration/strategies/radio_calibration.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/transport/connection_manager.h"
#include "core/log/logging.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>

namespace mg::core::calibration {

CalibrationService::CalibrationService() {
    strategies_[0] = std::make_unique<GyroCalibration>();
    strategies_[1] = std::make_unique<AccelCalibration>();
    strategies_[2] = std::make_unique<CompassCalibration>();
    strategies_[3] = std::make_unique<BaroCalibration>();
    strategies_[4] = std::make_unique<RadioCalibration>();
    load_history_from_disk();
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
    EventDispatchScope dispatch(*this);
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
    status_.compass_progress.clear();
    status_.current_step = 0;
    status_.total_steps = type == CalibrationType::Accelerometer &&
            accel_mode_ != AccelCalMode::Full
        ? 1
        : strategy->total_steps();
    status_.user_message = std::string("Starting ") + strategy->type_name() + " calibration...";
    status_.failure_reason.clear();
    status_.retry_count = 0;
    status_.max_retries = retry_info_.max_retries;
    status_.start_time_ms = now_ms();
    status_.accel_mode = accel_mode_;

    current_result_ = CalibrationResult{};
    current_result_.type = type;
    current_result_.stage = CalibrationStage::WaitingForStart;
    current_result_.timestamp_ms = wall_clock_now_ms();
    current_result_.accel_mode = accel_mode_;
    current_result_.total_steps = status_.total_steps;

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
    EventDispatchScope dispatch(*this);
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
    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active || !active_strategy_) return;

    // Store raw text for debugging
    current_result_.raw_status_text = text;

    // Let the strategy parse the text
    auto parse_result = active_strategy_->parse_statustext(
        text, severity, status_.stage);

    if (!parse_result.is_calibration_text) return;

    if (status_.current_type == CalibrationType::Accelerometer &&
        accel_mode_ != AccelCalMode::Full) {
        parse_result.current_step = 0;
        parse_result.total_steps = 1;
        if (parse_result.new_stage == CalibrationStage::WaitingOrientation) {
            parse_result.new_stage = CalibrationStage::Calibrating;
            parse_result.stage_changed = status_.stage != CalibrationStage::Calibrating;
        }
    }

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
        // 失败时把飞控原文带进结果，否则 handle_failure 会退回 describe_failure
        // 的通用文案，真正的失败原因（飞控原话）就丢了。
        if (parse_result.new_stage == CalibrationStage::Failed &&
            !parse_result.user_message.empty()) {
            current_result_.failure_reason = parse_result.user_message;
        }
        transition_to(parse_result.new_stage);
    } else {
        // Even without stage change, fire event for progress updates
        fire_event();
    }
}

void CalibrationService::process_command_ack(uint16_t command, uint8_t result,
                                               uint8_t progress) {
    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active || !active_strategy_) return;

    // Only process ACK for our calibration command
    if (command != active_strategy_->mav_command()) return;

    auto new_stage = active_strategy_->interpret_ack(result, progress, status_.stage);
    // PREFLIGHT_CALIBRATION accepted only confirms that the six-face workflow
    // started. Completion is reported later by STATUSTEXT, not this ACK.
    if (status_.current_type == CalibrationType::Accelerometer &&
        accel_mode_ == AccelCalMode::Full && result == 0) {
        new_stage = CalibrationStage::Calibrating;
    }

    MGC_LOG_DEBUG("[CalService] ACK: cmd=%u result=%u progress=%u -> stage=%d",
                  command, result, progress, static_cast<int>(new_stage));

    // Update progress from ACK if available
    if (progress > 0 && progress <= 100) {
        status_.progress = progress;
        current_result_.progress = progress;
    }

    // 飞控拒绝时把 MAV_RESULT 原文带出来，不然界面只会看到通用文案，
    // 无法区分"被拒绝"和"校准过程中失败"。
    if (new_stage == CalibrationStage::Failed) {
        current_result_.failure_reason =
            "Flight controller rejected the calibration command (MAV_RESULT " +
            std::to_string(static_cast<unsigned>(result)) + ")";
    }
    transition_to(new_stage);
}

void CalibrationService::process_mag_cal_progress(
    const mg::core::mavlink::MagCalibrationProgressInfo& progress) {
    constexpr uint8_t kMagCalSuccess = 4;
    constexpr uint8_t kMagCalFailed = 5;

    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);
    if (!status_.active || status_.current_type != CalibrationType::Magnetometer) return;

    status_.progress = progress.completion_pct;
    current_result_.progress = progress.completion_pct;
    // 分罗盘记录：界面按识别到的罗盘数量各画一条进度条。
    status_.compass_progress[progress.compass_id] = progress.completion_pct;
    for (const auto& [id, pct] : status_.compass_progress) {
        (void)id;
        if (pct > status_.progress) status_.progress = pct;
    }
    status_.user_message = "Magnetometer calibration: compass " +
        std::to_string(static_cast<unsigned>(progress.compass_id) + 1) + " " +
        std::to_string(static_cast<unsigned>(progress.completion_pct)) + "%";

    if (progress.cal_status >= kMagCalFailed) {
        current_result_.failure_reason = "Flight controller reported magnetometer calibration failure (status " +
            std::to_string(static_cast<unsigned>(progress.cal_status)) + ")";
        transition_to(CalibrationStage::Failed);
        return;
    }

    // A progress packet can reach 100% before the final report.  Wait for
    // MAG_CAL_REPORT so the result is not accepted prematurely.
    if (progress.cal_status != kMagCalSuccess) {
        status_.stage = CalibrationStage::Calibrating;
        current_result_.stage = CalibrationStage::Calibrating;
    }
    fire_event();
}

void CalibrationService::process_mag_cal_report(
    const mg::core::mavlink::MagCalibrationReportInfo& report) {
    constexpr uint8_t kMagCalSuccess = 4;
    constexpr uint8_t kMagCalFailed = 5;

    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);
    if (!status_.active || status_.current_type != CalibrationType::Magnetometer) return;

    if (report.cal_status >= kMagCalFailed) {
        current_result_.failure_reason = "Flight controller reported magnetometer calibration failure (status " +
            std::to_string(static_cast<unsigned>(report.cal_status)) + ")";
        status_.user_message = current_result_.failure_reason;
        transition_to(CalibrationStage::Failed);
        return;
    }
    if (report.cal_status != kMagCalSuccess) return;

    current_result_.mag_report_received = true;
    current_result_.fitness = report.fitness;
    current_result_.offset_x = report.offset_x;
    current_result_.offset_y = report.offset_y;
    current_result_.offset_z = report.offset_z;
    current_result_.diagonal_x = report.diagonal_x;
    current_result_.diagonal_y = report.diagonal_y;
    current_result_.diagonal_z = report.diagonal_z;
    current_result_.offdiag_x = report.offdiag_x;
    current_result_.offdiag_y = report.offdiag_y;
    current_result_.offdiag_z = report.offdiag_z;
    current_result_.progress = 100;
    status_.progress = 100;
    status_.user_message = report.autosaved
        ? "Magnetometer calibration completed and saved by flight controller"
        : "Magnetometer calibration completed by flight controller";
    transition_to(CalibrationStage::Success);
}

void CalibrationService::process_accelerometer_position_request(uint32_t position) {
    constexpr uint32_t kAccelCalSuccess = 16777215;
    constexpr uint32_t kAccelCalFailed = 16777216;

    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);
    if (!status_.active ||
        status_.current_type != CalibrationType::Accelerometer ||
        accel_mode_ != AccelCalMode::Full) {
        return;
    }

    if (position == kAccelCalSuccess) {
        status_.current_step = 6;
        status_.total_steps = 6;
        status_.progress = 100;
        status_.user_message = "Accelerometer calibration completed by flight controller";
        current_result_.current_step = 6;
        current_result_.total_steps = 6;
        current_result_.progress = 100;
        transition_to(CalibrationStage::Success);
        return;
    }

    if (position == kAccelCalFailed) {
        current_result_.failure_reason = "Flight controller reported accelerometer calibration failure";
        status_.user_message = current_result_.failure_reason;
        transition_to(CalibrationStage::Failed);
        return;
    }

    if (position < 1 || position > 6) return;

    const auto step = static_cast<uint8_t>(position);
    status_.current_step = step;
    status_.total_steps = 6;
    status_.progress = static_cast<uint8_t>((step - 1) * 100 / 6);
    status_.user_message = "Place vehicle for accelerometer calibration step " +
                           std::to_string(step);
    current_result_.current_step = step;
    current_result_.total_steps = 6;
    current_result_.progress = status_.progress;
    transition_to(CalibrationStage::WaitingOrientation);
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
    // NOTE: caller must hold mtx_. The enclosing EventDispatchScope invokes
    // fire_event_safe after the lock is released.
    if (event_cb_) {
        pending_events_.push_back(status_);
    }
}

void CalibrationService::fire_event_safe() {
    // Self-locks, copies callback+state, invokes outside lock.
    // Safe for re-entrant callbacks. Callers must NOT hold mtx_.
    std::vector<CalibrationStatus> pending;
    CalibrationEventCallback cb_fn;
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (pending_events_.empty()) return;
        pending.swap(pending_events_);
        cb_fn = event_cb_;
    }
    if (cb_fn) {
        for (const auto& status : pending) {
            cb_fn(status);
        }
    }
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
    current_result_.timestamp_ms = wall_clock_now_ms();
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
    // The MAV_CAL protocol supplies a final status for each compass.  Retrying
    // from the GCS races the flight controller's own state and was the source
    // of the misleading "Retrying 3/3" loop.  Leave the failed result visible
    // and let the operator explicitly start a new attempt.
    if (status_.current_type != CalibrationType::Magnetometer &&
        validator_.should_retry(current_result_, retry_info_)) {
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
    current_result_.timestamp_ms = wall_clock_now_ms();
    current_result_.accel_mode = accel_mode_;

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
        persist_history();
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

uint64_t CalibrationService::wall_clock_now_ms() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

std::string CalibrationService::history_storage_path() const {
    namespace fs = std::filesystem;
#if defined(MGCORE_TEST_BUILD)
    return (fs::temp_directory_path() / "M-GCS-test" / "calibration-history.v1").string();
#else
    const char* home = std::getenv("HOME");
#if defined(_WIN32)
    const char* app_data = std::getenv("LOCALAPPDATA");
    if (app_data && *app_data) {
        return (fs::path(app_data) / "M-GCS" / "calibration-history.v1").string();
    }
#elif defined(__APPLE__)
    if (home && *home) {
        return (fs::path(home) / "Library" / "Application Support" / "M-GCS" /
                "calibration-history.v1").string();
    }
#else
    const char* state_home = std::getenv("XDG_STATE_HOME");
    if (state_home && *state_home) {
        return (fs::path(state_home) / "M-GCS" / "calibration-history.v1").string();
    }
    if (home && *home) {
        return (fs::path(home) / ".local" / "state" / "M-GCS" /
                "calibration-history.v1").string();
    }
#endif
    return (fs::temp_directory_path() / "M-GCS" / "calibration-history.v1").string();
#endif
}

void CalibrationService::load_history_from_disk() {
    std::ifstream input(history_storage_path());
    if (!input.is_open()) return;

    std::string version;
    int type = 0;
    int stage = 0;
    int quality = 0;
    int accel_mode = static_cast<int>(AccelCalMode::Full);
    int progress = 0;
    int current_step = 0;
    int total_steps = 0;
    CalibrationResult result;
    while (input >> version >> type >> stage >> quality >> result.timestamp_ms >> accel_mode >>
           result.offset_x >> result.offset_y >> result.offset_z >> result.scale_x >> result.scale_y >>
           result.scale_z >> result.fitness >> result.diagonal_x >> result.diagonal_y >> result.diagonal_z >>
           result.offdiag_x >> result.offdiag_y >> result.offdiag_z >> result.prev_offset_x >>
           result.prev_offset_y >> result.prev_offset_z >> progress >> current_step >> total_steps >>
           std::quoted(result.failure_reason)) {
        if (version != "MGCAL1" || type < 0 || type >= static_cast<int>(history_.size()) ||
            stage < static_cast<int>(CalibrationStage::Idle) ||
            stage > static_cast<int>(CalibrationStage::Cancelled) ||
            quality < static_cast<int>(CalibrationQuality::Excellent) ||
            quality > static_cast<int>(CalibrationQuality::Poor) ||
            (accel_mode != static_cast<int>(AccelCalMode::Full) &&
             accel_mode != static_cast<int>(AccelCalMode::Trim) &&
             accel_mode != static_cast<int>(AccelCalMode::Simple)) ||
            progress < 0 || progress > 100 || current_step < 0 || current_step > 255 ||
            total_steps < 0 || total_steps > 255) {
            continue;
        }
        result.type = static_cast<CalibrationType>(type);
        result.stage = static_cast<CalibrationStage>(stage);
        result.quality = static_cast<CalibrationQuality>(quality);
        result.accel_mode = static_cast<AccelCalMode>(accel_mode);
        result.progress = static_cast<uint8_t>(progress);
        result.current_step = static_cast<uint8_t>(current_step);
        result.total_steps = static_cast<uint8_t>(total_steps);
        auto& history = history_[static_cast<size_t>(type)];
        history.push_back(std::move(result));
        while (history.size() > static_cast<size_t>(kMaxCalibrationHistory)) {
            history.erase(history.begin());
        }
    }
}

void CalibrationService::persist_history() const {
    namespace fs = std::filesystem;
    const fs::path path(history_storage_path());
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) {
        MGC_LOG_WARN("[CalService] Unable to create calibration history directory: %s", error.message().c_str());
        return;
    }

    const fs::path temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output.is_open()) {
        MGC_LOG_WARN("[CalService] Unable to write calibration history");
        return;
    }
    output << std::setprecision(9);
    for (const auto& type_history : history_) {
        for (const auto& result : type_history) {
            output << "MGCAL1 " << static_cast<int>(result.type) << ' '
                   << static_cast<int>(result.stage) << ' '
                   << static_cast<int>(result.quality) << ' ' << result.timestamp_ms << ' '
                   << static_cast<int>(result.accel_mode) << ' ' << result.offset_x << ' '
                   << result.offset_y << ' ' << result.offset_z << ' ' << result.scale_x << ' '
                   << result.scale_y << ' ' << result.scale_z << ' ' << result.fitness << ' '
                   << result.diagonal_x << ' ' << result.diagonal_y << ' ' << result.diagonal_z << ' '
                   << result.offdiag_x << ' ' << result.offdiag_y << ' ' << result.offdiag_z << ' '
                   << result.prev_offset_x << ' ' << result.prev_offset_y << ' '
                   << result.prev_offset_z << ' ' << static_cast<int>(result.progress) << ' '
                   << static_cast<int>(result.current_step) << ' ' << static_cast<int>(result.total_steps)
                   << ' ' << std::quoted(result.failure_reason) << '\n';
        }
    }
    output.close();
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(path, error);
        error.clear();
        fs::rename(temporary, path, error);
    }
    if (error) {
        MGC_LOG_WARN("[CalService] Unable to finalize calibration history: %s", error.message().c_str());
    }
}

} // namespace mg::core::calibration
