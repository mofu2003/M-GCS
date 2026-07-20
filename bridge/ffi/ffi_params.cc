#define MGCORE_EXPORTS
/**
 * ffi_params.cc - Section 3: Parameters
 *
 * 参数读写：请求全部参数、按名查询/设置、进度查询。
 *
 * mg_params_request_all, mg_params_get_all, mg_params_get_progress,
 * mg_params_get_by_name, mg_params_set
 */
#include "ffi_internal.h"
#include "param_json_serializers.h"
#include "telemetry_bindings.h"

MGCORE_API char* mg_params_request_all(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    if (mav->request_param_list(active_target_sys(core), active_target_comp(core))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_params_get_all(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("[]");
    auto params = mav->all_params();
    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& p : params) {
        jb.append_json(mg::bridge::param_json(p));
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_params_get_progress(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr(R"({"received":0,"total":0})");
    return to_cstr(mg::bridge::param_progress_json(
        static_cast<uint16_t>(mav->params_received()), mav->param_count()));
}

MGCORE_API char* mg_params_get_by_name(mg_core_t* core, const char* param_name) {
    CHECK_CORE(core);
    if (!param_name || param_name[0] == '\0') return to_cstr("{\"found\":false}");
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{\"found\":false}");
    auto p = mav->get_param_by_name(param_name);
    if (!p.has_value()) return to_cstr(mg::bridge::param_not_found_json());
    return to_cstr(mg::bridge::param_detail_json(p.value()));
}

MGCORE_API char* mg_params_set(mg_core_t* core, const char* param_name, float param_value, int param_type) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!param_name || param_name[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty param name");

    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");

    uint16_t total = mav->param_count();
    size_t received = mav->params_received();
    if (total == 0 || received < static_cast<size_t>(total)) {
        return fail(MG_ERROR_INVALID_STATE, "params not loaded; call mg_params_request_all() first and wait for completion");
    }

    uint8_t mav_type = static_cast<uint8_t>(param_type);
    if (mav_type == 0) {
        auto p = mav->get_param_by_name(param_name);
        if (!p.has_value()) return fail(MG_ERROR_NOT_FOUND, "parameter not found in cache");
        mav_type = p->type;
    }

    if (mav->set_param(active_target_sys(core), active_target_comp(core), param_name, param_value, mav_type)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

// Aliases matching ffi_api.h declarations
MGCORE_API char* mg_param_list(mg_core_t* core) { return mg_params_request_all(core); }
MGCORE_API char* mg_param_get(mg_core_t* core, const char* name) { return mg_params_get_by_name(core, name); }
MGCORE_API char* mg_param_set(mg_core_t* core, const char* name, float value) { return mg_params_set(core, name, value, 0); }

// ================================================================
// Parameter diff — return all params (frontend compares against defaults)
// ================================================================

MGCORE_API char* mg_params_get_changed(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");

    auto all_params = mav->all_params();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.begin_array();;
    // Since we don't have default values stored, return all params
    // The frontend can compare against ParameterDescriptors.json defaults
    for (const auto& p : all_params) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", p.name);
        item.field("value", static_cast<double>(p.value));
        item.field("type", static_cast<int>(p.type));
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    jb.field("count", static_cast<int>(all_params.size()));
    jb.end_object();
    return to_cstr(jb.str());
}
