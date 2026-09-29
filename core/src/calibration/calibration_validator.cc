/**
 * calibration_validator.cc - Parameter verification and quality assessment
 * 校准质量验证实现 — 偏移量阈值检测与质量等级判定
 *
 * Industrial-grade validation: every calibration result is verified
 * against configurable thresholds derived from ArduPilot standards
 * and IMU manufacturer specifications.
 */

#include "core/calibration/calibration_validator.h"
#include "core/log/logging.h"
#include <cmath>
#include <algorithm>

namespace mg::core::calibration {

CalibrationValidator::CalibrationValidator() = default;

CalibrationValidator::CalibrationValidator(const QualityThresholds& thresholds)
    : thresholds_(thresholds) {}

CalibrationValidator::~CalibrationValidator() = default;

// ================================================================
// Public interface
// ================================================================

CalibrationQuality CalibrationValidator::assess(const CalibrationResult& result) const {
    switch (result.type) {
        case CalibrationType::Gyroscope:
            return assess_gyro(result.offset_x, result.offset_y, result.offset_z);
        case CalibrationType::Accelerometer:
            return assess_accel(result.offset_x, result.offset_y, result.offset_z);
        case CalibrationType::Magnetometer:
            return assess_compass(result.offset_x, result.offset_y, result.offset_z, result.fitness);
        case CalibrationType::Barometer:
            // For barometer we check absolute pressure baseline reasonableness
            // A typical ground pressure is 950-1050 hPa; deviation from nominal is the metric
            return assess_baro(result.offset_x);  // offset_x stores ground pressure for baro
        case CalibrationType::Radio:
            // offset_x/y/z 存主通道的 min/trim/max，trim 应落在行程中间。
            return assess_radio(result.offset_x, result.offset_y, result.offset_z);
        default:
            return CalibrationQuality::Poor;
    }
}

bool CalibrationValidator::read_calibration_params(CalibrationType type,
                                                    const mg::core::mavlink::MavlinkAdapter* mav,
                                                    CalibrationResult& result) const {
    if (!mav) return false;

    auto params = mav->all_params();
    if (params.empty()) {
        MGC_LOG_WARN("[CalValidator] No parameters available for verification");
        return false;
    }

    switch (type) {
        case CalibrationType::Gyroscope: {
            // Read primary IMU gyro offsets (instance 0)
            float ox = 0.0f, oy = 0.0f, oz = 0.0f;
            bool ok = find_param(params, ardupilot_params::kGyroOffsX[0], ox)
                   && find_param(params, ardupilot_params::kGyroOffsY[0], oy)
                   && find_param(params, ardupilot_params::kGyroOffsZ[0], oz);
            if (ok) {
                result.offset_x = ox;
                result.offset_y = oy;
                result.offset_z = oz;
            }
            return ok;
        }

        case CalibrationType::Accelerometer: {
            float ox = 0.0f, oy = 0.0f, oz = 0.0f;
            float sx = 1.0f, sy = 1.0f, sz = 1.0f;
            bool ok = find_param(params, ardupilot_params::kAccelOffsX[0], ox)
                   && find_param(params, ardupilot_params::kAccelOffsY[0], oy)
                   && find_param(params, ardupilot_params::kAccelOffsZ[0], oz);
            // Scale factors are optional (may not exist on all FC versions)
            find_param(params, ardupilot_params::kAccelScaleX[0], sx);
            find_param(params, ardupilot_params::kAccelScaleY[0], sy);
            find_param(params, ardupilot_params::kAccelScaleZ[0], sz);
            if (ok) {
                result.offset_x = ox;
                result.offset_y = oy;
                result.offset_z = oz;
                result.scale_x = sx;
                result.scale_y = sy;
                result.scale_z = sz;
            }
            return ok;
        }

        case CalibrationType::Magnetometer: {
            float ox = 0.0f, oy = 0.0f, oz = 0.0f;
            float dx = 0.0f, dy = 0.0f, dz = 0.0f;
            float odx = 0.0f, ody = 0.0f, odz = 0.0f;
            bool ok = find_param(params, ardupilot_params::kMagOffsX[0], ox)
                   && find_param(params, ardupilot_params::kMagOffsY[0], oy)
                   && find_param(params, ardupilot_params::kMagOffsZ[0], oz);
            // Diagonals and off-diagonals are optional
            find_param(params, ardupilot_params::kMagDiaX[0], dx);
            find_param(params, ardupilot_params::kMagDiaY[0], dy);
            find_param(params, ardupilot_params::kMagDiaZ[0], dz);
            find_param(params, ardupilot_params::kMagOdiX[0], odx);
            find_param(params, ardupilot_params::kMagOdiY[0], ody);
            find_param(params, ardupilot_params::kMagOdiZ[0], odz);
            if (ok) {
                result.offset_x  = ox;
                result.offset_y  = oy;
                result.offset_z  = oz;
                result.diagonal_x = dx;
                result.diagonal_y = dy;
                result.diagonal_z = dz;
                result.offdiag_x  = odx;
                result.offdiag_y  = ody;
                result.offdiag_z  = odz;
            }
            return ok;
        }

        case CalibrationType::Barometer: {
            float gp = 0.0f, gt = 0.0f;
            bool ok = find_param(params, ardupilot_params::kBaroGroundPress, gp);
            find_param(params, ardupilot_params::kBaroGroundTemp, gt);
            if (ok) {
                // Store ground pressure in offset_x for quality assessment
                result.offset_x = gp;
                result.offset_y = gt;
            }
            return ok;
        }

        case CalibrationType::Radio: {
            // 四个主通道的行程都要有效，否则说明校准没覆盖全部摇杆。
            for (int channel = 0; channel < 4; ++channel) {
                float min_pwm = 0.0f, trim_pwm = 0.0f, max_pwm = 0.0f;
                const bool ok =
                    find_param(params, ardupilot_params::kRcMin[channel], min_pwm) &&
                    find_param(params, ardupilot_params::kRcTrim[channel], trim_pwm) &&
                    find_param(params, ardupilot_params::kRcMax[channel], max_pwm);
                if (!ok || max_pwm <= min_pwm) return false;
                if (channel == 0) {
                    result.offset_x = min_pwm;
                    result.offset_y = trim_pwm;
                    result.offset_z = max_pwm;
                }
            }
            return true;
        }

        default:
            return false;
    }
}

bool CalibrationValidator::should_retry(const CalibrationResult& result,
                                         const RetryInfo& retry) const {
    // Don't retry if cancelled by user
    if (result.stage == CalibrationStage::Cancelled) return false;

    // Don't retry if already at max
    if (retry.retry_count >= retry.max_retries) return false;

    // Retry on failure
    if (result.stage == CalibrationStage::Failed) return true;

    // Retry on poor quality
    if (result.quality == CalibrationQuality::Poor) return true;

    // Marginal quality: allow retry but not forced
    // (caller can let user decide)
    return false;
}

std::string CalibrationValidator::describe_failure(const CalibrationResult& result) const {
    if (result.stage != CalibrationStage::Failed &&
        result.stage != CalibrationStage::Cancelled) {
        return "";
    }

    // If we have a specific failure reason, use it
    if (!result.failure_reason.empty()) {
        return result.failure_reason;
    }

    // Generate description based on quality assessment
    switch (result.type) {
        case CalibrationType::Gyroscope:
            return "Gyroscope calibration failed. Ensure vehicle is stationary "
                   "and on a level surface. Vibration may affect results.";

        case CalibrationType::Accelerometer:
            if (result.current_step > 0 && result.total_steps > 0) {
                return "Accelerometer calibration failed at step " +
                       std::to_string(result.current_step) + " of " +
                       std::to_string(result.total_steps) +
                       ". Ensure each orientation is held steady for 2+ seconds.";
            }
            return "Accelerometer calibration failed. Place vehicle on a level "
                   "surface and ensure it is stationary.";

        case CalibrationType::Magnetometer:
            return "Magnetometer calibration failed. Move away from magnetic "
                   "interference sources (motors, power cables, metal structures). "
                   "Rotate the vehicle slowly around all axes during calibration.";

        case CalibrationType::Barometer:
            return "Barometer calibration failed. Ensure the sensor is not "
                   "blocked and the vehicle is at a stable location.";

        default:
            return "Calibration failed. Check sensor connections and try again.";
    }
}

// ================================================================
// String converters
// ================================================================

const char* CalibrationValidator::quality_string(CalibrationQuality q) {
    switch (q) {
        case CalibrationQuality::Excellent: return "excellent";
        case CalibrationQuality::Good:      return "good";
        case CalibrationQuality::Marginal:  return "marginal";
        case CalibrationQuality::Poor:      return "poor";
        default: return "unknown";
    }
}

const char* CalibrationValidator::type_string(CalibrationType t) {
    switch (t) {
        case CalibrationType::Gyroscope:     return "gyroscope";
        case CalibrationType::Accelerometer: return "accelerometer";
        case CalibrationType::Magnetometer:  return "magnetometer";
        case CalibrationType::Barometer:     return "barometer";
        case CalibrationType::Radio:         return "radio";
        default: return "unknown";
    }
}

const char* CalibrationValidator::stage_string(CalibrationStage s) {
    switch (s) {
        case CalibrationStage::Idle:              return "idle";
        case CalibrationStage::WaitingForStart:   return "waiting_for_start";
        case CalibrationStage::WaitingOrientation: return "waiting_orientation";
        case CalibrationStage::Calibrating:       return "calibrating";
        case CalibrationStage::Verifying:         return "verifying";
        case CalibrationStage::Success:           return "success";
        case CalibrationStage::Failed:            return "failed";
        case CalibrationStage::Cancelled:         return "cancelled";
        default: return "unknown";
    }
}

// ================================================================
// Private helpers
// ================================================================

float CalibrationValidator::vec3_magnitude(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

CalibrationQuality CalibrationValidator::assess_gyro(float ox, float oy, float oz) const {
    float mag = vec3_magnitude(ox, oy, oz);
    if (mag < thresholds_.gyro_excellent) return CalibrationQuality::Excellent;
    if (mag < thresholds_.gyro_good)      return CalibrationQuality::Good;
    if (mag < thresholds_.gyro_marginal)  return CalibrationQuality::Marginal;
    return CalibrationQuality::Poor;
}

CalibrationQuality CalibrationValidator::assess_accel(float ox, float oy, float oz) const {
    // For accelerometer, the expected offset on Z axis is ~9.81 m/s²
    // when level. We check the deviation from nominal.
    // The offsets stored by ArduPilot are the raw sensor offsets,
    // typically in the range of a few m/s².
    float mag = vec3_magnitude(ox, oy, oz);
    if (mag < thresholds_.accel_excellent) return CalibrationQuality::Excellent;
    if (mag < thresholds_.accel_good)      return CalibrationQuality::Good;
    if (mag < thresholds_.accel_marginal)  return CalibrationQuality::Marginal;
    return CalibrationQuality::Poor;
}

CalibrationQuality CalibrationValidator::assess_compass(float ox, float oy, float oz,
                                                         float fitness) const {
    // Primary metric: offset magnitude
    float mag = vec3_magnitude(ox, oy, oz);
    CalibrationQuality offset_quality;
    if (mag < thresholds_.mag_excellent) offset_quality = CalibrationQuality::Excellent;
    else if (mag < thresholds_.mag_good) offset_quality = CalibrationQuality::Good;
    else if (mag < thresholds_.mag_marginal) offset_quality = CalibrationQuality::Marginal;
    else offset_quality = CalibrationQuality::Poor;

    // Secondary metric: fitness score (if available)
    if (fitness > 0.0f) {
        CalibrationQuality fitness_quality;
        if (fitness < thresholds_.mag_fitness_excellent) fitness_quality = CalibrationQuality::Excellent;
        else if (fitness < thresholds_.mag_fitness_good) fitness_quality = CalibrationQuality::Good;
        else if (fitness < thresholds_.mag_fitness_marginal) fitness_quality = CalibrationQuality::Marginal;
        else fitness_quality = CalibrationQuality::Poor;

        // Return the worse of the two assessments
        return static_cast<CalibrationQuality>(
            std::max(static_cast<int>(offset_quality), static_cast<int>(fitness_quality)));
    }

    return offset_quality;
}

CalibrationQuality CalibrationValidator::assess_baro(float ground_pressure) const {
    // Ground pressure should be in a reasonable atmospheric range
    // Standard sea level: 1013.25 hPa
    // Typical range: 950-1050 hPa depending on altitude and weather
    // Deviation from nominal indicates calibration quality
    float deviation = std::abs(ground_pressure - 1013.25f);
    if (deviation < thresholds_.baro_excellent) return CalibrationQuality::Excellent;
    if (deviation < thresholds_.baro_good)      return CalibrationQuality::Good;
    if (deviation < thresholds_.baro_marginal)  return CalibrationQuality::Marginal;
    return CalibrationQuality::Poor;
}

CalibrationQuality CalibrationValidator::assess_radio(float rc_min, float rc_trim, float rc_max) const {
    // PWM 行程的常见范围是 1000-2000us；行程过窄说明摇杆没有打满。
    const float span = rc_max - rc_min;
    if (span < 400.0f) return CalibrationQuality::Poor;
    if (rc_trim < rc_min || rc_trim > rc_max) return CalibrationQuality::Poor;

    // 中位值偏离行程中心的比例越小越好。
    const float centre = (rc_max + rc_min) * 0.5f;
    const float offset = std::abs(rc_trim - centre);
    if (offset < 50.0f && span >= 800.0f) return CalibrationQuality::Excellent;
    if (offset < 100.0f) return CalibrationQuality::Good;
    if (offset < 200.0f) return CalibrationQuality::Marginal;
    return CalibrationQuality::Poor;
}

bool CalibrationValidator::find_param(const std::vector<mg::core::mavlink::ParamInfo>& params,
                                       const char* name, float& value) const {
    if (!name) return false;
    for (const auto& p : params) {
        if (p.name == name) {
            value = p.value;
            return true;
        }
    }
    return false;
}

} // namespace mg::core::calibration
