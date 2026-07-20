#ifndef MGCORE_FFI_API_TELEMETRY_H
#define MGCORE_FFI_API_TELEMETRY_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_vehicle_get_summary(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_battery(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_global_position(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_attitude(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_gps(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_rc_channels(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_servo_output(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_home_position(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_vfr_hud(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_imu(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_pressure(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_highres_imu(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_extended_sys_state(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_firmware_version(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_vibration(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_esc_status(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_wind_estimate(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_prearm_check(mg_core_t* core);
MGCORE_API char* mg_telemetry_get_freshness(mg_core_t* core);
MGCORE_API char* mg_telemetry_get_dashboard_snapshot(mg_core_t* core);
MGCORE_API char* mg_telemetry_register_series(mg_core_t* core, const char* name, const char* unit);
MGCORE_API char* mg_telemetry_append(mg_core_t* core, const char* name, float value);
MGCORE_API char* mg_telemetry_get_series(mg_core_t* core, const char* name);
MGCORE_API char* mg_telemetry_get_stats(mg_core_t* core, const char* name);
MGCORE_API char* mg_telemetry_get_series_names(mg_core_t* core);
MGCORE_API char* mg_telemetry_clear(mg_core_t* core);

MGCORE_API char* mg_sensors_get_health(mg_core_t* core);
MGCORE_API char* mg_sensors_get_statustext(mg_core_t* core);
MGCORE_API char* mg_health_get_all(mg_core_t* core);

MGCORE_API char* mg_sensor_decode_device_id(mg_core_t* core, int device_id, const char* category);
MGCORE_API char* mg_sensor_decode_px4_id(mg_core_t* core, int device_id, const char* category);
MGCORE_API char* mg_sensor_decode_px4_type(mg_core_t* core, int type_value, const char* category);
MGCORE_API char* mg_sensor_decode_type_param(mg_core_t* core, int type_value, const char* category);
MGCORE_API char* mg_sensors_identify_all(mg_core_t* core);

MGCORE_API char* mg_rc_get_channels(mg_core_t* core);
MGCORE_API char* mg_rc_get_channel(mg_core_t* core, int channel);
MGCORE_API char* mg_servo_get_outputs(mg_core_t* core);
MGCORE_API char* mg_servo_get_output(mg_core_t* core, int channel);

MGCORE_API char* mg_vehicle_count(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_flight_time(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_adsb_traffic(mg_core_t* core);
MGCORE_API char* mg_vehicle_get_distance_sensors(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_TELEMETRY_H */
