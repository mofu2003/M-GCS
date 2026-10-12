#ifndef MGCORE_FFI_API_PARAMS_H
#define MGCORE_FFI_API_PARAMS_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_params_request_all(mg_core_t* core);
MGCORE_API char* mg_params_get_all(mg_core_t* core);
MGCORE_API char* mg_params_get_progress(mg_core_t* core);
MGCORE_API char* mg_params_get_by_name(mg_core_t* core, const char* param_name);
// 参数写入事务状态：每条记录带 id、目标值、回读值、MAV_PARAM_TYPE 与状态
// (pending/confirmed/failed)；failed 记录携带飞控拒绝（PARAM_ERROR）或超时的原因。
// 写入由 mg_config_write_param / mg_params_set 触发，它们的 success 只表示
// "已交给发送层"（send-only）；调用方必须用本接口或回读参数值确认结果，
// 不能把发送成功当成写入完成（safety.md S2 的写入确认语义）。
MGCORE_API char* mg_params_get_write_status(mg_core_t* core);

MGCORE_API char* mg_config_read_param(mg_core_t* core, const char* param_name);
MGCORE_API char* mg_config_write_param(mg_core_t* core, const char* param_name, float value);


#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_PARAMS_H */
