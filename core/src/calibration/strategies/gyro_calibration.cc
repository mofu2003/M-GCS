/**
 * gyro_calibration.cc - Gyroscope calibration implementation
 * 陀螺仪校准实现 — 发送指令、等待 ACK、验证偏移量
 *
 * Single-step calibration: send command, wait for ACK, verify offsets.
 * ArduPilot sends STATUSTEXT "Gyro calibration successful" or "Gyro calibration failed".
 */

#include "core/calibration/strategies/gyro_calibration.h"
#include "core/log/logging.h"
#include <algorithm>

namespace mg::core::calibration {

namespace {

/// 完成短语必须整句匹配，不能用 "success"/"complete" 子串：
/// "Gyro calibration unsuccessful" 也含 "success"，子串匹配会把飞控报的失败
/// 读成成功（同目录 compass_calibration.cc 按整句 "Calibration successful" 判定）。
bool mentions_completion(const std::string& text) {
    return text.find("calibration successful") != std::string::npos ||
           text.find("Calibration successful") != std::string::npos ||
           text.find("calibration complete") != std::string::npos ||
           text.find("Calibration complete") != std::string::npos;
}

/// 失败要在完成之前判定："unsuccessful" 本身含 "successful"。
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

uint16_t GyroCalibration::mav_command() const {
    return 241;  // MAV_CMD_PREFLIGHT_CALIBRATION
}

bool GyroCalibration::send_calibration_command(
    mg::core::mavlink::MavlinkAdapter* mav,
    uint8_t target_sys, uint8_t target_comp,
    AccelCalMode /*accel_mode*/) {
    MGC_LOG_INFO("[GyroCal] Sending gyro calibration command to sys=%u comp=%u",
                 target_sys, target_comp);
    // MAV_CMD_PREFLIGHT_CALIBRATION: param1=1 for gyro calibration
    return mav->send_command_long(target_sys, target_comp,
                                  241,  // MAV_CMD_PREFLIGHT_CALIBRATION
                                  0,
                                  kCalGyroStart,  // param1: gyro cal
                                  0.0f,            // param2: unused
                                  0.0f,            // param3: unused
                                  0.0f,            // param4: unused
                                  0.0f,            // param5: unused
                                  0.0f,            // param6: unused
                                  0.0f);           // param7: unused
}

StatusParseResult GyroCalibration::parse_statustext(
    const std::string& text, uint8_t severity,
    CalibrationStage current_stage) const {
    StatusParseResult result;

    // ArduPilot gyro calibration STATUSTEXT messages
    if (text.find("Gyro") != std::string::npos ||
        text.find("gyro") != std::string::npos) {
        result.is_calibration_text = true;

        if (mentions_failure(text)) {
            result.new_stage = CalibrationStage::Failed;
            result.stage_changed = true;
            result.user_message = "Gyroscope calibration failed";
        } else if (mentions_completion(text)) {
            result.new_stage = CalibrationStage::Success;
            result.progress = 100;
            result.current_step = 1;
            result.total_steps = 1;
            result.stage_changed = true;
            result.user_message = "Gyroscope calibration successful";
        } else if (text.find("calibrating") != std::string::npos ||
                   text.find("Calibrating") != std::string::npos ||
                   text.find("place") != std::string::npos ||
                   text.find("Place") != std::string::npos) {
            if (current_stage != CalibrationStage::Calibrating) {
                result.new_stage = CalibrationStage::Calibrating;
                result.stage_changed = true;
            }
            result.current_step = 1;
            result.total_steps = 1;
            result.progress = 50;
            result.user_message = "Gyroscope calibrating - keep vehicle still";
        }
    }

    // Generic calibration messages (ArduPilot sometimes uses these)
    if (!result.is_calibration_text) {
        if (text.find("Initialising") != std::string::npos ||
            text.find("initialising") != std::string::npos) {
            result.is_calibration_text = true;
            result.new_stage = CalibrationStage::Calibrating;
            result.stage_changed = (current_stage != CalibrationStage::Calibrating);
            result.progress = 25;
        }
    }

    return result;
}

CalibrationStage GyroCalibration::interpret_ack(
    uint8_t result, uint8_t progress,
    CalibrationStage current_stage) const {
    // MAV_RESULT values:
    // 0 = ACCEPTED, 1 = TEMPORARILY_REJECTED, 2 = DENIED
    // 3 = UNSUPPORTED, 4 = FAILED, 5 = IN_PROGRESS
    switch (result) {
        case 0:  // MAV_RESULT_ACCEPTED
            // ACK 只确认飞控受理了启动命令，不是校准结果（与同目录
            // radio_calibration.cc:84-88、compass_calibration.cc:187-190 同一语义）。
            // 结果来自 STATUSTEXT；ArduPilot 的陀螺校准是同步完成的，服务层对
            // "受理即完成"的固件另有补偿，见 calibration_service.cc
            // 的 process_command_ack。
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

bool GyroCalibration::verify_calibration(
    const mg::core::mavlink::MavlinkAdapter* mav,
    CalibrationResult& result,
    const CalibrationValidator& validator) const {
    // Read gyro offsets from FC parameters
    if (!validator.read_calibration_params(CalibrationType::Gyroscope, mav, result)) {
        MGC_LOG_WARN("[GyroCal] Failed to read gyro calibration parameters");
        result.failure_reason = "Failed to read calibration parameters from flight controller";
        return false;
    }

    // Assess quality
    result.quality = validator.assess(result);
    MGC_LOG_INFO("[GyroCal] Verification: offsets=(%.4f, %.4f, %.4f) quality=%s",
                 result.offset_x, result.offset_y, result.offset_z,
                 CalibrationValidator::quality_string(result.quality));

    return result.quality != CalibrationQuality::Poor;
}

} // namespace mg::core::calibration
