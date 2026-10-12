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

#include <algorithm>
#include <set>

#include "cJSON.h"

// get_desc_db() shared via ffi_internal.h

namespace {

} // namespace

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
