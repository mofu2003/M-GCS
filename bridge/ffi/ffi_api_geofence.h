#ifndef MGCORE_FFI_API_GEOFENCE_H
#define MGCORE_FFI_API_GEOFENCE_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_geofence_check(mg_core_t* core, double lat, double lon);
MGCORE_API char* mg_geofence_get_config(mg_core_t* core);

MGCORE_API char* mg_fence_download(mg_core_t* core);
MGCORE_API char* mg_fence_get_plan(mg_core_t* core);
MGCORE_API char* mg_fence_enable(mg_core_t* core, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_GEOFENCE_H */
