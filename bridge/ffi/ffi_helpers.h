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
    mg::core::telemetry::JsonBuilder jb;
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
    // 分罗盘进度：元素为 {compass_id, completion_pct}，界面按识别到的罗盘数量
    // 各画一条进度条。
    {
        mg::core::telemetry::JsonBuilder arr;
        arr.begin_array();
        for (const auto& [compass_id, pct] : s.compass_progress) {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("compass_id", (int)compass_id);
            item.field("completion_pct", (int)pct);
            item.end_object();
            arr.append_json(item.str());
        }
        arr.end_array();
        jb.raw_field("compass_progress", arr.str());
    }
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
} // namespace ffi_helpers
} // namespace bridge
} // namespace mg
