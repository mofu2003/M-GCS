#ifndef MGCORE_FFI_API_PARAMS_H
#define MGCORE_FFI_API_PARAMS_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_param_list(mg_core_t* core);
MGCORE_API char* mg_param_get(mg_core_t* core, const char* name);
MGCORE_API char* mg_param_set(mg_core_t* core, const char* name, float value);

MGCORE_API char* mg_params_request_all(mg_core_t* core);
MGCORE_API char* mg_params_get_all(mg_core_t* core);
MGCORE_API char* mg_params_get_progress(mg_core_t* core);
MGCORE_API char* mg_params_get_by_name(mg_core_t* core, const char* param_name);
MGCORE_API char* mg_params_set(mg_core_t* core, const char* param_name, float param_value, int param_type);
MGCORE_API char* mg_params_get_changed(mg_core_t* core);
MGCORE_API char* mg_params_save_to_file(mg_core_t* core, const char* filepath);
MGCORE_API char* mg_params_load_from_file(mg_core_t* core, const char* filepath);

MGCORE_API char* mg_config_read_param(mg_core_t* core, const char* param_name);
MGCORE_API char* mg_config_write_param(mg_core_t* core, const char* param_name, float value);
MGCORE_API char* mg_config_write_params(mg_core_t* core, const char* json_params);

MGCORE_API char* mg_param_file_export(mg_core_t* core, const char* json);
MGCORE_API char* mg_param_file_import(mg_core_t* core, const char* content);
MGCORE_API char* mg_param_file_export_csv(mg_core_t* core, const char* json);
MGCORE_API char* mg_param_file_import_csv(mg_core_t* core, const char* content);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_PARAMS_H */
