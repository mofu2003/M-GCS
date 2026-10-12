#define MGCORE_EXPORTS
/**
 * ffi_rc_config.cc - RC configuration and calibration
 *
 * RC 通道映射、校准数据、失联保护设置，AP/PX4 参数自动适配。
 *
 * AP:  RCMAP_ROLL/PITCH/YAW/THROTTLE, CH{n}_MIN/MAX/REV/TRIM
 * PX4: RC_MAP_ROLL/PITCH/YAW/THROTTLE, RC{n}_MIN/MAX/REV/TRIM
 *
 * RC Calibration uses MAV_CMD_DO_RC_CALIBRATION (211) for both AP and PX4.
 * The FC responds with STATUSTEXT messages describing the calibration process.
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"





static const char* func_names[] = {"roll", "pitch", "yaw", "throttle", "mode", "rtilt"};
static const char* ap_params[]   = {"RCMAP_ROLL", "RCMAP_PITCH", "RCMAP_YAW", "RCMAP_THROTTLE", "RCMAP_MODE", "RCMAP_RTLTILT"};
static const char* px4_params[]  = {"RC_MAP_ROLL", "RC_MAP_PITCH", "RC_MAP_YAW", "RC_MAP_THROTTLE", "RC_MAP_MODE", NULL};

MGCORE_API char* mg_rc_get_mapping(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{}");

    bool px4 = ffi_is_px4(core);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();

    for (int i = 0; i < 6; i++) {
        if (!func_names[i]) break;
        const char* param = px4 ? (px4_params[i] ? px4_params[i] : ap_params[i]) : ap_params[i];
        if (!param) continue;
        float val = ffi_get_param(core, param);
        if (val != 0) {
            jb.field(func_names[i], (int)val);
        }
    }

    // RC trim values (CH1_TRIM..CH18_TRIM)
    mg::core::telemetry::JsonBuilder trims;
    trims.begin_array();
    for (int ch = 1; ch <= 18; ch++) {
        char name[32];
        if (px4) snprintf(name, sizeof(name), "RC%d_TRIM", ch);
        else snprintf(name, sizeof(name), "CH%d_TRIM", ch);
        float trim = ffi_get_param(core, name);
        if (trim == 0) continue;

        mg::core::telemetry::JsonBuilder t;
        t.begin_object();
        t.field("channel", ch);
        t.field("trim", (int)trim);
        t.end_object();
        trims.append_json(t.str());
    }
    trims.end_array();
    jb.raw_field("trims", trims.str());

    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_rc_set_mapping(mg_core_t* core, const char* function, int channel) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (!function || function[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null function");
    if (channel < 1 || channel > 18) return fail(MG_ERROR_INVALID_PARAM, "channel must be 1-18");

    auto* mav = get_active_mavlink(core);
    bool px4 = ffi_is_px4(core);

    const char* param = NULL;
    for (int i = 0; i < 6; i++) {
        if (func_names[i] && strcmp(func_names[i], function) == 0) {
            param = px4 ? (px4_params[i] ? px4_params[i] : ap_params[i]) : ap_params[i];
            break;
        }
    }
    if (!param) return fail(MG_ERROR_NOT_FOUND, "unknown function");

    auto p = mav->get_param_by_name(param);
    if (!p.has_value()) return fail(MG_ERROR_NOT_FOUND, "mapping param not found");

    if (mav->set_param(active_target_sys(core), active_target_comp(core),
                       param, (float)channel, p->type)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "set mapping failed");
}

MGCORE_API char* mg_rc_get_calibration(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("[]");

    bool px4 = ffi_is_px4(core);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();

    for (int ch = 1; ch <= 18; ch++) {
        char min_name[32], max_name[32], rev_name[32], trim_name[32];
        if (px4) {
            snprintf(min_name, sizeof(min_name), "RC%d_MIN", ch);
            snprintf(max_name, sizeof(max_name), "RC%d_MAX", ch);
            snprintf(rev_name, sizeof(rev_name), "RC%d_REV", ch);
            snprintf(trim_name, sizeof(trim_name), "RC%d_TRIM", ch);
        } else {
            snprintf(min_name, sizeof(min_name), "CH%d_MIN", ch);
            snprintf(max_name, sizeof(max_name), "CH%d_MAX", ch);
            snprintf(rev_name, sizeof(rev_name), "CH%d_REV", ch);
            snprintf(trim_name, sizeof(trim_name), "CH%d_TRIM", ch);
        }

        float min_val = ffi_get_param(core, min_name);
        float max_val = ffi_get_param(core, max_name);
        if (min_val == 0 && max_val == 0) continue;

        mg::core::telemetry::JsonBuilder obj;
        obj.begin_object();
        obj.field("channel", ch);
        obj.field("min", (int)min_val);
        obj.field("max", (int)max_val);
        obj.field("trim", (int)ffi_get_param(core, trim_name));
        obj.field("reversed", ffi_get_param(core, rev_name) != 0);
        obj.end_object();
        jb.append_json(obj.str());
    }

    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_rc_get_failsafe(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{}");

    bool px4 = ffi_is_px4(core);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();

    if (px4) {
        jb.field("mode", (int)ffi_get_param(core, "COM_RC_FAIL_MODE"));
        jb.field("loss_timeout", ffi_get_param(core, "COM_RC_LOSS_T"));
    } else {
        jb.field("mode", (int)ffi_get_param(core, "FS_THR_ENABLE"));
        jb.field("threshold", (int)ffi_get_param(core, "FS_THR_VALUE"));
    }

    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_rc_set_failsafe(mg_core_t* core, int mode, int threshold) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    bool px4 = ffi_is_px4(core);

    // 写入失败必须如实返回（S1：拒绝/失败要传播，不得吞掉）。逐项求值，
    // 不用 && 短路，否则前一项失败会跳过后续写入并留下半写状态。
    bool all_ok = true;
    if (px4) {
        all_ok = ffi_set_param(core, "COM_RC_FAIL_MODE", (float)mode) && all_ok;
    } else {
        all_ok = ffi_set_param(core, "FS_THR_ENABLE", (float)mode) && all_ok;
        all_ok = ffi_set_param(core, "FS_THR_VALUE", (float)threshold) && all_ok;
    }
    return all_ok ? ok() : fail(MG_ERROR_SEND_FAILED, "RC failsafe parameter write failed");
}
