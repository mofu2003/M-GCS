#ifndef MGCORE_FFI_API_CONFIGURATION_H
#define MGCORE_FFI_API_CONFIGURATION_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_config_get_groups(mg_core_t* core);
MGCORE_API char* mg_config_get_group_descriptor(mg_core_t* core, const char* group_name);
MGCORE_API char* mg_config_read_group(mg_core_t* core, const char* group_name);
MGCORE_API char* mg_config_validate_param(mg_core_t* core, const char* param_name, float value);
MGCORE_API char* mg_config_get_presets(mg_core_t* core);
MGCORE_API char* mg_config_apply_preset(mg_core_t* core, const char* preset_name);

MGCORE_API char* mg_serial_get_configs(mg_core_t* core);
MGCORE_API char* mg_serial_get_uart_names(mg_core_t* core);
MGCORE_API char* mg_serial_get_config(mg_core_t* core, const char* port_name);
MGCORE_API char* mg_serial_set_baud(mg_core_t* core, const char* port_name, int baud);
MGCORE_API char* mg_serial_set_protocol(mg_core_t* core, const char* port_name, int protocol);

MGCORE_API char* mg_actuator_get_mapping(mg_core_t* core);
MGCORE_API char* mg_actuator_set_function(mg_core_t* core, int channel, int function_id);
MGCORE_API char* mg_actuator_get_limits(mg_core_t* core, int channel);
MGCORE_API char* mg_actuator_set_limits(mg_core_t* core, int channel, int min_pwm, int max_pwm, int trim_pwm);
MGCORE_API char* mg_actuator_motor_test(mg_core_t* core, int motor, float throttle_pct, int duration_ms);

MGCORE_API char* mg_rc_get_mapping(mg_core_t* core);
MGCORE_API char* mg_rc_set_mapping(mg_core_t* core, const char* function, int channel);
MGCORE_API char* mg_rc_get_calibration(mg_core_t* core);
MGCORE_API char* mg_rc_calibrate_start(mg_core_t* core);
MGCORE_API char* mg_rc_calibrate_status(mg_core_t* core);
MGCORE_API char* mg_rc_calibrate_save(mg_core_t* core);
MGCORE_API char* mg_rc_get_failsafe(mg_core_t* core);
MGCORE_API char* mg_rc_set_failsafe(mg_core_t* core, int mode, int threshold);

/* Legacy calibration names. New consumers use mg_calibration_* below. */
MGCORE_API char* mg_cal_start(mg_core_t* core, int type, int mode);
MGCORE_API char* mg_cal_cancel(mg_core_t* core);
MGCORE_API char* mg_cal_status(mg_core_t* core);
MGCORE_API char* mg_cal_history(mg_core_t* core, int type);
MGCORE_API char* mg_cal_retry_info(mg_core_t* core);
MGCORE_API char* mg_calibration_start(mg_core_t* core, int type, int accel_mode);
MGCORE_API char* mg_calibration_cancel(mg_core_t* core);
MGCORE_API char* mg_calibration_get_status(mg_core_t* core);
MGCORE_API char* mg_calibration_get_history(mg_core_t* core, int type);
MGCORE_API char* mg_calibration_get_retry_info(mg_core_t* core);
MGCORE_API char* mg_calibration_accel_confirm(mg_core_t* core, int position);
MGCORE_API char* mg_calibration_start_airspeed(mg_core_t* core);

MGCORE_API char* mg_px4_actuator_get_config(mg_core_t* core);
MGCORE_API char* mg_px4_actuator_set_config(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_px4_flight_mode_get_config(mg_core_t* core);
MGCORE_API char* mg_px4_safety_get(mg_core_t* core);
MGCORE_API char* mg_px4_safety_set(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_px4_tuning_get(mg_core_t* core);
MGCORE_API char* mg_px4_tuning_set(mg_core_t* core, const char* json_config);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_CONFIGURATION_H */
