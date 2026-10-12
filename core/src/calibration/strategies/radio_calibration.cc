/**
 * radio_calibration.cc - RC transmitter calibration implementation
 * 遥控器校准实现 — 下发 RC 校准指令并读回通道行程
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param4=1
 * 飞控负责采样，完成后回写 RCn_MIN/TRIM/MAX。
 */

#include "core/calibration/strategies/radio_calibration.h"
#include "statustext_match.h"
#include "core/log/logging.h"

namespace mg::core::calibration {

uint16_t RadioCalibration::mav_command() const {
    return 241;  // MAV_CMD_PREFLIGHT_CALIBRATION
}

bool RadioCalibration::send_calibration_command(
    mg::core::mavlink::MavlinkAdapter* mav,
    uint8_t target_sys, uint8_t target_comp,
    AccelCalMode /*accel_mode*/) {
    MGC_LOG_INFO("[RadioCal] Sending RC calibration command to sys=%u comp=%u",
                 target_sys, target_comp);
    // MAV_CMD_PREFLIGHT_CALIBRATION: param4=1 for radio calibration
    return mav->send_command_long(target_sys, target_comp,
                                  241,  // MAV_CMD_PREFLIGHT_CALIBRATION
                                  0,
                                  0.0f,             // param1: unused
                                  0.0f,             // param2: unused
                                  0.0f,             // param3: unused
                                  kCalRadioStart,   // param4: radio cal
                                  0.0f,             // param5: unused
                                  0.0f,             // param6: unused
                                  0.0f);            // param7: unused
}

StatusParseResult RadioCalibration::parse_statustext(
    const std::string& text, uint8_t severity,
    CalibrationStage current_stage) const {
    (void)severity;
    StatusParseResult result;

    // 话题词：飞控在遥控校准期间发的 STATUSTEXT 里带这些字样。保留原来这几段
    // 是为了不误命中含 "RC" 的无关词（FORCE、SOURCE 之类）；本轮只统一判定口径。
    const bool mentions_radio =
        text.find("RC ") != std::string::npos ||
        text.find("RC1") != std::string::npos ||
        text.find("Radio") != std::string::npos ||
        text.find("radio") != std::string::npos ||
        text.find("transmitter") != std::string::npos ||
        text.find("Transmitter") != std::string::npos;
    if (!mentions_radio) return result;

    result.is_calibration_text = true;
    // 与 gyro/baro/accel/compass 同一口径：先判失败（"unsuccessful" 含 "successful"），
    // 再用整句判定完成——裸子串会把 "RC calibration unsuccessful" 读成成功。
    if (text::mentions_failure(text)) {
        result.new_stage = CalibrationStage::Failed;
        result.stage_changed = true;
        result.user_message = "遥控器校准失败";
    } else if (text::mentions_completion(text)) {
        result.new_stage = CalibrationStage::Success;
        result.progress = 100;
        result.current_step = 1;
        result.total_steps = 1;
        result.stage_changed = true;
        result.user_message = "遥控器校准完成";
    } else {
        result.new_stage = CalibrationStage::Calibrating;
        result.stage_changed = (current_stage != CalibrationStage::Calibrating);
        result.current_step = 1;
        result.total_steps = 1;
        result.progress = 50;
        result.user_message = "遥控器校准中，请打满各通道行程";
    }
    return result;
}

CalibrationStage RadioCalibration::interpret_ack(
    uint8_t result, uint8_t progress,
    CalibrationStage current_stage) const {
    (void)progress;
    switch (result) {
        case 0:  // MAV_RESULT_ACCEPTED
            // ACK only confirms that the FC accepted the start command. The
            // operator still has to move the sticks before RCn_MIN/TRIM/MAX
            // can be verified.
            return CalibrationStage::Calibrating;
        case 5:  // MAV_RESULT_IN_PROGRESS
            return current_stage == CalibrationStage::Calibrating
                ? current_stage
                : CalibrationStage::Calibrating;
        default:
            return CalibrationStage::Failed;
    }
}

bool RadioCalibration::verify_calibration(
    const mg::core::mavlink::MavlinkAdapter* mav,
    CalibrationResult& result,
    const CalibrationValidator& validator) const {
    // 飞控校准后会回写 RCn_MIN/TRIM/MAX；读回主通道即可判定是否落地。
    if (!validator.read_calibration_params(CalibrationType::Radio, mav, result)) {
        MGC_LOG_WARN("[RadioCal] Failed to read RC calibration parameters");
        result.failure_reason = "未能从飞控读回遥控器行程参数";
        return false;
    }

    result.quality = validator.assess(result);
    MGC_LOG_INFO("[RadioCal] Verification: min=%.0f trim=%.0f max=%.0f quality=%s",
                 result.offset_x, result.offset_y, result.offset_z,
                 CalibrationValidator::quality_string(result.quality));

    return result.quality != CalibrationQuality::Poor;
}

} // namespace mg::core::calibration
