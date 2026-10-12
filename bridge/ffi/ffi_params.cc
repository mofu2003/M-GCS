#define MGCORE_EXPORTS
/**
 * ffi_params.cc - Section 3: Parameters
 *
 * 参数读写：请求全部参数、按名查询/设置、进度查询。
 *
 * mg_params_request_all, mg_params_get_all, mg_params_get_progress,
 * mg_params_get_by_name, mg_params_get_write_status, mg_params_set
 */
#include "ffi_internal.h"
#include "ffi_response.h"
#include "core/telemetry/param_json_serializers.h"
#include "core/config/parameter_change_set.h"

#include <unordered_set>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

namespace {

using mg::core::config::ParamEntry;
using mg::core::config::ParameterChange;
using mg::core::config::ParameterChangeDisposition;
using mg::core::config::ParameterChangeSet;
using mg::core::config::ParameterChangeSetPlanner;

const char* disposition_name(ParameterChangeDisposition disposition) {
    switch (disposition) {
        case ParameterChangeDisposition::Ready: return "ready";
        case ParameterChangeDisposition::Unchanged: return "unchanged";
        case ParameterChangeDisposition::Unknown: return "unknown";
        case ParameterChangeDisposition::Invalid: return "invalid";
    }
    return "invalid";
}

bool parse_change_set(const char* json, std::vector<ParamEntry>& entries, std::string& error) {
    if (!json || json[0] == '\0') {
        error = "empty parameter change-set";
        return false;
    }
    cJSON* root = mg::core::json::parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        error = "expected JSON array";
        return false;
    }
    const int size = cJSON_GetArraySize(root);
    entries.reserve(size);
    for (int index = 0; index < size; index++) {
        cJSON* item = cJSON_GetArrayItem(root, index);
        cJSON* name = item ? cJSON_GetObjectItemCaseSensitive(item, "name") : nullptr;
        cJSON* value = item ? cJSON_GetObjectItemCaseSensitive(item, "value") : nullptr;
        cJSON* type = item ? cJSON_GetObjectItemCaseSensitive(item, "type") : nullptr;
        if (!name || !cJSON_IsString(name) || !name->valuestring ||
            !value || !cJSON_IsNumber(value)) {
            cJSON_Delete(root);
            error = "every change requires a name and numeric value";
            return false;
        }
        ParamEntry entry;
        entry.name = name->valuestring;
        entry.value = static_cast<float>(value->valuedouble);
        if (type && cJSON_IsNumber(type) && type->valueint >= 0 && type->valueint <= 255) {
            entry.type = static_cast<uint8_t>(type->valueint);
        }
        entries.push_back(std::move(entry));
    }
    cJSON_Delete(root);
    return true;
}

std::string change_set_json(const ParameterChangeSet& change_set) {
    mg::core::telemetry::JsonBuilder output;
    output.begin_object();
    output.field("success", true);
    output.field("ready", change_set.ready_count);
    output.field("unchanged", change_set.unchanged_count);
    output.field("unknown", change_set.unknown_count);
    output.field("invalid", change_set.invalid_count);
    mg::core::telemetry::JsonBuilder items;
    items.begin_array();
    for (const auto& change : ParameterChangeSetPlanner::ordered_ready(change_set)) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("name", change.entry.name);
        item.field("value", change.entry.value);
        item.field("type", static_cast<int>(change.entry.type));
        item.field("disposition", disposition_name(change.disposition));
        if (!change.reason.empty()) item.field("reason", change.reason);
        item.end_object();
        items.append_json(item.str());
    }
    for (const auto& change : change_set.changes) {
        if (change.disposition == ParameterChangeDisposition::Ready) continue;
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("name", change.entry.name);
        item.field("value", change.entry.value);
        item.field("type", static_cast<int>(change.entry.type));
        item.field("disposition", disposition_name(change.disposition));
        if (!change.reason.empty()) item.field("reason", change.reason);
        item.end_object();
        items.append_json(item.str());
    }
    items.end_array();
    output.raw_field("items", items.str());
    output.end_object();
    return output.str();
}

} // namespace

MGCORE_API char* mg_params_request_all(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    if (mav->request_param_list(active_target_sys(core), active_target_comp(core))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}



MGCORE_API char* mg_params_get_all(mg_core_t* core) {
    CHECK_CORE(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return to_cstr("[]");
    auto params = mav->all_params();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();
    for (const auto& p : params) {
        jb.append_json(mg::core::telemetry::param_json(p));
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_params_get_progress(mg_core_t* core) {
    CHECK_CORE(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return to_cstr(R"({"received":0,"total":0})");
    return to_cstr(mg::core::telemetry::param_progress_json(
        static_cast<uint16_t>(mav->params_download_received()), mav->param_count()));
}

MGCORE_API char* mg_params_get_by_name(mg_core_t* core, const char* param_name) {
    CHECK_CORE(core);
    if (!param_name || param_name[0] == '\0') return to_cstr("{\"found\":false}");
    auto mav = shared_active_mavlink(core);
    if (!mav) return to_cstr("{\"found\":false}");
    auto p = mav->get_param_by_name(param_name);
    if (!p.has_value()) return to_cstr(mg::core::telemetry::param_not_found_json());
    return to_cstr(mg::core::telemetry::param_detail_json(p.value()));
}

MGCORE_API char* mg_params_get_write_status(mg_core_t* core) {
    CHECK_CORE_RET(core, mg::bridge::ffi::response::error(MG_ERROR_NULL_CORE, "null core"));

    auto mav = shared_active_mavlink(core);
    mg::core::telemetry::JsonBuilder writes;
    writes.begin_array();
    if (mav) {
        for (const auto& record : mav->param_change_history()) {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("id", static_cast<unsigned long long>(record.id));
            item.field("name", record.name);
            item.field("requested_value", record.requested_value);
            item.field("actual_value", record.actual_value);
            item.field("type", static_cast<int>(record.type));
            item.field("state", record.state);
            item.field("error", record.error);
            item.field("requested_at_ms", static_cast<int64_t>(record.requested_at_ms));
            item.field("completed_at_ms", static_cast<int64_t>(record.completed_at_ms));
            item.end_object();
            writes.append_json(item.str());
        }
    }
    mg::core::telemetry::JsonBuilder data;
    data.begin_object();
    data.raw_field("writes", writes.str());
    data.end_object();
    return mg::bridge::ffi::response::ok(data.str());
}
