#ifndef MGCORE_FFI_API_ENVIRONMENT_H
#define MGCORE_FFI_API_ENVIRONMENT_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_sitl_start(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_sitl_stop(mg_core_t* core);
MGCORE_API char* mg_sitl_status(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_ENVIRONMENT_H */
