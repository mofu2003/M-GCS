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
#include "core/telemetry/json_serializers.h"
#include "core/config/config_descriptors.h"

#include <algorithm>
#include <set>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

// get_desc_db() shared via ffi_internal.h

namespace {

constexpr const char* kOtherParameterGroup = "other";

const char* infer_param_type(const mg::core::mavlink::ParamInfo& param) {
    switch (param.type) {
        case MAV_PARAM_TYPE_REAL32:
        case MAV_PARAM_TYPE_REAL64:
            return "float";
        case MAV_PARAM_TYPE_UINT8:
        case MAV_PARAM_TYPE_INT8:
        case MAV_PARAM_TYPE_UINT16:
        case MAV_PARAM_TYPE_INT16:
        case MAV_PARAM_TYPE_UINT32:
        case MAV_PARAM_TYPE_INT32:
        case MAV_PARAM_TYPE_UINT64:
        case MAV_PARAM_TYPE_INT64:
            return "int";
        default:
            return "float";
    }
}

mg::core::config::GroupDescriptor other_parameter_group(
    mg_core_t* core,
    const mg::core::config::ParamDescriptors& db,
    const std::string& profile) {
    mg::core::config::GroupDescriptor group;
    group.name = kOtherParameterGroup;
    group.display_name = "Other";
    group.description = "Loaded parameters without a local descriptor";

    auto mav = shared_active_mavlink(core);
    if (!mav) return group;

    std::set<std::string> grouped;
    for (const auto& group_name : db.get_group_names(profile)) {
        const auto descriptor = db.get_group(profile, group_name);
        grouped.insert(descriptor.param_names.begin(), descriptor.param_names.end());
    }
    for (const auto& param : mav->all_params()) {
        if (grouped.count(param.name) != 0) continue;
        group.param_names.push_back(param.name);
    }
    std::sort(group.param_names.begin(), group.param_names.end());
    return group;
}

} // namespace

// ================================================================
// Helper: serialize a ParamDescriptor to JSON
// ================================================================
static std::string param_desc_json(const mg::core::config::ParamDescriptor& desc) {
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

    if (!desc.options.empty()) {
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
    auto mav = shared_active_mavlink(core);
    if (!mav) return "{}";

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("name", group.name);
    jb.field("display_name", group.display_name);
    jb.field("description", group.description);

    mg::core::telemetry::JsonBuilder params;
    params.begin_array();
    for (const auto& pname : group.param_names) {
        auto desc = db.get(profile, pname);
        auto val = mav->get_param_by_name(pname.c_str());

        mg::core::telemetry::JsonBuilder p;
        p.begin_object();
        p.field("param_name", pname);
        p.field("display_name", desc.display_name);
        if (val.has_value()) {
            p.field("value", val->value);
            p.field("type", static_cast<int>(val->type));
            p.field("valid", true);
            if (!desc.options.empty()) {
                const auto value = static_cast<int>(val->value);
                for (const auto& option : desc.options) {
                    if (option.value != value) continue;
                    if (!option.label.empty()) p.field("display_value", option.label);
                    if (!option.english_label.empty()) {
                        p.field("display_value_en", option.english_label);
                    }
                    break;
                }
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




/// 语义角色 → 固件参数名（S6）。
///
/// 角色名由前端定义、与固件和机型无关（如 `rc.map.roll`、`pid.rate.roll.p`），
/// 参数名只在这里由插件提供。值里的 `{n}` 是序号占位符，由调用方替换。
/// 返回 `{"profile":"plane","roles":{"rc.map.roll":"RCMAP_ROLL",...}}`；
/// 没有角色表的固件（如未登记角色的插件）返回空 `roles`，不是错误。
MGCORE_API char* mg_config_get_param_roles(mg_core_t* core) {
    CHECK_CORE(core);

    auto& db = get_desc_db();
    const auto profile = ffi_param_profile(core);
    const auto table = db.roles(profile);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("profile", profile);
    mg::core::telemetry::JsonBuilder roles;
    roles.begin_object();
    for (const auto& entry : table) roles.field(entry.first.c_str(), entry.second);
    roles.end_object();
    jb.raw_field("roles", roles.str());
    jb.end_object();
    return to_cstr(jb.str());
}
