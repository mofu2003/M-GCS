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

namespace {
    // ArduPilot-specific MAVLink command codes
    constexpr uint16_t MAV_CMD_PREFLIGHT_CALIBRATION_CMD = 241;      // Calibration
    constexpr uint16_t MAV_CMD_DO_CANCEL_MAG_CAL_CMD = 42426;         // Cancel magnetometer calibration
    constexpr uint16_t MAV_CMD_ACCELCAL_VEHICLE_POS_CMD = 42429;      // Accelerometer calibration position
}
#include "core/transport/connection_manager.h"
#include "core/log/logging.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <thread>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace mg::core::calibration {

namespace {

/// 这条 PREFLIGHT_CALIBRATION 的 COMMAND_ACK 返回时，校准是否已经真的做完了？
///
/// MAVLink 语义上 ACCEPTED 只表示"命令被受理"，策略层据此统一返回"校准中"
/// （gyro/baro/accel 与 radio/compass 一致）。但 ArduPilot 有几条预检校准是
/// 同步完成的：handler 在校准结束后才回 ACCEPTED，而且不另发结果 STATUSTEXT，
/// ACCEPTED 就是唯一信号：
///   * 陀螺：`AP_InertialSensor::calibrate_gyros()` → `_init_gyro()` 同步跑完，
///     全程只有 DEV_PRINTF，没有任何 "Gyro calibration ..." STATUSTEXT；
///   * 简易/板级加速度计：`AP_InertialSensor::simple_accel_cal()` /
///     `calibrate_trim()` 同样同步返回，文本只有 "Trim OK: ..." 之类。
/// 六面加速度计（Full）不在此列：由 STATUSTEXT / ACCELCAL_VEHICLE_POS 握手驱动。
/// 气压计也不在此列：`_handle_command_preflight_calibration_baro()` 在返回
/// ACCEPTED 之前就发了 "Barometer calibration complete"。
///
/// 补偿只把 ACCEPTED 当成"命令已完成"的信号，真伪仍由 handle_success 的参数
/// 回读判定（读不回来就落 Failed）。
bool ack_reports_synchronous_completion(CalibrationType type,
                                        AccelCalMode accel_mode) {
    if (type == CalibrationType::Gyroscope) return true;
    return type == CalibrationType::Accelerometer &&
           (accel_mode == AccelCalMode::Simple || accel_mode == AccelCalMode::Trim);
}

} // namespace

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

    const auto mav = conn_ ? conn_->shared_mavlink() : nullptr;
    if (!mav) {
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
    compass_expected_ids_.clear();
    compass_reports_received_.clear();
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
    validator_.read_calibration_params(type, mav.get(), current_result_);
    current_result_.prev_offset_x = current_result_.offset_x;
    current_result_.prev_offset_y = current_result_.offset_y;
    current_result_.prev_offset_z = current_result_.offset_z;

    MGC_LOG_INFO("[CalService] Starting %s calibration (sys=%u comp=%u)",
                 strategy->type_name(), target_sys_, target_comp_);

    fire_event();

    // Send the calibration command
    if (!strategy->send_calibration_command(mav.get(), target_sys_, target_comp_, accel_mode)) {
        handle_failure("Failed to send calibration command");
        return false;
    }

    // Transition to Calibrating
    last_progress_ms_ = now_ms();

    transition_to(CalibrationStage::Calibrating);
    return true;
}

bool CalibrationService::cancel() {
    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active) return false;

    if (status_.stage == CalibrationStage::Cancelling) {
        // 已经在取消中：取消命令由第一次请求发出，这里只表示“请求已受理”，
        // 不重复下发，也不提前收尾。
        return true;
    }

    MGC_LOG_INFO("[CalService] Calibration cancel requested by user (type=%s)",
                 active_strategy_ ? active_strategy_->type_name() : "unknown");

    // 只进入 Cancelling：取消命令的实际下发由 FFI 层完成，飞控确认（或取消
    // 超时/链路断开）之前 active 保持 true，界面不得显示为已停止（S8）。
    status_.stage = CalibrationStage::Cancelling;
    status_.user_message = "Cancelling calibration...";
    current_result_.stage = CalibrationStage::Cancelling;
    current_result_.failure_reason = "Cancelled by user";
    cancel_requested_ms_ = now_ms();
    fire_event();
    return true;
}

