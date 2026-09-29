/**
 * radio_calibration.h - RC transmitter calibration strategy
 * 遥控器校准 — 采集各通道行程并写入 RCn_MIN/TRIM/MAX
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param4=1
 * 指令下发后由飞控自己采样摇杆行程，用户需要在提示期间打满各通道。
 * 完成后飞控回写 RCn_MIN/RCn_TRIM/RCn_MAX，这里只做读回校验。
 */

#ifndef MGCORE_RADIO_CALIBRATION_H
#define MGCORE_RADIO_CALIBRATION_H

#include "core/calibration/calibration_strategy.h"

namespace mg::core::calibration {

class RadioCalibration : public ICalibrationStrategy {
public:
    CalibrationType type() const override { return CalibrationType::Radio; }
    const char* type_name() const override { return "radio"; }
    uint8_t total_steps() const override { return 1; }
    uint16_t mav_command() const override;

    bool send_calibration_command(
        mg::core::mavlink::MavlinkAdapter* mav,
        uint8_t target_sys, uint8_t target_comp,
        AccelCalMode accel_mode) override;

    StatusParseResult parse_statustext(
        const std::string& text, uint8_t severity,
        CalibrationStage current_stage) const override;

    CalibrationStage interpret_ack(
        uint8_t result, uint8_t progress,
        CalibrationStage current_stage) const override;

    bool verify_calibration(
        const mg::core::mavlink::MavlinkAdapter* mav,
        CalibrationResult& result,
        const CalibrationValidator& validator) const override;
};

} // namespace mg::core::calibration
#endif // MGCORE_RADIO_CALIBRATION_H
