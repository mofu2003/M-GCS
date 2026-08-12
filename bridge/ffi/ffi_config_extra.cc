#define MGCORE_EXPORTS
/**
 * ffi_config_extra.cc - Additional config FFI functions
 *
 * 配置补充：单参数读写（带元数据）、批量写入、任务请求。
 *
 * Implements mg_config_read_param, mg_config_write_param, mg_config_write_params,
 * and mg_mission_request which were declared in ffi_api.h but not yet implemented.
 */
#include "ffi_internal.h"
#include "json_serializers.h"
// get_desc_db() shared via ffi_internal.h

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"


// ================================================================
// mg_config_read_param - Read a single parameter with full metadata
// ================================================================
MGCORE_API char* mg_config_read_param(mg_core_t* core, const char* param_name) {
    CHECK_CORE(core);
    if (!param_name || param_name[0] == '\0') return to_cstr("{}");

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);
    auto desc = db.get(profile, param_name);

    auto* mav = get_active_mavlink(core);
    auto val = mav ? mav->get_param_by_name(param_name) : std::nullopt;

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("param_name", desc.param_name);
    jb.field("display_name", desc.display_name);
    jb.field("description", desc.description);
    jb.field("group", desc.group);
    jb.field("type", desc.type);
    jb.field("requires_reboot", desc.requires_reboot);
    jb.field("read_only", desc.read_only);
    jb.field("found", desc.found);
    if (!desc.unit.empty()) jb.field("unit", desc.unit);
    if (!desc.category.empty()) jb.field("category", desc.category);
    if (desc.increment != 0) jb.field("increment", desc.increment);
    if (desc.min_value != desc.max_value) {
        jb.field("min", desc.min_value);
        jb.field("max", desc.max_value);
    }
    if (!desc.options.empty()) {
        mg::bridge::JsonBuilder opts;
        opts.begin_array();
        for (const auto& opt : desc.options) {
            mg::bridge::JsonBuilder o;
            o.begin_object();
            o.field("value", opt.value);
            o.field("label", opt.label);
            if (!opt.description.empty()) o.field("description", opt.description);
            o.end_object();
            opts.append_json(o.str());
        }
        opts.end_array();
        jb.raw_field("options", opts.str());
    }
    if (val.has_value()) {
        jb.field("value", val->value);
        jb.field("mavlink_type", static_cast<int>(val->type));
        jb.field("valid", true);
        if (!desc.options.empty()) {
            std::string label = db.enum_label(profile, param_name, static_cast<int>(val->value));
            if (!label.empty()) jb.field("display_value", label);
        }
    } else {
        jb.field("valid", false);
    }
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// mg_config_write_param - Write a single parameter with validation
// ================================================================
MGCORE_API char* mg_config_write_param(mg_core_t* core, const char* param_name, float value) {
    CHECK_CORE(core); if (!get_active_mavlink(core)) return fail(MG_ERROR_NOT_CONNECTED, "no mavlink"); CHECK_CONNECTED(core);
    if (!param_name || param_name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null param name");

    auto& db = get_desc_db();
    auto err = db.validate(ffi_param_profile(core), param_name, value);
    if (!err.empty()) {
        mg::bridge::JsonBuilder jb;
        jb.begin_object();
        jb.field("success", false);
        jb.field("error", err);
        jb.end_object();
        return to_cstr(jb.str());
    }

    auto* mav = get_active_mavlink(core);
    auto cached = mav->get_param_by_name(param_name);
    uint8_t type = cached.has_value() ? cached->type : 9; // MAV_PARAM_TYPE_REAL32

    if (mav->set_param(active_target_sys(core), active_target_comp(core), param_name, value, type)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "set_param failed");
}

// ================================================================
// mg_config_write_params - Write multiple parameters from JSON array
// ================================================================
MGCORE_API char* mg_config_write_params(mg_core_t* core, const char* json) {
    CHECK_CORE(core); if (!get_active_mavlink(core)) return fail(MG_ERROR_NOT_CONNECTED, "no mavlink"); CHECK_CONNECTED(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    cJSON* root = cJSON_Parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "expected JSON array");
    }

    auto* mav = get_active_mavlink(core);
    int written = 0;
    int failed_count = 0;

    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        if (!name || !cJSON_IsString(name) || !value || !cJSON_IsNumber(value)) continue;

        const char* pname = name->valuestring;
        float pvalue = static_cast<float>(value->valuedouble);

        auto cached = mav->get_param_by_name(pname);
        uint8_t type = cached.has_value() ? cached->type : 9;

        if (mav->set_param(active_target_sys(core), active_target_comp(core), pname, pvalue, type)) {
            written++;
        } else {
            failed_count++;
        }
    }

    cJSON_Delete(root);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", failed_count == 0);
    jb.field("written", written);
    jb.field("failed", failed_count);
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// mg_mission_request - Request mission download (alias for download)
// ================================================================
MGCORE_API char* mg_mission_request(mg_core_t* core, const char* json_request) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    (void)json_request;
    auto* mav = get_active_mavlink(core);
    mav->mission_download(active_target_sys(core), active_target_comp(core));
    return ok();
}
