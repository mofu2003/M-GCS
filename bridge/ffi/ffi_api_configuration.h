#ifndef MGCORE_FFI_API_CONFIGURATION_H
#define MGCORE_FFI_API_CONFIGURATION_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_config_get_groups(mg_core_t* core);
MGCORE_API char* mg_config_get_group_descriptor(mg_core_t* core, const char* group_name);
MGCORE_API char* mg_config_read_group(mg_core_t* core, const char* group_name);
/// 语义角色 → 固件参数名。角色名由前端定义（与固件、机型无关），参数名只由
/// 插件提供（safety.md S6）。返回 {"profile":"...","roles":{...}}；值里的
/// `{n}` 是序号占位符，由调用方替换。没有角色表的固件返回空 roles，不是错误。
MGCORE_API char* mg_config_get_param_roles(mg_core_t* core);
MGCORE_API char* mg_config_validate_param(mg_core_t* core, const char* param_name, float value);
MGCORE_API char* mg_config_get_presets(mg_core_t* core);
MGCORE_API char* mg_config_apply_preset(mg_core_t* core, const char* preset_name);

MGCORE_API char* mg_serial_get_configs(mg_core_t* core);
MGCORE_API char* mg_serial_get_uart_names(mg_core_t* core);

MGCORE_API char* mg_actuator_get_mapping(mg_core_t* core);
MGCORE_API char* mg_actuator_set_function(mg_core_t* core, int channel, int function_id);
MGCORE_API char* mg_actuator_get_limits(mg_core_t* core, int channel);
MGCORE_API char* mg_actuator_set_limits(mg_core_t* core, int channel, int min_pwm, int max_pwm, int trim_pwm);
MGCORE_API char* mg_actuator_motor_test(mg_core_t* core, int motor, float throttle_pct, int duration_ms);

MGCORE_API char* mg_rc_get_mapping(mg_core_t* core);
MGCORE_API char* mg_rc_set_mapping(mg_core_t* core, const char* function, int channel);
MGCORE_API char* mg_rc_get_calibration(mg_core_t* core);
MGCORE_API char* mg_rc_get_failsafe(mg_core_t* core);
MGCORE_API char* mg_rc_set_failsafe(mg_core_t* core, int mode, int threshold);

/* type: 0=gyro, 1=accel, 2=compass, 3=baro, 4=radio (RC transmitter travel). */
MGCORE_API char* mg_calibration_start(mg_core_t* core, int type, int accel_mode);
/// 取消校准。返回 {"success":true,"state":"cancelling"} 只表示取消命令已下发；
/// 真正停止要等飞控 COMMAND_ACK，期间 mg_calibration_get_status 的 active 仍为
/// true、stage 为 "cancelling"，确认（或 10 s 取消超时/链路断开）后才落到
/// "cancelled"。界面在确认前不得显示为已停止（safety.md S8）。
MGCORE_API char* mg_calibration_cancel(mg_core_t* core);
MGCORE_API char* mg_calibration_get_status(mg_core_t* core);
MGCORE_API char* mg_calibration_get_history(mg_core_t* core, int type);
MGCORE_API char* mg_calibration_get_retry_info(mg_core_t* core);
MGCORE_API char* mg_calibration_accel_confirm(mg_core_t* core, int position);
MGCORE_API char* mg_calibration_start_airspeed(mg_core_t* core);


#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_CONFIGURATION_H */
