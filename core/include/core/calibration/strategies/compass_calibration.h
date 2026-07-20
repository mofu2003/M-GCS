/**
 * compass_calibration.h - Magnetometer calibration strategy
 * 磁力计校准 — 需旋转机体覆盖各轴，支持 MAG_CAL_REPORT
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param2=1
 * Requires rotating the vehicle around all axes.
 * Uses MAG_CAL_REPORT for precise offsets + fitness when available.
 * Falls back to STATUSTEXT parsing for older firmware.
 */

#ifndef MGCORE_COMPASS_CALIBRATION_H
#define MGCORE_COMPASS_CALIBRATION_H

#include "core/calibration/calibration_strategy.h"

namespace mg::core::calibration {

class CompassCalibration : public ICalibrationStrategy {
public:
    CalibrationType type() const override { return CalibrationType::Magnetometer; }
    const char* type_name() const override { return "magnetometer"; }
    uint8_t total_steps() const override { return 1; }  // Continuous until done
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
#endif // MGCORE_COMPASS_CALIBRATION_H
