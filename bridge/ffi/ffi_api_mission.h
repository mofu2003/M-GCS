#ifndef MGCORE_FFI_API_MISSION_H
#define MGCORE_FFI_API_MISSION_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_mission_upload(mg_core_t* core, const char* json_wps);
MGCORE_API char* mg_mission_download(mg_core_t* core);
MGCORE_API char* mg_mission_start(mg_core_t* core);
MGCORE_API char* mg_mission_clear(mg_core_t* core);
MGCORE_API char* mg_mission_request(mg_core_t* core, const char* json_request);
MGCORE_API char* mg_mission_get_progress(mg_core_t* core);
MGCORE_API char* mg_mission_get_plan(mg_core_t* core);
MGCORE_API char* mg_mission_set_current(mg_core_t* core, int seq);
MGCORE_API char* mg_mission_get_current(mg_core_t* core);

MGCORE_API char* mg_mission_add_item(mg_core_t* core, const char* json);
MGCORE_API char* mg_mission_update_item(mg_core_t* core, const char* json);
MGCORE_API char* mg_mission_remove_item(mg_core_t* core, int index);
MGCORE_API char* mg_mission_move_item(mg_core_t* core, int from_index, int to_index);
MGCORE_API char* mg_mission_validate(mg_core_t* core);
MGCORE_API char* mg_mission_get_stats(mg_core_t* core);
MGCORE_API char* mg_mission_export_json(mg_core_t* core);
MGCORE_API char* mg_mission_import_json(mg_core_t* core, const char* json);
MGCORE_API char* mg_mission_export_csv(mg_core_t* core);
MGCORE_API char* mg_mission_import_csv(mg_core_t* core, const char* csv);
MGCORE_API char* mg_mission_get_templates(mg_core_t* core);

MGCORE_API char* mg_survey_generate(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_survey_generate_corridor(mg_core_t* core, const char* json_config);

MGCORE_API char* mg_rally_download(mg_core_t* core);
MGCORE_API char* mg_rally_get_plan(mg_core_t* core);
MGCORE_API char* mg_rally_clear(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_MISSION_H */
