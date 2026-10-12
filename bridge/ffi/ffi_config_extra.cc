#define MGCORE_EXPORTS
/**
 * ffi_config_extra.cc - Additional config FFI functions
 *
 * 配置补充：单参数读写（带元数据）、批量写入、任务请求。
 *
 * Implements mg_config_read_param, mg_config_write_param, mg_config_write_params,
 * along with configuration extensions.
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"
#include <cstring>
// get_desc_db() shared via ffi_internal.h

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"


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

    mg::core::telemetry::JsonBuilder jb;
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
    // FLTMODE1..6 / COM_FLTMODE1..6 没有随描述库发布取值名称，但飞行模式表里的
    // custom_mode ↔ 名称是现成的。这里按**当前机型**注入，前端参数控件就会把它渲染
    // 成模式名下拉，而不是让用户填数字（对标 MP 从参数元数据取模式名）。
    const bool is_mode_slot =
        (param_name[0] == 'F' && std::strncmp(param_name, "FLTMODE", 7) == 0 &&
         param_name[7] >= '1' && param_name[7] <= '6' && param_name[8] == '\0') ||
        (std::strncmp(param_name, "COM_FLTMODE", 11) == 0 &&
         param_name[11] >= '1' && param_name[11] <= '6' && param_name[12] == '\0');
    if (is_mode_slot) {
        mg::core::telemetry::JsonBuilder opts;
        opts.begin_array();
        for (const auto& entry : get_active_flight_modes(core).mode_entries()) {
            mg::core::telemetry::JsonBuilder o;
            o.begin_object();
            o.field("value", static_cast<int>(entry.custom_mode));
            o.field("label", entry.name);
            o.end_object();
            opts.append_json(o.str());
        }
        opts.end_array();
        jb.raw_field("options", opts.str());
        jb.field("options_from_flight_modes", true);
    }

    if (!is_mode_slot && !desc.options.empty()) {
        mg::core::telemetry::JsonBuilder opts;
        opts.begin_array();
        for (const auto& opt : desc.options) {
            mg::core::telemetry::JsonBuilder o;
            o.begin_object();
            o.field("value", opt.value);
            o.field("label", opt.label);
            if (!opt.english_label.empty()) o.field("english_label", opt.english_label);
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
            const auto value = static_cast<int>(val->value);
            for (const auto& option : desc.options) {
                if (option.value != value) continue;
                if (!option.label.empty()) jb.field("display_value", option.label);
                if (!option.english_label.empty()) {
                    jb.field("display_value_en", option.english_label);
                }
                break;
            }
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
        mg::core::telemetry::JsonBuilder jb;
        jb.begin_object();
        jb.field("success", false);
        write_error(jb, MG_ERROR_INVALID_PARAM, err);
        jb.end_object();
        return to_cstr(jb.str());
    }

    auto* mav = get_active_mavlink(core);
    const std::string vehicle_name = ffi_param_vehicle_name(core, param_name);
    auto cached = mav->get_param_by_name(vehicle_name.c_str());
    if (!cached.has_value()) cached = mav->get_param_by_name(param_name);
    uint8_t type = cached.has_value() ? cached->type : 9; // MAV_PARAM_TYPE_REAL32

    if (mav->set_param(active_target_sys(core), active_target_comp(core),
                       vehicle_name.c_str(), value, type)) {
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

    cJSON* root = mg::core::json::parse(json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "expected JSON array");
    }

    auto* mav = get_active_mavlink(core);
    int written = 0;
    int failed_count = 0;
    auto& descriptor_db = get_desc_db();
    const auto profile = ffi_param_profile(core);
    mg::core::telemetry::JsonBuilder errors;
    errors.begin_array();

    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON* name = cJSON_GetObjectItemCaseSensitive(item, "name");
        cJSON* value = cJSON_GetObjectItemCaseSensitive(item, "value");
        if (!name || !cJSON_IsString(name) || !value || !cJSON_IsNumber(value)) continue;

        const char* pname = name->valuestring;
        float pvalue = static_cast<float>(value->valuedouble);

        const auto validation_error = descriptor_db.validate(profile, pname, pvalue);
        if (!validation_error.empty()) {
            mg::core::telemetry::JsonBuilder error;
            error.begin_object();
            error.field("name", pname);
            error.field("reason", validation_error);
            error.end_object();
            errors.append_json(error.str());
            failed_count++;
            continue;
        }

        const std::string vehicle_name = ffi_param_vehicle_name(core, pname);
        auto cached = mav->get_param_by_name(vehicle_name.c_str());
        if (!cached.has_value()) cached = mav->get_param_by_name(pname);
        uint8_t type = cached.has_value() ? cached->type : 9;

        if (mav->set_param(active_target_sys(core), active_target_comp(core),
                           vehicle_name.c_str(), pvalue, type)) {
            written++;
        } else {
            failed_count++;
        }
    }

    cJSON_Delete(root);
    errors.end_array();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", failed_count == 0);
    if (failed_count != 0) {
        write_error(jb, MG_ERROR_REJECTED,
                    std::to_string(failed_count) + " parameter writes failed");
    }
    jb.field("written", written);
    jb.field("failed", failed_count);
    jb.raw_field("errors", errors.str());
    jb.end_object();
    return to_cstr(jb.str());
}
