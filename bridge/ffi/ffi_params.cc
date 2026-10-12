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

MGCORE_API char* mg_params_refresh(mg_core_t* core, const char* json_names) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!json_names || json_names[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty parameter refresh list");
    }

    cJSON* root = mg::core::json::parse(json_names);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "expected JSON array of parameter names");
    }

    auto mav = shared_active_mavlink(core);
    if (!mav || mav->params_received() == 0) {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_STATE, "parameters are not loaded");
    }

    const auto params = mav->all_params();
    std::unordered_set<std::string> available;
    available.reserve(params.size());
    for (const auto& param : params) available.insert(param.name);

    std::vector<std::string> resolved;
    std::unordered_set<std::string> seen;
    std::vector<std::string> unknown;
    int requested = 0;
    int duplicates = 0;

    const int size = cJSON_GetArraySize(root);
    for (int index = 0; index < size; ++index) {
        cJSON* item = cJSON_GetArrayItem(root, index);
        if (!item || !cJSON_IsString(item) || !item->valuestring || item->valuestring[0] == '\0') {
            continue;
        }
        requested++;
        const std::string entry = item->valuestring;
        if (entry.back() == '*') {
            const std::string prefix = entry.substr(0, entry.size() - 1);
            if (prefix.empty()) {
                unknown.push_back(entry);
                continue;
            }
            for (const auto& param : params) {
                const std::string canonical_name =
                    ffi_param_canonical_name(core, param.name);
                if (canonical_name.rfind(prefix, 0) != 0) continue;
                if (seen.insert(param.name).second) resolved.push_back(param.name);
                else duplicates++;
            }
            continue;
        }

        const std::string vehicle_name = ffi_param_vehicle_name(core, entry);
        if (available.count(vehicle_name) == 0) {
            unknown.push_back(entry);
            continue;
        }
        if (seen.insert(vehicle_name).second) resolved.push_back(vehicle_name);
        else duplicates++;
    }
    cJSON_Delete(root);

    const int sent = mav->start_param_refresh(
        active_target_sys(core), active_target_comp(core), resolved);
    const int failed = static_cast<int>(resolved.size()) - sent;
    const auto status = mav->param_refresh_status();

    mg::core::telemetry::JsonBuilder output;
    output.begin_object();
    output.field("success", failed == 0);
    if (failed != 0) {
        write_error(output, MG_ERROR_SEND_FAILED,
                    std::to_string(failed) + " parameter refresh requests were not sent");
    }
    output.field("requested", requested);
    output.field("resolved", static_cast<int>(resolved.size()));
    output.field("sent", sent);
    output.field("failed", failed);
    output.field("duplicates", duplicates);
    output.field("active", status.active);
    output.field("pending", status.pending);
    output.field("completed", status.completed);
    output.field("round", status.round);
    mg::core::telemetry::JsonBuilder unknown_json;
    unknown_json.begin_array();
    for (const auto& name : unknown) unknown_json.append_string(name);
    unknown_json.end_array();
    output.raw_field("unknown", unknown_json.str());
    output.end_object();
    return to_cstr(output.str());
}