void CalibrationService::poll() {
    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active) return;

    // 取消在途：等飞控确认。取消是操作者意图，不是失败，不触发重试；链路断开
    // 或等待超过 cancel_timeout_ms_ 就收尾为 Cancelled，避免界面卡在"取消中"。
    if (status_.stage == CalibrationStage::Cancelling) {
        if (conn_ && !conn_->is_connected()) {
            finalize_cancelled(
                "Cancelled by user (link lost before the flight controller confirmed)");
            return;
        }
        const uint64_t cancel_now = now_ms();
        if (cancel_requested_ms_ != 0 && cancel_now > cancel_requested_ms_ &&
            cancel_now - cancel_requested_ms_ > cancel_timeout_ms_) {
            finalize_cancelled(
                "Cancelled by user (flight controller did not confirm within " +
                std::to_string(cancel_timeout_ms_ / 1000) + "s)");
        }
        return;
    }

    // 链路断开后飞控不可能继续上报进度，直接失败，避免界面永远停在"校准中"。
    if (conn_ && !conn_->is_connected()) {
        // 链路都没了，重试没有意义：把重试额度用掉，让 handle_failure 直接收尾。
        retry_info_.retry_count = retry_info_.max_retries;
        handle_failure("Calibration aborted: link lost");
        return;
    }

    // 只有采样阶段才判定停滞。WaitingForStart / WaitingOrientation 是等用户操作的
    // 阶段（例如加速度计要人搬动机体），把等待时间算进去会误判超时。
    if (status_.stage != CalibrationStage::Calibrating) return;
    if (last_progress_ms_ == 0) return;

    const uint64_t now = now_ms();
    if (now > last_progress_ms_ && now - last_progress_ms_ > stall_timeout_ms_) {
        handle_failure("Calibration timed out: no progress from the flight controller for " +
                       std::to_string(stall_timeout_ms_ / 1000) + "s");
    }
}

