#define MGCORE_EXPORTS
/**
 * ffi_telemetry.cc - Telemetry history and log persistence FFI
 *
 * 遥测环形缓冲区、飞行日志记录、参数文件 I/O、数据新鲜度查询。
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/settings/flight_log_writer.h"
#include "core/config/param_file_io.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

// ================================================================
// Telemetry History FFI
// ================================================================

MGCORE_API char* mg_telemetry_register_series(mg_core_t* core, const char* name, const char* unit) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null name");

    get_active_ring_buffer(core).register_series(name, unit ? unit : "");
    return ok();
}

MGCORE_API char* mg_telemetry_append(mg_core_t* core, const char* name, float value) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null name");

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    get_active_ring_buffer(core).append(name, static_cast<uint64_t>(ms), value);
    return ok();
}

MGCORE_API char* mg_telemetry_get_series(mg_core_t* core, const char* name) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return to_cstr("[]");

    return to_cstr(get_active_ring_buffer(core).to_json(name));
}

MGCORE_API char* mg_telemetry_get_stats(mg_core_t* core, const char* name) {
    CHECK_CORE(core);
    if (!name || name[0] == '\0') return to_cstr("{}");

    return to_cstr(get_active_ring_buffer(core).stats_json(name));
}

MGCORE_API char* mg_telemetry_get_series_names(mg_core_t* core) {
    CHECK_CORE(core);

    auto names = get_active_ring_buffer(core).get_series_names();

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& n : names) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", n);
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_telemetry_clear(mg_core_t* core) {
    CHECK_CORE(core);
    get_active_ring_buffer(core).clear();
    return ok();
}

// ================================================================
// Flight Log Persistence FFI
// ================================================================

MGCORE_API char* mg_log_file_start(mg_core_t* core, const char* session_name) {
    CHECK_CORE(core);

    std::string path = core->log_writer.start_session(session_name ? session_name : "");

    if (path.empty()) {
        return fail(MG_ERROR_INTERNAL, "failed to create log file");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("file", path);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_log_file_stop(mg_core_t* core) {
    CHECK_CORE(core);
    core->log_writer.stop_session();
    return ok();
}

MGCORE_API char* mg_log_file_status(mg_core_t* core) {
    CHECK_CORE(core);

    auto& writer = core->log_writer;

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("active", writer.is_active());
    jb.field("file", writer.current_file_path());
    jb.field("events", static_cast<int64_t>(writer.event_count()));
    jb.field("bytes", static_cast<int64_t>(writer.bytes_written()));
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// Parameter File I/O FFI
// ================================================================

MGCORE_API char* mg_param_file_export(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return to_cstr("");

    cJSON* root = cJSON_Parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return to_cstr("");
    }

    std::vector<mg::core::config::ParamEntry> params;
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        if (name && cJSON_IsString(name) && value && cJSON_IsNumber(value)) {
            params.push_back({name->valuestring, static_cast<float>(value->valuedouble)});
        }
    }
    cJSON_Delete(root);

    return to_cstr(mg::core::config::ParamFileIO::export_param(params));
}

MGCORE_API char* mg_param_file_import(mg_core_t* core, const char* content) {
    CHECK_CORE(core);
    if (!content || content[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty content");

    auto params = mg::core::config::ParamFileIO::import_param(content);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", !params.empty());
    jb.field("count", static_cast<int>(params.size()));
    mg::bridge::JsonBuilder items;
    items.begin_array();
    for (const auto& param : params) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", param.name);
        item.field("value", param.value);
        item.field("type", static_cast<int>(param.type));
        item.end_object();
        items.append_json(item.str());
    }
    items.end_array();
    jb.raw_field("items", items.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_param_file_export_csv(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return to_cstr("");

    cJSON* root = cJSON_Parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return to_cstr("");
    }

    std::vector<mg::core::config::ParamEntry> params;
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        if (name && cJSON_IsString(name) && value && cJSON_IsNumber(value)) {
            params.push_back({name->valuestring, static_cast<float>(value->valuedouble)});
        }
    }
    cJSON_Delete(root);

    return to_cstr(mg::core::config::ParamFileIO::export_csv(params));
}

MGCORE_API char* mg_param_file_import_csv(mg_core_t* core, const char* content) {
    CHECK_CORE(core);
    if (!content || content[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty content");

    auto params = mg::core::config::ParamFileIO::import_csv(content);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("count", static_cast<int>(params.size()));
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// ffi_api.h: mg_telemetry_get_freshness
// ================================================================
MGCORE_API char* mg_telemetry_get_freshness(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"connected":false,"groups":{}})");
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("connected", is_active_connected(core));
    jb.raw_field("groups", cache->freshness_json());
    jb.end_object();
    return to_cstr(jb.str());
}
