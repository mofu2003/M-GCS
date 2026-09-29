/**
 * compass_calibration.cc - Magnetometer calibration implementation
 * 磁力计校准实现 — 偏移量/对角线/适应度验证
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param2=1
 * Requires rotating vehicle around all axes. ArduPilot reports
 * progress via STATUSTEXT and optionally MAG_CAL_REPORT messages.
 *
 * Industrial-grade: verifies offsets + diagonals + fitness score.
 */

#include "core/calibration/strategies/compass_calibration.h"
#include "core/log/logging.h"
#include <algorithm>

namespace mg::core::calibration {

uint16_t CompassCalibration::mav_command() const {
    return 42424;  // MAV_CMD_DO_START_MAG_CAL
}

bool CompassCalibration::send_calibration_command(
    mg::core::mavlink::MavlinkAdapter* mav,
    uint8_t target_sys, uint8_t target_comp,
    AccelCalMode /*accel_mode*/) {
    MGC_LOG_INFO("[CompassCal] Sending magnetometer calibration command to sys=%u comp=%u",
                 target_sys, target_comp);
    // Match Mission Planner's modern MAV_CAL workflow.  It supports multiple
    // compasses and returns MAG_CAL_PROGRESS/MAG_CAL_REPORT messages.
    // 参数与 Mission Planner 的 ConfigHWCompass 保持一致：
    // param1=0 校准所有检测到的罗盘；param2=1 飞控内部失败自动重试；
    // param3=1 成功后自动保存。之前 param2 写成 0，等于替飞控关掉了重试。
    return mav->send_command_long(target_sys, target_comp,
                                  42424,  // MAV_CMD_DO_START_MAG_CAL
                                  0,
                                  0.0f,            // param1: all compasses
                                  1.0f,            // param2: retry on failure enabled
                                  1.0f,            // param3: autosave
                                  0.0f,            // param4: unused
                                  0.0f,            // param5: unused
                                  0.0f,            // param6: unused
                                  0.0f);           // param7: unused
}

StatusParseResult CompassCalibration::parse_statustext(
    const std::string& text, uint8_t severity,
    CalibrationStage current_stage) const {
    StatusParseResult result;

    // ArduPilot magnetometer calibration STATUSTEXT messages
    if (text.find("compass") != std::string::npos ||
        text.find("Compass") != std::string::npos ||
        text.find("CompassCal") != std::string::npos ||
        text.find("Mag") != std::string::npos ||
        text.find("mag") != std::string::npos ||
        text.find("Magnetometer") != std::string::npos ||
        text.find("magnetometer") != std::string::npos ||
        text.find("MAG") != std::string::npos) {
        result.is_calibration_text = true;
    }

    // Also check for generic calibration messages during active calibration
    if (!result.is_calibration_text &&
        (current_stage == CalibrationStage::Calibrating ||
         current_stage == CalibrationStage::WaitingOrientation)) {
        if (text.find("calibrat") != std::string::npos ||
            text.find("Calibrat") != std::string::npos ||
            text.find("rotate") != std::string::npos ||
            text.find("Rotate") != std::string::npos ||
            text.find("sample") != std::string::npos ||
            text.find("Sample") != std::string::npos) {
            result.is_calibration_text = true;
        }
    }

    if (!result.is_calibration_text) return result;

    // Success detection
    if (text.find("Calibration successful") != std::string::npos ||
        text.find("calibration successful") != std::string::npos ||
        (text.find("Compass") != std::string::npos &&
            text.find("calibrated") != std::string::npos &&
            text.find("fail") == std::string::npos &&
            text.find("Fail") == std::string::npos)) {
        result.new_stage = CalibrationStage::Success;
        result.progress = 100;
        result.stage_changed = true;
        result.user_message = "Magnetometer calibration successful";
        return result;
    }

    // Failure detection
    if (text.find("failed") != std::string::npos ||
        text.find("Failed") != std::string::npos ||
        text.find("FAILED") != std::string::npos ||
        text.find("Failed:") != std::string::npos) {
        result.new_stage = CalibrationStage::Failed;
        result.stage_changed = true;
        result.user_message = text;  // Preserve failure reason
        return result;
    }

    // Progress detection: "compass: x%" or "Mag (0): x%"
    // ArduPilot reports compass calibration progress as percentage
    size_t pct_pos = text.find('%');
    if (pct_pos != std::string::npos && pct_pos > 0) {
        // Try to extract the percentage number
        size_t num_start = pct_pos - 1;
        while (num_start > 0 && (text[num_start] >= '0' && text[num_start] <= '9')) {
            num_start--;
        }
        if (text[num_start] < '0' || text[num_start] > '9') {
            num_start++;
        }
        if (num_start < pct_pos) {
            std::string num_str = text.substr(num_start, pct_pos - num_start);
            int pct = 0;
            try {
                pct = std::stoi(num_str);
            } catch (...) {
                pct = 0;
            }
            if (pct >= 0 && pct <= 100) {
                result.progress = static_cast<uint8_t>(pct);
                result.new_stage = CalibrationStage::Calibrating;
                result.stage_changed = (current_stage != CalibrationStage::Calibrating);
                result.user_message = "Magnetometer calibrating: " + std::to_string(pct) + "%";
                return result;
            }
        }
    }

    // Rotation prompts
    if (text.find("Rotate") != std::string::npos ||
        text.find("rotate") != std::string::npos ||
        text.find("all axes") != std::string::npos ||
        text.find("interference") != std::string::npos) {
        result.new_stage = CalibrationStage::Calibrating;
        result.stage_changed = (current_stage != CalibrationStage::Calibrating);
        result.user_message = text;
    }

    return result;
}

CalibrationStage CompassCalibration::interpret_ack(
    uint8_t result, uint8_t progress,
    CalibrationStage current_stage) const {
    switch (result) {
        case 0:  // MAV_RESULT_ACCEPTED
            // Completion is reported by MAG_CAL_REPORT, not this ACK.
            return CalibrationStage::Calibrating;
        case 5:  // MAV_RESULT_IN_PROGRESS
            if (current_stage != CalibrationStage::Calibrating)
                return CalibrationStage::Calibrating;
            return current_stage;
        case 1:  // TEMPORARILY_REJECTED
        case 2:  // DENIED
        case 4:  // FAILED
            return CalibrationStage::Failed;
        default:
            return CalibrationStage::Failed;
    }
}

bool CompassCalibration::verify_calibration(
    const mg::core::mavlink::MavlinkAdapter* mav,
    CalibrationResult& result,
    const CalibrationValidator& validator) const {
    // MAG_CAL_REPORT is the authoritative result from the modern ArduPilot
    // workflow.  Parameter cache updates can arrive seconds later, so do not
    // reject a valid report merely because the cache has not caught up.
    if (result.mag_report_received) {
        result.quality = validator.assess(result);
        return result.quality != CalibrationQuality::Poor;
    }

    if (!validator.read_calibration_params(CalibrationType::Magnetometer, mav, result)) {
        MGC_LOG_WARN("[CompassCal] Failed to read compass calibration parameters");
        result.failure_reason = "Failed to read calibration parameters from flight controller";
        return false;
    }

    result.quality = validator.assess(result);
    MGC_LOG_INFO("[CompassCal] Verification: offsets=(%.1f, %.1f, %.1f) "
                 "diagonal=(%.1f, %.1f, %.1f) fitness=%.1f quality=%s",
                 result.offset_x, result.offset_y, result.offset_z,
                 result.diagonal_x, result.diagonal_y, result.diagonal_z,
                 result.fitness,
                 CalibrationValidator::quality_string(result.quality));

    return result.quality != CalibrationQuality::Poor;
}

} // namespace mg::core::calibration
