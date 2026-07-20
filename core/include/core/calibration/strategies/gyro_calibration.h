/**
 * gyro_calibration.h - Gyroscope calibration strategy
 * 陀螺仪校准 — 机体静止放置，单步完成，约3-5秒
 *
 * ArduPilot: MAV_CMD_PREFLIGHT_CALIBRATION param1=1
 * Single-step calibration. Vehicle must be stationary on level surface.
 * Duration: typically 3-5 seconds.
 */

#ifndef MGCORE_GYRO_CALIBRATION_H
#define MGCORE_GYRO_CALIBRATION_H

#include "core/calibration/calibration_strategy.h"

namespace mg::core::calibration {

class GyroCalibration : public ICalibrationStrategy {
public:
    CalibrationType type() const override { return CalibrationType::Gyroscope; }
    const char* type_name() const override { return "gyroscope"; }
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
#endif // MGCORE_GYRO_CALIBRATION_H
