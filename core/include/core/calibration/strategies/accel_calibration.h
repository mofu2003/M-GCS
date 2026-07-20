/**
 * accel_calibration.h - Accelerometer calibration strategy
 * 加速度计校准 — 6面校准（水平、右倾、左倾、前倾、后倾、倒置）
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param5=1 (6-face) or param5=4 (simple)
 * 6-face calibration: 6 steps, each requires specific vehicle orientation.
 * Steps: level, right side, left side, nose down, nose up, on back.
 */

#ifndef MGCORE_ACCEL_CALIBRATION_H
#define MGCORE_ACCEL_CALIBRATION_H

#include "core/calibration/calibration_strategy.h"

namespace mg::core::calibration {

class AccelCalibration : public ICalibrationStrategy {
public:
    CalibrationType type() const override { return CalibrationType::Accelerometer; }
    const char* type_name() const override { return "accelerometer"; }
    uint8_t total_steps() const override { return 6; }  // 6-face calibration
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

private:
    // Map STATUSTEXT orientation prompts to step numbers
    uint8_t detect_step_from_text(const std::string& text) const;
};

} // namespace mg::core::calibration
#endif // MGCORE_ACCEL_CALIBRATION_H
