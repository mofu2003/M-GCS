/**
 * baro_calibration.h - Barometer calibration strategy
 * 气压计校准 — 设置地面气压基准，单步完成
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param3=1
 * Sets ground pressure baseline. Single-step, near-instant.
 * Vehicle must be at a known, stable location.
 */

#ifndef MGCORE_BARO_CALIBRATION_H
#define MGCORE_BARO_CALIBRATION_H

#include "core/calibration/calibration_strategy.h"

namespace mg::core::calibration {

class BaroCalibration : public ICalibrationStrategy {
public:
    CalibrationType type() const override { return CalibrationType::Barometer; }
    const char* type_name() const override { return "barometer"; }
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
#endif // MGCORE_BARO_CALIBRATION_H
