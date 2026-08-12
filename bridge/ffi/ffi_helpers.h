/**
 * ffi_helpers.h - Shared helpers for FFI split files
 *
 * 校准状态和结果的 JSON 序列化辅助函数。
 * 
 * Inline JSON serializers used by ffi_calibration.cc.
 * Kept in a separate header to avoid duplicating the
 * cal serializer logic if calibration serializers are needed elsewhere.
 */
#pragma once

#include "ffi_internal.h"
#include "core/calibration/calibration_validator.h"

namespace mg {
namespace bridge {
namespace ffi_helpers {

inline std::string cal_status_json(const mg::core::calibration::CalibrationStatus& s) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("active",         s.active);
    jb.field("type",           mg::core::calibration::CalibrationValidator::type_string(s.current_type));
    jb.field("stage",          mg::core::calibration::CalibrationValidator::stage_string(s.stage));
    jb.field("progress",       (int)s.progress);
    jb.field("current_step",   (int)s.current_step);
    jb.field("total_steps",    (int)s.total_steps);
    jb.field("message",        s.user_message);
    jb.field("failure_reason", s.failure_reason);
    jb.field("retry_count",    s.retry_count);
    jb.field("max_retries",    s.max_retries);
    jb.field("timestamp_ms",   (int64_t)s.start_time_ms);
    if (s.current_type == mg::core::calibration::CalibrationType::Accelerometer) {
        jb.field("accelerometer_mode", s.accel_mode == mg::core::calibration::AccelCalMode::Trim
            ? "level"
            : s.accel_mode == mg::core::calibration::AccelCalMode::Simple
            ? "simple"
            : "six_face");
    }
    jb.end_object();
    return jb.str();
}

inline std::string cal_result_json(const mg::core::calibration::CalibrationResult& r) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("type",     mg::core::calibration::CalibrationValidator::type_string(r.type));
    jb.field("stage",    mg::core::calibration::CalibrationValidator::stage_string(r.stage));
    jb.field("quality",  mg::core::calibration::CalibrationValidator::quality_string(r.quality));
    jb.field("timestamp_ms", (int64_t)r.timestamp_ms);

    if (r.type == mg::core::calibration::CalibrationType::Accelerometer) {
        const char* accel_mode = r.accel_mode == mg::core::calibration::AccelCalMode::Trim
            ? "level"
            : r.accel_mode == mg::core::calibration::AccelCalMode::Simple
            ? "simple"
            : "six_face";
        jb.field("accelerometer_mode", accel_mode);
    }

    { JsonBuilder sub; sub.begin_object(); sub.field("x", r.offset_x); sub.field("y", r.offset_y); sub.field("z", r.offset_z); sub.end_object(); jb.raw_field("offsets", sub.str()); }

    if (r.type == mg::core::calibration::CalibrationType::Accelerometer) {
        JsonBuilder sub; sub.begin_object(); sub.field("x", r.scale_x); sub.field("y", r.scale_y); sub.field("z", r.scale_z); sub.end_object(); jb.raw_field("scale", sub.str());
    }

    if (r.type == mg::core::calibration::CalibrationType::Magnetometer) {
        jb.field("fitness", r.fitness);
        { JsonBuilder sub; sub.begin_object(); sub.field("x", r.diagonal_x); sub.field("y", r.diagonal_y); sub.field("z", r.diagonal_z); sub.end_object(); jb.raw_field("diagonals", sub.str()); }
        { JsonBuilder sub; sub.begin_object(); sub.field("x", r.offdiag_x); sub.field("y", r.offdiag_y); sub.field("z", r.offdiag_z); sub.end_object(); jb.raw_field("offdiagonals", sub.str()); }
    }

    { JsonBuilder sub; sub.begin_object(); sub.field("x", r.prev_offset_x); sub.field("y", r.prev_offset_y); sub.field("z", r.prev_offset_z); sub.end_object(); jb.raw_field("previous_offsets", sub.str()); }

    jb.field("progress", (int)r.progress);
    jb.field("current_step", (int)r.current_step);
    jb.field("total_steps", (int)r.total_steps);
    jb.field("failure_reason", r.failure_reason);
    jb.end_object();
    return jb.str();
}

inline std::string cal_history_json(const std::vector<mg::core::calibration::CalibrationResult>& results) {
    JsonBuilder jb;
    jb.begin_array();
    for (const auto& r : results) { jb.append_json(cal_result_json(r)); }
    jb.end_array();
    return jb.str();
}

inline std::string cal_retry_json(const mg::core::calibration::RetryInfo& retry) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("retry_count", retry.retry_count);
    jb.field("max_retries", retry.max_retries);
    jb.field("backoff_ms", retry.backoff_ms);
    jb.field("last_failure", retry.last_failure);
    jb.end_object();
    return jb.str();
}

} // namespace ffi_helpers
} // namespace bridge
} // namespace mg
