/**
 * baro_calibration.cc - Barometer calibration implementation
 * 气压计校准实现 — 发送指令并验证地面气压值
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param3=1
 * Sets ground pressure baseline. Near-instant, single-step.
 * Verifies GND_ABS_PRESS is in reasonable atmospheric range.
 */

#include "core/calibration/strategies/baro_calibration.h"
#include "core/log/logging.h"
#include <cmath>

namespace mg::core::calibration {

namespace {

/// 完成短语必须整句匹配："Barometer calibration incomplete" 也含 "complete"，
/// 用子串匹配会把失败读成成功（同目录 compass_calibration.cc 按整句判定）。
/// ArduPilot AP_Baro::calibrate() 完成时发 "Barometer %u calibration complete"，
/// 快速校准 update_calibration() 发 "Barometer calibration complete"；
/// PX4 baro_calibration.cpp 发 "[cal] calibration done: baro"。
bool mentions_completion(const std::string& text) {
    return text.find("calibration complete") != std::string::npos ||
           text.find("Calibration complete") != std::string::npos ||
           text.find("calibration successful") != std::string::npos ||
           text.find("Calibration successful") != std::string::npos ||
           text.find("calibration done") != std::string::npos ||
           text.find("Calibration done") != std::string::npos;
}

/// 失败要在完成之前判定："unsuccessful" 含 "successful"，"incomplete" 含 "complete"。
bool mentions_failure(const std::string& text) {
    return text.find("failed") != std::string::npos ||
           text.find("Failed") != std::string::npos ||
           text.find("FAILED") != std::string::npos ||
           text.find("unsuccessful") != std::string::npos ||
           text.find("Unsuccessful") != std::string::npos ||
           text.find("incomplete") != std::string::npos ||
           text.find("Incomplete") != std::string::npos;
}

} // namespace

uint16_t BaroCalibration::mav_command() const {
    return 241;  // MAV_CMD_PREFLIGHT_CALIBRATION
}

bool BaroCalibration::send_calibration_command(
    mg::core::mavlink::MavlinkAdapter* mav,
    uint8_t target_sys, uint8_t target_comp,
    AccelCalMode /*accel_mode*/) {
    MGC_LOG_INFO("[BaroCal] Sending barometer calibration command to sys=%u comp=%u",
                 target_sys, target_comp);
    // MAV_CMD_PREFLIGHT_CALIBRATION: param3=1 for barometer
    return mav->send_command_long(target_sys, target_comp,
                                  241,  // MAV_CMD_PREFLIGHT_CALIBRATION
                                  0,
                                  0.0f,            // param1: unused
                                  0.0f,            // param2: unused
                                  kCalBaroStart,   // param3: baro cal
                                  0.0f,            // param4: unused
                                  0.0f,            // param5: unused
                                  0.0f,            // param6: unused
                                  0.0f);           // param7: unused
}

StatusParseResult BaroCalibration::parse_statustext(
    const std::string& text, uint8_t severity,
    CalibrationStage current_stage) const {
    StatusParseResult result;

    if (text.find("baro") != std::string::npos ||
        text.find("Baro") != std::string::npos ||
        text.find("Barometer") != std::string::npos ||
        text.find("barometer") != std::string::npos ||
        text.find("pressure") != std::string::npos ||
        text.find("Pressure") != std::string::npos) {
        result.is_calibration_text = true;
    }

    if (!result.is_calibration_text) return result;

    if (mentions_failure(text)) {
        result.new_stage = CalibrationStage::Failed;
        result.stage_changed = true;
        result.user_message = "Barometer calibration failed";
    } else if (mentions_completion(text)) {
        result.new_stage = CalibrationStage::Success;
        result.progress = 100;
        result.current_step = 1;
        result.total_steps = 1;
        result.stage_changed = true;
        result.user_message = "Barometer calibration successful";
    } else {
        result.new_stage = CalibrationStage::Calibrating;
        result.stage_changed = (current_stage != CalibrationStage::Calibrating);
        result.progress = 50;
    }

    return result;
}

CalibrationStage BaroCalibration::interpret_ack(
    uint8_t result, uint8_t progress,
    CalibrationStage current_stage) const {
    switch (result) {
        case 0:  // MAV_RESULT_ACCEPTED
            // ACK 只确认飞控受理了启动命令（与 radio_calibration.cc:84-88、
            // compass_calibration.cc:187-190 同一语义）。ArduPilot 在回 ACCEPTED
            // 之前就发了 "Barometer calibration complete"，PX4 完成后发
            // "[cal] calibration done: baro"，结果由 STATUSTEXT 给出。
            return CalibrationStage::Calibrating;
        case 5:  // MAV_RESULT_IN_PROGRESS
            return CalibrationStage::Calibrating;
        case 1: case 2: case 4:
            return CalibrationStage::Failed;
        default:
            return CalibrationStage::Failed;
    }
}

bool BaroCalibration::verify_calibration(
    const mg::core::mavlink::MavlinkAdapter* mav,
    CalibrationResult& result,
    const CalibrationValidator& validator) const {
    if (!validator.read_calibration_params(CalibrationType::Barometer, mav, result)) {
        MGC_LOG_WARN("[BaroCal] Failed to read barometer calibration parameters");
        result.failure_reason = "Failed to read calibration parameters from flight controller";
        return false;
    }

    result.quality = validator.assess(result);
    MGC_LOG_INFO("[BaroCal] Verification: ground_pressure=%.2f Pa ground_temp=%.1f C quality=%s",
                 result.offset_x, result.offset_y,
                 CalibrationValidator::quality_string(result.quality));

    return result.quality != CalibrationQuality::Poor;
}

} // namespace mg::core::calibration
