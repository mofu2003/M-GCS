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

MGCORE_API char* mg_mission_doc_create(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_mission_doc_get(mg_core_t* core);
MGCORE_API char* mg_mission_doc_apply(mg_core_t* core, const char* json_command);
MGCORE_API char* mg_mission_doc_undo(mg_core_t* core);
MGCORE_API char* mg_mission_doc_redo(mg_core_t* core);
MGCORE_API char* mg_mission_doc_validate(mg_core_t* core);
MGCORE_API char* mg_mission_doc_compile(mg_core_t* core);
MGCORE_API char* mg_mission_doc_estimate(mg_core_t* core);
MGCORE_API char* mg_mission_doc_upload(
    mg_core_t* core,
    unsigned long long expected_revision);
MGCORE_API char* mg_mission_doc_verify(mg_core_t* core, double tolerance_m);
// 把飞控当前任务读进任务文档（替换实体列表，保留 metadata / defaults / home）。
// 需要一次已完成的任务下载，否则返回 invalid_state：读取语义与 MP 的 Read、
// QGC 的 Download 一致，调用方应在覆盖本地改动前自行确认。
MGCORE_API char* mg_mission_doc_import_from_vehicle(mg_core_t* core);
MGCORE_API char* mg_mission_get_command_catalog(mg_core_t* core);
MGCORE_API char* mg_mission_doc_export(mg_core_t* core);
MGCORE_API char* mg_mission_doc_import(mg_core_t* core, const char* json_document);
MGCORE_API char* mg_mission_doc_save(mg_core_t* core, const char* filepath);
MGCORE_API char* mg_mission_doc_load(mg_core_t* core, const char* filepath);
// 文件互操作：支持 "waypoints"（Mission Planner / QGC 的 QGC WPL 110 文本）
// 和 "plan"（QGroundControl JSON）。ArduPilot/MP 默认推荐 waypoints，PX4/QGC
// 默认推荐 plan；导出的文本放在返回 JSON 的 "text" 字段里，导入用同一份文本。
MGCORE_API char* mg_mission_doc_export_format(
    mg_core_t* core,
    const char* format);
MGCORE_API char* mg_mission_doc_import_format(
    mg_core_t* core,
    const char* format,
    const char* text);

MGCORE_API char* mg_survey_generate(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_survey_generate_corridor(mg_core_t* core, const char* json_config);

MGCORE_API char* mg_rally_download(mg_core_t* core);
MGCORE_API char* mg_rally_get_plan(mg_core_t* core);
MGCORE_API char* mg_rally_clear(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_MISSION_H */