MGCORE_API char* mg_params_get_refresh_status(mg_core_t* core) {
    CHECK_CORE(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) {
        return to_cstr(R"({"active":false,"total":0,"completed":0,"pending":0,"round":0,"failed":[]})");
    }
    const auto status = mav->param_refresh_status();
    mg::core::telemetry::JsonBuilder output;
    output.begin_object();
    output.field("active", status.active);
    output.field("total", status.total);
    output.field("completed", status.completed);
    output.field("pending", status.pending);
    output.field("round", status.round);
    mg::core::telemetry::JsonBuilder failed;
    failed.begin_array();
    for (const auto& name : status.failed) failed.append_string(name);
    failed.end_array();
    output.raw_field("failed", failed.str());
    output.end_object();
    return to_cstr(output.str());
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

MGCORE_API char* mg_params_set(mg_core_t* core, const char* param_name, float param_value, int param_type) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!param_name || param_name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty param name");

    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");

    uint16_t total = mav->param_count();
    size_t received = mav->params_received();
    if (total == 0 || received < static_cast<size_t>(total)) {
        return fail(MG_ERROR_INVALID_STATE, "params not loaded; call mg_params_request_all() first and wait for completion");
    }

    const std::string vehicle_name = ffi_param_vehicle_name(core, param_name);
    uint8_t mav_type = static_cast<uint8_t>(param_type);
    if (mav_type == 0) {
        auto p = mav->get_param_by_name(vehicle_name.c_str());
        if (!p.has_value()) p = mav->get_param_by_name(param_name);
        if (!p.has_value()) return fail(MG_ERROR_NOT_FOUND, "parameter not found in cache");
        mav_type = p->type;
    }

    if (mav->set_param(active_target_sys(core), active_target_comp(core),
                       vehicle_name.c_str(), param_value, mav_type)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_params_prepare_change_set(mg_core_t* core, const char* json_params) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    if (mav->param_count() == 0 ||
        mav->params_received() < static_cast<size_t>(mav->param_count())) {
        return fail(MG_ERROR_INVALID_STATE, "parameters are not fully loaded");
    }

    std::vector<ParamEntry> entries;
    std::string error;
    if (!parse_change_set(json_params, entries, error)) {
        return fail(MG_ERROR_INVALID_PARAM, error.c_str());
    }
    return to_cstr(change_set_json(ParameterChangeSetPlanner::prepare(entries, mav->all_params())));
}

MGCORE_API char* mg_params_commit_change_set(mg_core_t* core, const char* json_params) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");

    const uint16_t total = mav->param_count();
    if (total == 0 || mav->params_received() < static_cast<size_t>(total)) {
        return fail(MG_ERROR_INVALID_STATE, "parameters are not fully loaded");
    }

    std::vector<ParamEntry> entries;
    std::string error;
    if (!parse_change_set(json_params, entries, error)) {
        return fail(MG_ERROR_INVALID_PARAM, error.c_str());
    }
    const auto prepared = ParameterChangeSetPlanner::prepare(entries, mav->all_params());
    mg::core::telemetry::JsonBuilder output;
    output.begin_object();
    mg::core::telemetry::JsonBuilder succeeded;
    succeeded.begin_array();
    mg::core::telemetry::JsonBuilder failed;
    failed.begin_array();
    int written = 0;
    int resolved = 0;
    for (const auto& change : ParameterChangeSetPlanner::ordered_ready(prepared)) {
        auto& descriptor_db = get_desc_db();
        const auto validation_error = descriptor_db.validate(
            ffi_param_profile(core), change.entry.name, change.entry.value);
        if (!validation_error.empty()) {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("name", change.entry.name);
            item.field("reason", validation_error);
            item.end_object();
            failed.append_json(item.str());
            continue;
        }
        if (mav->set_param(active_target_sys(core), active_target_comp(core),
                           change.entry.name.c_str(), change.entry.value, change.entry.type)) {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("name", change.entry.name);
            item.field("value", change.entry.value);
            item.end_object();
            succeeded.append_json(item.str());
            written++;
            resolved++;
        } else {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("name", change.entry.name);
            item.field("reason", "failed to send parameter write");
            item.end_object();
            failed.append_json(item.str());
        }
    }
    for (const auto& change : prepared.changes) {
        if (change.disposition == ParameterChangeDisposition::Ready) continue;
        if (change.disposition == ParameterChangeDisposition::Unchanged) {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("name", change.entry.name);
            item.field("value", change.entry.value);
            item.end_object();
            succeeded.append_json(item.str());
            resolved++;
        } else {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("name", change.entry.name);
            item.field("reason", change.reason.empty()
                ? "parameter is no longer available"
                : change.reason);
            item.end_object();
            failed.append_json(item.str());
        }
    }
    succeeded.end_array();
    failed.end_array();
    const bool all_applied = static_cast<int>(prepared.changes.size()) == resolved;
    output.field("success", all_applied);
    if (!all_applied) {
        write_error(output, MG_ERROR_REJECTED,
                    std::to_string(static_cast<int>(prepared.changes.size()) - resolved) +
                        " parameter changes did not apply");
    }
    output.field("written", written);
    output.field("unchanged", prepared.unchanged_count);
    output.field("unknown", prepared.unknown_count);
    output.raw_field("succeeded", succeeded.str());
    output.raw_field("failed", failed.str());
    output.end_object();
    return to_cstr(output.str());
}
