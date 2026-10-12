#ifndef MGCORE_FFI_API_TELEMETRY_H
#define MGCORE_FFI_API_TELEMETRY_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_vehicle_get_summary(mg_core_t* core);
/// Returns connection and top-bar telemetry for one session without selecting it.
/// The response data object includes `heartbeat_alive` and `heartbeat_fresh`; armed,
/// mode and motion state must be treated as stale when the latter is false.
MGCORE_API char* mg_vehicle_get_session_status(mg_core_t* core, int vehicle_id);
MGCORE_API char* mg_vehicle_get_global_position(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_attitude(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_home_position(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_vfr_hud(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_extended_sys_state(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_firmware_version(mg_core_t* core);
MGCORE_API char* mg_telemetry_get_freshness(mg_core_t* core);
MGCORE_API char* mg_telemetry_get_dashboard_snapshot(mg_core_t* core);

/// 传感器波形历史：把采样点写入 out_points，返回实际写入的点数。
/// 每个点占 10 个 float：
/// [相对毫秒, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, mag_x, mag_y, mag_z]，
/// 单位依次是 m/s²、°/s、高斯（G）；
/// out_max_points 是调用方缓冲区能容纳的点数上限，target_points 是期望点数
/// （<= 0 表示按缓冲区上限），超过时后端按 min/max 分桶抽稀；
/// window_ms 只取最新的这么多毫秒（<= 0 表示整个缓冲）；
/// instance 是 IMU 实例号（0 表示 IMU1），取值来自 mg_waveform_get_instances。
MGCORE_API int mg_waveform_poll(mg_core_t* core, float* out_points, int out_max_points,
                                int target_points, int window_ms, int instance);

/// 已识别到的传感器实例清单。返回 JSON：
/// {"imu":[{"index":0,"active":true,"age_ms":42,"name":"BMI088"}],
///  "mag":[{"index":0,"active":true,"age_ms":42,"name":"IST8310"}]}
/// index 是实例号（0 表示 IMU1 / 磁力计 1），直接作为 mg_waveform_poll 的
/// instance 参数；active 表示最近若干秒内确实收到过该实例的数据；
/// name 是解码出的传感器型号，参数未下载时可能是空串。
/// index 0 始终出现在列表里，其余实例只在收到数据时出现。
MGCORE_API char* mg_waveform_get_instances(mg_core_t* core);

MGCORE_API char* mg_sensors_get_health(mg_core_t* core);
// 飞控消息历史（STATUSTEXT）。since_seq 传上次拿到的最大 seq，只返回更新的那些，
// 便于前端增量追加；返回 total 表示已产生的消息总数。
// 每条含 seq / timestamp_ms / severity / severity_name / text。
MGCORE_API char* mg_telemetry_get_messages(mg_core_t* core, unsigned long long since_seq);


MGCORE_API char* mg_rc_get_channels(mg_core_t* core);
MGCORE_API char* mg_servo_get_outputs(mg_core_t* core);


#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_TELEMETRY_H */
