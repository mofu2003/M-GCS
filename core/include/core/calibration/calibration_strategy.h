/**
 * calibration_strategy.h - Strategy base class for sensor calibration
 * 校准策略接口 — 每种传感器实现此接口，处理指令/进度/结果
 *
 * Each sensor type implements this interface with its own:
 *   - MAVLink command to trigger calibration
 *   - STATUSTEXT parsing for progress/orientation feedback
 *   - ACK result interpretation
 *   - Post-calibration verification logic
 *
 * Design: QGC FirmwarePlugin pattern.
 * Adding a new calibration type = implement one strategy class.
 */

#ifndef MGCORE_CALIBRATION_STRATEGY_H
#define MGCORE_CALIBRATION_STRATEGY_H

#include "calibration_types.h"
#include "calibration_validator.h"
#include "core/mavlink/mavlink_adapter.h"

namespace mg::core::calibration {

/// Result of processing a STATUSTEXT message during calibration
struct StatusParseResult {
    CalibrationStage new_stage = CalibrationStage::Idle;  // Stage transition (if any)
    uint8_t progress = 0;          // Updated progress (0-100)
    uint8_t current_step = 0;      // Updated step number
    uint8_t total_steps = 0;       // Updated total steps
    std::string user_message;      // User-facing message
    bool stage_changed = false;    // Whether stage actually changed
    bool is_calibration_text = false;  // Whether this text is calibration-related
};

class ICalibrationStrategy {
public:
    virtual ~ICalibrationStrategy() = default;

    /// Get the calibration type this strategy handles.
    virtual CalibrationType type() const = 0;

    /// Get the type name as string.
    virtual const char* type_name() const = 0;

    /// Get the total number of steps for this calibration type.
    /// For accel 6-face: 6 steps. For gyro: 1 step. Etc.
    virtual uint8_t total_steps() const = 0;

    /// Send the MAVLink command to start calibration.
    /// Returns true if the command was sent successfully.
    virtual bool send_calibration_command(
        mg::core::mavlink::MavlinkAdapter* mav,
        uint8_t target_sys, uint8_t target_comp,
        AccelCalMode accel_mode) = 0;

    /// Parse a STATUSTEXT message received during calibration.
    /// Returns whether this message was calibration-related and any state transition.
    virtual StatusParseResult parse_statustext(
        const std::string& text,
        uint8_t severity,
        CalibrationStage current_stage) const = 0;

    /// Interpret COMMAND_ACK result during calibration.
    /// Returns the new stage based on ACK result.
    virtual CalibrationStage interpret_ack(
        uint8_t result, uint8_t progress,
        CalibrationStage current_stage) const = 0;

    /// Read post-calibration parameters and verify.
    virtual bool verify_calibration(
        const mg::core::mavlink::MavlinkAdapter* mav,
        CalibrationResult& result,
        const CalibrationValidator& validator) const = 0;

    /// Get the MAV_CMD for this calibration.
    virtual uint16_t mav_command() const = 0;
};

} // namespace mg::core::calibration

#endif // MGCORE_CALIBRATION_STRATEGY_H
