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
#include "json_serializers.h"





static const char* func_names[] = {"roll", "pitch", "yaw", "throttle", "mode", "rtilt"};
static const char* ap_params[]   = {"RCMAP_ROLL", "RCMAP_PITCH", "RCMAP_YAW", "RCMAP_THROTTLE", "RCMAP_MODE", "RCMAP_RTLTILT"};
static const char* px4_params[]  = {"RC_MAP_ROLL", "RC_MAP_PITCH", "RC_MAP_YAW", "RC_MAP_THROTTLE", "RC_MAP_MODE", NULL};

MGCORE_API char* mg_rc_get_mapping(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{}");

    bool px4 = ffi_is_px4(core);

    mg::bridge::JsonBuilder jb;
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
    mg::bridge::JsonBuilder trims;
    trims.begin_array();
    for (int ch = 1; ch <= 18; ch++) {
        char name[32];
        if (px4) snprintf(name, sizeof(name), "RC%d_TRIM", ch);
        else snprintf(name, sizeof(name), "CH%d_TRIM", ch);
        float trim = ffi_get_param(core, name);
        if (trim == 0) continue;

        mg::bridge::JsonBuilder t;
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

    mg::bridge::JsonBuilder jb;
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

        mg::bridge::JsonBuilder obj;
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

MGCORE_API char* mg_rc_calibrate_start(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    // MAV_CMD_DO_RC_CALIBRATION (211)
    bool sent = mav->send_command_long(
        active_target_sys(core), active_target_comp(core),
        211, 0,
        0, 0, 0, 0, 0, 0, 0);
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "rc calibrate send failed");
}

MGCORE_API char* mg_rc_calibrate_status(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr(R"({"active":false})");

    auto ack = mav->latest_command_ack();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    if (ack.has_value() && ack->command == 211) {
        jb.field("active", ack->result == 1);  // MAV_RESULT_IN_PROGRESS
        jb.field("result", (int)ack->result);
        jb.field("progress", (int)ack->progress);
    } else {
        jb.field("active", false);
        jb.field("result", -1);
        jb.field("progress", 0);
    }
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_rc_calibrate_save(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    // ArduPilot 与 PX4 都是在 RC 校准结束时由飞控自己写入 RCn_MIN/TRIM/MAX，
    // 没有独立的“保存”命令。以前这里无条件返回 success，等于掩盖了
    // “校准还没结束”的情况，现在显式区分。
    const auto ack = get_active_mavlink(core)->latest_command_ack();
    if (ack.has_value() && ack->command == 211 /* MAV_CMD_DO_RC_CALIBRATION */ &&
        ack->result == 1 /* MAV_RESULT_IN_PROGRESS */) {
        return fail(MG_ERROR_INVALID_STATE,
                    "RC calibration is still in progress; the flight controller "
                    "saves the ranges when it finishes");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("saved_by_flight_controller", true);
    jb.field("message",
             "RC calibration ranges are persisted by the flight controller; "
             "there is no separate save command");
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_rc_get_failsafe(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return to_cstr("{}");

    bool px4 = ffi_is_px4(core);

    mg::bridge::JsonBuilder jb;
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

    if (px4) {
        ffi_set_param(core, "COM_RC_FAIL_MODE", (float)mode);
    } else {
        ffi_set_param(core, "FS_THR_ENABLE", (float)mode);
        ffi_set_param(core, "FS_THR_VALUE", (float)threshold);
    }
    return ok();
}
