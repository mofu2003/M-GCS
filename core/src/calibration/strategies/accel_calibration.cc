/**
 * accel_calibration.cc - Accelerometer calibration implementation
 * 加速度计校准实现 — 6面姿态识别与 STATUSTEXT 解析
 *
 * 6-face calibration: 6 steps, each requires specific vehicle orientation.
 * ArduPilot STATUSTEXT messages identify which orientation is needed.
 * Steps: level -> right -> left -> nose down -> nose up -> back
 */

#include "core/calibration/strategies/accel_calibration.h"
#include "core/log/logging.h"
#include <algorithm>

namespace mg::core::calibration {

uint16_t AccelCalibration::mav_command() const {
    return 241;  // MAV_CMD_PREFLIGHT_CALIBRATION
}

bool AccelCalibration::send_calibration_command(
    mg::core::mavlink::MavlinkAdapter* mav,
    uint8_t target_sys, uint8_t target_comp,
    AccelCalMode accel_mode) {
    float mode_value = static_cast<float>(accel_mode);
    MGC_LOG_INFO("[AccelCal] Sending accel calibration command (mode=%d) to sys=%u comp=%u",
                 static_cast<int>(accel_mode), target_sys, target_comp);
    // MAV_CMD_PREFLIGHT_CALIBRATION: param5 for accelerometer
    return mav->send_command_long(target_sys, target_comp,
                                  241,  // MAV_CMD_PREFLIGHT_CALIBRATION
                                  0,
                                  0.0f,            // param1: unused
                                  0.0f,            // param2: unused
                                  0.0f,            // param3: unused
                                  0.0f,            // param4: unused
                                  mode_value,      // param5: accel cal mode
                                  0.0f,            // param6: unused
                                  0.0f);           // param7: unused
}

StatusParseResult AccelCalibration::parse_statustext(
    const std::string& text, uint8_t severity,
    CalibrationStage current_stage) const {
    StatusParseResult result;

    // ArduPilot accelerometer calibration STATUSTEXT messages
    if (text.find("Place") != std::string::npos ||
        text.find("place") != std::string::npos ||
        text.find("Accelerometer") != std::string::npos ||
        text.find("accelerometer") != std::string::npos ||
        text.find("Accel") != std::string::npos ||
        text.find("accel") != std::string::npos) {
        result.is_calibration_text = true;
    }

    if (!result.is_calibration_text) return result;

    // Detect success/failure
    if (text.find("calibration successful") != std::string::npos ||
        text.find("Calibration successful") != std::string::npos ||
        text.find("success") != std::string::npos ||
        text.find("Success") != std::string::npos) {
        result.new_stage = CalibrationStage::Success;
        result.progress = 100;
        result.current_step = 6;
        result.total_steps = 6;
        result.stage_changed = true;
        result.user_message = "Accelerometer calibration successful";
        return result;
    }

    if (text.find("failed") != std::string::npos ||
        text.find("Failed") != std::string::npos ||
        text.find("FAILED") != std::string::npos) {
        result.new_stage = CalibrationStage::Failed;
        result.stage_changed = true;
        result.user_message = "Accelerometer calibration failed";
        return result;
    }

    // Detect step from orientation prompt
    uint8_t step = detect_step_from_text(text);
    if (step > 0) {
        result.new_stage = CalibrationStage::WaitingOrientation;
        result.current_step = step;
        result.total_steps = 6;
        result.progress = static_cast<uint8_t>((step - 1) * 100 / 6);
        result.stage_changed = (current_stage != CalibrationStage::WaitingOrientation);
        result.user_message = text;  // Pass through orientation instruction
        return result;
    }

    // Generic "calibrating" message
    if (text.find("calibrating") != std::string::npos ||
        text.find("Calibrating") != std::string::npos) {
        result.new_stage = CalibrationStage::Calibrating;
        result.stage_changed = (current_stage != CalibrationStage::Calibrating);
        result.total_steps = 6;
    }

    return result;
}

CalibrationStage AccelCalibration::interpret_ack(
    uint8_t result, uint8_t progress,
    CalibrationStage current_stage) const {
    switch (result) {
        case 0:  // MAV_RESULT_ACCEPTED
            return CalibrationStage::Success;
        case 5:  // MAV_RESULT_IN_PROGRESS
            if (current_stage != CalibrationStage::Calibrating &&
                current_stage != CalibrationStage::WaitingOrientation)
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

bool AccelCalibration::verify_calibration(
    const mg::core::mavlink::MavlinkAdapter* mav,
    CalibrationResult& result,
    const CalibrationValidator& validator) const {
    if (!validator.read_calibration_params(CalibrationType::Accelerometer, mav, result)) {
        MGC_LOG_WARN("[AccelCal] Failed to read accel calibration parameters");
        result.failure_reason = "Failed to read calibration parameters from flight controller";
        return false;
    }

    result.quality = validator.assess(result);
    MGC_LOG_INFO("[AccelCal] Verification: offsets=(%.4f, %.4f, %.4f) scale=(%.4f, %.4f, %.4f) quality=%s",
                 result.offset_x, result.offset_y, result.offset_z,
                 result.scale_x, result.scale_y, result.scale_z,
                 CalibrationValidator::quality_string(result.quality));

    return result.quality != CalibrationQuality::Poor;
}

uint8_t AccelCalibration::detect_step_from_text(const std::string& text) const {
    // ArduPilot 6-face accelerometer calibration prompts
    // Step 1: Place vehicle LEVEL
    if (text.find("level") != std::string::npos ||
        text.find("Level") != std::string::npos) return 1;

    // Step 2: Place vehicle on its LEFT side
    if (text.find("left") != std::string::npos ||
        text.find("Left") != std::string::npos) return 2;

    // Step 3: Place vehicle on its RIGHT side
    if (text.find("right") != std::string::npos ||
        text.find("Right") != std::string::npos) return 3;

    // Step 4: Place vehicle nose DOWN
    if (text.find("nose down") != std::string::npos ||
        text.find("Nose down") != std::string::npos) return 4;

    // Step 5: Place vehicle nose UP
    if (text.find("nose up") != std::string::npos ||
        text.find("Nose up") != std::string::npos) return 5;

    // Step 6: Place vehicle on its BACK
    if (text.find("back") != std::string::npos ||
        text.find("Back") != std::string::npos) return 6;

    return 0;  // Not a step prompt
}

} // namespace mg::core::calibration
