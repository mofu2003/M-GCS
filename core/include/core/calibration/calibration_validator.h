/**
 * calibration_validator.h - Parameter verification and quality assessment
 * 校准质量验证 — 读取飞控参数，评估偏移量是否在阈值内
 *
 * After calibration completes, this module:
 *   1. Reads calibration parameters from the FC (via MavlinkAdapter)
 *   2. Assesses quality against configurable thresholds
 *   3. Determines if recalibration is needed
 *   4. Provides user-friendly failure descriptions
 *
 * Quality thresholds are based on ArduPilot calibration standards
 * and IMU manufacturer specifications. Configurable per-instance
 * for different airframe types (e.g., large vs small vehicles).
 */

#ifndef MGCORE_CALIBRATION_VALIDATOR_H
#define MGCORE_CALIBRATION_VALIDATOR_H

#include "calibration_types.h"
#include "core/mavlink/mavlink_adapter.h"

namespace mg::core::calibration {

class CalibrationValidator {
public:
    CalibrationValidator();
    explicit CalibrationValidator(const QualityThresholds& thresholds);
    ~CalibrationValidator();

    /// Assess calibration result quality based on offset magnitudes.
    CalibrationQuality assess(const CalibrationResult& result) const;

    /// Read calibration parameters from FC after calibration.
    /// Uses MavlinkAdapter to read INS_*, COMPASS_*, GND_* parameters.
    /// Returns a CalibrationResult with current parameter values.
    bool read_calibration_params(CalibrationType type,
                                 const mg::core::mavlink::MavlinkAdapter* mav,
                                 CalibrationResult& result) const;

    /// Determine if recalibration should be attempted.
    bool should_retry(const CalibrationResult& result, const RetryInfo& retry) const;

    /// Get user-friendly failure description.
    std::string describe_failure(const CalibrationResult& result) const;

    /// Get quality level as string.
    static const char* quality_string(CalibrationQuality q);

    /// Get calibration type as string.
    static const char* type_string(CalibrationType t);

    /// Get calibration stage as string.
    static const char* stage_string(CalibrationStage s);

    /// Get threshold configuration (mutable for tuning).
    QualityThresholds& thresholds() { return thresholds_; }
    const QualityThresholds& thresholds() const { return thresholds_; }

private:
    QualityThresholds thresholds_;

    // Magnitude = sqrt(x² + y² + z²)
    static float vec3_magnitude(float x, float y, float z);

    // Quality assessment helpers
    CalibrationQuality assess_gyro(float ox, float oy, float oz) const;
    CalibrationQuality assess_accel(float ox, float oy, float oz) const;
    CalibrationQuality assess_compass(float ox, float oy, float oz, float fitness) const;
    CalibrationQuality assess_baro(float pressure_deviation) const;

    /// 遥控器行程质量：中位值应落在行程中间，且行程不能过窄。
    CalibrationQuality assess_radio(float rc_min, float rc_trim, float rc_max) const;

    // Find parameter value by name in all_params
    bool find_param(const std::vector<mg::core::mavlink::ParamInfo>& params,
                    const char* name, float& value) const;
};

} // namespace mg::core::calibration

#endif // MGCORE_CALIBRATION_VALIDATOR_H