void CalibrationService::process_statustext(const std::string& text, uint8_t severity) {
    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active || !active_strategy_) return;
    // 取消在途：忽略飞控残留的进度/结果文本，不回退也不复活已取消的状态。
    if (status_.stage == CalibrationStage::Cancelling) return;

    // Store raw text for debugging
    current_result_.raw_status_text = text;

    // Let the strategy parse the text
    auto parse_result = active_strategy_->parse_statustext(
        text, severity, status_.stage);

    if (!parse_result.is_calibration_text) return;

    last_progress_ms_ = now_ms();

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

    // 取消在途：只有 ACCEPTED 才确认飞控已停止。DENIED/FAILED/IN_PROGRESS
    // 都不能被转换成成功，继续保持 cancelling，交给看门狗处理无确认场景。
    if (status_.stage == CalibrationStage::Cancelling) {
        if (!is_cancel_command(command)) return;
        if (result != 0) {
            MGC_LOG_WARN("[CalService] Flight controller did not accept cancel (cmd=%u result=%u)",
                         static_cast<unsigned>(command), static_cast<unsigned>(result));
            current_result_.failure_reason =
                "Flight controller did not accept calibration cancellation (MAV_RESULT " +
                std::to_string(static_cast<unsigned>(result)) + ")";
            status_.user_message = current_result_.failure_reason;
            fire_event();
            return;
        }
        MGC_LOG_INFO("[CalService] Cancel confirmed by flight controller (cmd=%u result=%u)",
                     static_cast<unsigned>(command), static_cast<unsigned>(result));
        finalize_cancelled("Cancelled by user");
        return;
    }

    // Only process ACK for our calibration command
    if (command != active_strategy_->mav_command()) return;

    last_progress_ms_ = now_ms();

    auto new_stage = active_strategy_->interpret_ack(result, progress, status_.stage);
    // PREFLIGHT_CALIBRATION accepted only confirms that the six-face workflow
    // started. Completion is reported later by STATUSTEXT, not this ACK.
    if (status_.current_type == CalibrationType::Accelerometer &&
        accel_mode_ == AccelCalMode::Full && result == 0) {
        new_stage = CalibrationStage::Calibrating;
    } else if (result == 0 &&
               ack_reports_synchronous_completion(status_.current_type,
                                                  accel_mode_)) {
        // 同步完成的固件（ArduPilot 的陀螺/简易/板级加速度计）没有后续
        // STATUSTEXT，ACCEPTED 就是"命令已完成"的信号；真伪交给
        // handle_success 的参数回读。
        new_stage = CalibrationStage::Success;
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
    if (status_.stage == CalibrationStage::Cancelling) return;

    last_progress_ms_ = now_ms();

    for (uint8_t id = 0; id < 8; ++id) {
        if ((progress.cal_mask & (static_cast<uint8_t>(1u) << id)) != 0) {
            compass_expected_ids_.insert(id);
        }
    }
    compass_expected_ids_.insert(progress.compass_id);

    status_.progress = progress.completion_pct;
    current_result_.progress = progress.completion_pct;
    // 分罗盘记录：界面按识别到的罗盘数量各画一条进度条。
    const auto previous = status_.compass_progress.find(progress.compass_id);
    const bool progress_changed =
        previous == status_.compass_progress.end() ||
        previous->second != progress.completion_pct;
    status_.compass_progress[progress.compass_id] = progress.completion_pct;
    for (const auto& [id, pct] : status_.compass_progress) {
        (void)id;
        if (pct > status_.progress) status_.progress = pct;
    }
    status_.user_message = "Magnetometer calibration: compass " +
        std::to_string(static_cast<unsigned>(progress.compass_id) + 1) + " " +
        std::to_string(static_cast<unsigned>(progress.completion_pct)) + "%";
    if (progress_changed) {
        MGC_LOG_DEBUG("[CalService] MAG_CAL_PROGRESS compass=%u pct=%u status=%u",
                      static_cast<unsigned>(progress.compass_id),
                      static_cast<unsigned>(progress.completion_pct),
                      static_cast<unsigned>(progress.cal_status));
    }

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
    if (status_.stage == CalibrationStage::Cancelling) return;

    last_progress_ms_ = now_ms();

    if (report.cal_status >= kMagCalFailed) {
        current_result_.failure_reason = "Flight controller reported magnetometer calibration failure (status " +
            std::to_string(static_cast<unsigned>(report.cal_status)) + ")";
        status_.user_message = current_result_.failure_reason;
        transition_to(CalibrationStage::Failed);
        return;
    }
    if (report.cal_status != kMagCalSuccess) return;

    for (uint8_t id = 0; id < 8; ++id) {
        if ((report.cal_mask & (static_cast<uint8_t>(1u) << id)) != 0) {
            compass_expected_ids_.insert(id);
        }
    }
    compass_expected_ids_.insert(report.compass_id);
    compass_reports_received_.insert(report.compass_id);

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
    const bool all_reports_received = std::all_of(
        compass_expected_ids_.begin(), compass_expected_ids_.end(),
        [&](uint8_t id) { return compass_reports_received_.count(id) != 0; });
    if (!all_reports_received) {
        status_.stage = CalibrationStage::Calibrating;
        current_result_.stage = CalibrationStage::Calibrating;
        status_.progress = std::min<uint8_t>(status_.progress, 99);
        status_.user_message = "Waiting for all compass calibration reports";
        fire_event();
        return;
    }
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
    if (status_.stage == CalibrationStage::Cancelling) return;

    last_progress_ms_ = now_ms();

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

bool CalibrationService::is_cancel_command(uint16_t command) {
    // 本服务的取消路径只可能发这三条命令（见 bridge/ffi/ffi_calibration.cc）：
    //  MAV_CMD_PREFLIGHT_CALIBRATION（陀螺/气压/遥控，全零参数）
    //  MAV_CMD_DO_CANCEL_MAG_CAL（罗盘）
    //  MAV_CMD_ACCELCAL_VEHICLE_POS（加速度计，position 16777216 = 取消）
    return command == MAV_CMD_PREFLIGHT_CALIBRATION_CMD ||
           command == MAV_CMD_DO_CANCEL_MAG_CAL_CMD ||
           command == MAV_CMD_ACCELCAL_VEHICLE_POS_CMD;
}

bool CalibrationService::finish_cancel(const std::string& reason) {
    EventDispatchScope dispatch(*this);
    mg::core::platform::LockGuard lk(mtx_);

    if (!status_.active || status_.stage != CalibrationStage::Cancelling) return false;
    finalize_cancelled(reason);
    return true;
}

void CalibrationService::finalize_cancelled(const std::string& reason) {
    // NOTE: caller must hold mtx_

    MGC_LOG_INFO("[CalService] Calibration cancellation finalized: %s", reason.c_str());

    status_.stage = CalibrationStage::Cancelled;
    status_.active = false;
    status_.user_message = reason;
    status_.failure_reason.clear();
    current_result_.stage = CalibrationStage::Cancelled;
    current_result_.failure_reason = reason;
    current_result_.timestamp_ms = wall_clock_now_ms();
    cancel_requested_ms_ = 0;
    save_to_history();
    fire_event();
}

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
    // 校准历史的落盘也在这里完成：锁内只取快照，文件 I/O 在锁外。
    std::vector<CalibrationStatus> pending;
    CalibrationEventCallback cb_fn;
    std::array<std::vector<CalibrationResult>, 5> history_snapshot;
    bool flush_history = false;
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (pending_events_.empty() && !persist_dirty_) return;
        pending.swap(pending_events_);
        cb_fn = event_cb_;
        if (persist_dirty_) {
            persist_dirty_ = false;
            flush_history = true;
            history_snapshot = history_;
        }
    }
    if (flush_history) {
        write_history_file(history_snapshot);
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
        const auto mav = conn_->shared_mavlink();
        bool verified = active_strategy_->verify_calibration(
            mav.get(), current_result_, validator_);
        current_result_.quality = validator_.assess(current_result_);

        if (!verified) {
            MGC_LOG_WARN("[CalService] Calibration verification failed: %s",
                         current_result_.failure_reason.empty()
                             ? "parameter readback or quality check failed"
                             : current_result_.failure_reason.c_str());
            current_result_.stage = CalibrationStage::Failed;
            if (current_result_.failure_reason.empty()) {
                current_result_.failure_reason =
                    "Calibration verification failed: parameter readback or quality check failed.";
            }
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
        const auto mav = conn_->shared_mavlink();
        bool sent = active_strategy_->send_calibration_command(
            mav.get(), target_sys_, target_comp_, accel_mode_);
        if (!sent) {
            status_.stage = CalibrationStage::Failed;
            status_.active = false;
            status_.failure_reason = "failed to send calibration command during retry";
            current_result_.stage = CalibrationStage::Failed;
            current_result_.failure_reason = status_.failure_reason;
        } else {
            // 重试等于重新开始一轮采样，重置停滞计时。
            last_progress_ms_ = now_ms();
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
        // 只打脏标记；真正的写盘在 fire_event_safe 的锁外路径完成。
        persist_dirty_ = true;
    }
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
    // 每进程唯一路径，避免并发测试时互相覆盖。必须与 test_sim_calibration.cc 同步。
    return (fs::temp_directory_path() / ("M-GCS-test-" + std::to_string(
#if defined(_WIN32)
        ::_getpid()
#else
        ::getpid()
#endif
    )) / "calibration-history.v1").string();
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
            stage > static_cast<int>(CalibrationStage::Cancelling) ||
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

void CalibrationService::write_history_file(
    const std::array<std::vector<CalibrationResult>, 5>& history) const {
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
    for (const auto& type_history : history) {
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
