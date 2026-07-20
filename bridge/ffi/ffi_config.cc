#define MGCORE_EXPORTS
/**
 * ffi_config.cc - Parameter descriptor FFI
 *
 * 配置描述符：参数分组、描述信息、预设管理。
 *
 * mg_config_get_groups, mg_config_get_group_descriptor,
 * mg_config_read_group, mg_config_validate_param,
 * mg_config_get_presets, mg_config_apply_preset
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include "core/config/config_descriptors.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

// get_desc_db() shared via ffi_internal.h

// ================================================================
// Helper: serialize a ParamDescriptor to JSON
// ================================================================
static std::string param_desc_json(const mg::core::config::ParamDescriptor& desc) {
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
    } else if (desc.min_value != desc.max_value) {
        jb.field("min", desc.min_value);
        jb.field("max", desc.max_value);
    }

    if (desc.default_value != 0) jb.field("default", desc.default_value);

    jb.end_object();
    return jb.str();
}

// ================================================================
// Helper: serialize a group with values to JSON
// ================================================================
static std::string config_read_group_json(mg_core_t* core,
                                           const mg::core::config::GroupDescriptor& group) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return "{}";

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("name", group.name);
    jb.field("display_name", group.display_name);
    jb.field("description", group.description);

    mg::bridge::JsonBuilder params;
    params.begin_array();
    for (const auto& pname : group.param_names) {
        auto desc = db.get(profile, pname);
        auto val = mav->get_param_by_name(pname.c_str());

        mg::bridge::JsonBuilder p;
        p.begin_object();
        p.field("param_name", pname);
        p.field("display_name", desc.display_name);
        if (val.has_value()) {
            p.field("value", val->value);
            p.field("type", static_cast<int>(val->type));
            p.field("valid", true);
            if (!desc.options.empty()) {
                std::string label = db.enum_label(profile, pname, static_cast<int>(val->value));
                if (!label.empty()) p.field("display_value", label);
            }
        } else {
            p.field("valid", false);
        }
        p.end_object();
        params.append_json(p.str());
    }
    params.end_array();
    jb.raw_field("parameters", params.str());

    jb.end_object();
    return jb.str();
}

// ================================================================
// FFI Functions
// ================================================================

MGCORE_API char* mg_config_get_groups(mg_core_t* core) {
    CHECK_CORE(core);

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);
    auto names = db.get_group_names(profile);

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& n : names) {
        auto g = db.get_group(profile, n);
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", g.name);
        item.field("display_name", g.display_name);
        item.field("description", g.description);
        item.field("param_count", static_cast<int>(g.param_names.size()));
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_config_get_group_descriptor(mg_core_t* core, const char* group_name) {
    CHECK_CORE(core);
    if (!group_name || group_name[0] == '\0') return to_cstr("{}");

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);
    auto group = db.get_group(profile, group_name);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("name", group.name);
    jb.field("display_name", group.display_name);
    jb.field("description", group.description);

    mg::bridge::JsonBuilder params;
    params.begin_array();
    for (const auto& pname : group.param_names) {
        auto desc = db.get(profile, pname);
        params.append_json(param_desc_json(desc));
    }
    params.end_array();
    jb.raw_field("parameters", params.str());

    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_config_read_group(mg_core_t* core, const char* group_name) {
    CHECK_CORE(core);
    CHECK_CORE(core); if (!get_active_mavlink(core)) return fail(MG_ERROR_NOT_CONNECTED, "no mavlink");
    if (!group_name || group_name[0] == '\0') return to_cstr("{}");

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);
    auto group = db.get_group(profile, group_name);
    if (group.param_names.empty()) return to_cstr("{}");

    return to_cstr(config_read_group_json(core, group));
}

MGCORE_API char* mg_config_validate_param(mg_core_t* core, const char* param_name, float value) {
    CHECK_CORE(core);
    if (!param_name || param_name[0] == '\0') {
        return to_cstr(R"({"valid":false,"error":"null param name"})");
    }

    auto& db = get_desc_db();
    auto err = db.validate(ffi_param_profile(core), param_name, value);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid", err.empty());
    if (!err.empty()) jb.field("error", err);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_config_get_presets(mg_core_t* core) {
    CHECK_CORE(core);

    auto presets = mg::core::config::get_all_presets();

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& p : presets) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", p.name);
        item.field("description", p.description);
        item.field("param_count", static_cast<int>(p.params.size()));
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_config_apply_preset(mg_core_t* core, const char* preset_name) {
    CHECK_CORE(core);
    CHECK_CORE(core); if (!get_active_mavlink(core)) return fail(MG_ERROR_NOT_CONNECTED, "no mavlink");
    CHECK_CONNECTED(core);
    if (!preset_name || preset_name[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "null preset name");
    }

    auto presets = mg::core::config::get_all_presets();
    for (const auto& p : presets) {
        if (p.name == preset_name) {
            auto* mav = get_active_mavlink(core);
            uint8_t sys = active_target_sys(core);
            uint8_t comp = active_target_comp(core);

            int written = 0;
            int failed = 0;
            for (const auto& [param, value] : p.params) {
                auto cached = mav->get_param_by_name(param.c_str());
                uint8_t type = cached.has_value() ? cached->type : 9; // REAL32 default
                if (mav->set_param(sys, comp, param.c_str(), value, type)) {
                    written++;
                } else {
                    failed++;
                }
            }

            mg::bridge::JsonBuilder jb;
            jb.begin_object();
            jb.field("success", failed == 0);
            jb.field("written", written);
            jb.field("failed", failed);
            jb.end_object();
            return to_cstr(jb.str());
        }
    }

    return fail(MG_ERROR_INVALID_PARAM, "unknown preset");
}
