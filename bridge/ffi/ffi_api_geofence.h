#ifndef MGCORE_FFI_API_GEOFENCE_H
#define MGCORE_FFI_API_GEOFENCE_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_geofence_get(mg_core_t* core);
MGCORE_API char* mg_geofence_set(mg_core_t* core, const char* json_fence);
MGCORE_API char* mg_geofence_check(mg_core_t* core, double lat, double lon);
MGCORE_API char* mg_geofence_get_config(mg_core_t* core);
MGCORE_API char* mg_geofence_set_config(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_geofence_add_point(mg_core_t* core, double lat, double lon, float alt);
MGCORE_API char* mg_geofence_remove_point(mg_core_t* core, int index);
MGCORE_API char* mg_geofence_get_points(mg_core_t* core);
MGCORE_API char* mg_geofence_validate(mg_core_t* core);
MGCORE_API char* mg_geofence_export_json(mg_core_t* core);
MGCORE_API char* mg_geofence_import_json(mg_core_t* core, const char* json);
MGCORE_API char* mg_geofence_export_kml(mg_core_t* core, const char* name);

MGCORE_API char* mg_fence_download(mg_core_t* core);
MGCORE_API char* mg_fence_get_plan(mg_core_t* core);
MGCORE_API char* mg_fence_clear(mg_core_t* core);
MGCORE_API char* mg_fence_enable(mg_core_t* core, int enable);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_GEOFENCE_H */
