#ifndef MGCORE_FFI_API_ENVIRONMENT_H
#define MGCORE_FFI_API_ENVIRONMENT_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_terrain_query(mg_core_t* core, double lat, double lon);
MGCORE_API char* mg_terrain_query_line(mg_core_t* core,
                                        double lat1, double lon1,
                                        double lat2, double lon2,
                                        int samples);

MGCORE_API char* mg_sitl_start(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_sitl_stop(mg_core_t* core);
MGCORE_API char* mg_sitl_status(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_ENVIRONMENT_H */
