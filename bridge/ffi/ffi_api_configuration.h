#ifndef MGCORE_FFI_API_CONFIGURATION_H
#define MGCORE_FFI_API_CONFIGURATION_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 语义角色 → 固件参数名。角色名由前端定义（与固件、机型无关），参数名只由
/// 插件提供（safety.md S6）。返回 {"profile":"...","roles":{...}}；值里的
/// `{n}` 是序号占位符，由调用方替换。没有角色表的固件返回空 roles，不是错误。
MGCORE_API char* mg_config_get_param_roles(mg_core_t* core);

MGCORE_API char* mg_serial_get_uart_names(mg_core_t* core);



/* type: 0=gyro, 1=accel, 2=compass, 3=baro, 4=radio (RC transmitter travel). */
MGCORE_API char* mg_calibration_start(mg_core_t* core, int type, int accel_mode);
/// 取消校准。返回 {"success":true,"state":"cancelling"} 只表示取消命令已下发；
/// 真正停止要等飞控 COMMAND_ACK，期间 mg_calibration_get_status 的 active 仍为
/// true、stage 为 "cancelling"，确认（或 10 s 取消超时/链路断开）后才落到
/// "cancelled"。界面在确认前不得显示为已停止（safety.md S8）。
MGCORE_API char* mg_calibration_cancel(mg_core_t* core);
MGCORE_API char* mg_calibration_get_status(mg_core_t* core);
MGCORE_API char* mg_calibration_accel_confirm(mg_core_t* core, int position);


#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_CONFIGURATION_H */
