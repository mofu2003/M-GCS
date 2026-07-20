#define MGCORE_EXPORTS
/**
 * ffi_px4.cc - PX4-specific APIs
 *
 * PX4 专有功能：执行器配置、飞行模式、安全设置、调参。
 *
 * Uses PX4 parameter protocol to read/write PX4-specific settings.
 * Falls back to NOT_SUPPORTED for features requiring deep PX4 integration.
 */
#include "ffi_internal.h"
#include "json_builder.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

// PX4 actuator config: reads PWM_MAIN_FUNC1..16 parameters
MGCORE_API char* mg_px4_actuator_get_config(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    mg::bridge::JsonBuilder funcs; funcs.begin_array();
    for (int i = 1; i <= 8; i++) {
        auto name = ffi_fmt_param("PWM_MAIN_FUNC%d", i);
        int val = static_cast<int>(ffi_get_param(core, name.c_str()));
        funcs.append_json(std::to_string(val));
    }
    funcs.end_array();
    jb.raw_field("main_functions", funcs.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_px4_actuator_set_config(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    if (!json_config || json_config[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty config");
    cJSON* root = cJSON_Parse(json_config);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    cJSON* funcs = cJSON_GetObjectItemCaseSensitive(root, "main_functions");
    if (!funcs || !cJSON_IsArray(funcs)) { cJSON_Delete(root); return fail(MG_ERROR_INVALID_PARAM, "main_functions array required"); }
    int written = 0;
    int size = cJSON_GetArraySize(funcs);
    for (int i = 0; i < size && i < 8; i++) {
        cJSON* item = cJSON_GetArrayItem(funcs, i);
        if (!item || !cJSON_IsNumber(item)) continue;
        auto name = ffi_fmt_param("PWM_MAIN_FUNC%d", i + 1);
        if (ffi_set_param(core, name.c_str(), static_cast<float>(item->valueint))) written++;
    }
    cJSON_Delete(root);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("written", written);
    jb.end_object();
    return to_cstr(jb.str());
}

// PX4 flight mode config: reads COM_FLTMODE1..6 parameters
MGCORE_API char* mg_px4_flight_mode_get_config(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    mg::bridge::JsonBuilder modes; modes.begin_array();
    for (int i = 1; i <= 6; i++) {
        auto name = ffi_fmt_param("COM_FLTMODE%d", i);
        int val = static_cast<int>(ffi_get_param(core, name.c_str()));
        modes.append_json(std::to_string(val));
    }
    modes.end_array();
    jb.raw_field("flight_modes", modes.str());
    jb.end_object();
    return to_cstr(jb.str());
}

// PX4 safety: reads CBRK_* parameters (circuit breakers)
MGCORE_API char* mg_px4_safety_get(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    jb.field("airspeed_sensor_cbrk", static_cast<int>(ffi_get_param(core, "CBRK_AIRSPD_CHK")));
    jb.field("gps_cbrk", static_cast<int>(ffi_get_param(core, "CBRK_GPS_CHK")));
    jb.field("safety_cbrk", static_cast<int>(ffi_get_param(core, "CBRK_IO_SAFETY")));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_px4_safety_set(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    if (!json_config || json_config[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty config");
    cJSON* root = cJSON_Parse(json_config);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    int written = 0;
    cJSON* aspd = cJSON_GetObjectItemCaseSensitive(root, "airspeed_sensor_cbrk");
    if (aspd && cJSON_IsNumber(aspd)) { if (ffi_set_param(core, "CBRK_AIRSPD_CHK", (float)aspd->valuedouble)) written++; }
    cJSON* gps = cJSON_GetObjectItemCaseSensitive(root, "gps_cbrk");
    if (gps && cJSON_IsNumber(gps)) { if (ffi_set_param(core, "CBRK_GPS_CHK", (float)gps->valuedouble)) written++; }
    cJSON* safety = cJSON_GetObjectItemCaseSensitive(root, "safety_cbrk");
    if (safety && cJSON_IsNumber(safety)) { if (ffi_set_param(core, "CBRK_IO_SAFETY", (float)safety->valuedouble)) written++; }
    cJSON_Delete(root);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("written", written);
    jb.end_object();
    return to_cstr(jb.str());
}

// PX4 tuning: reads PID parameters
MGCORE_API char* mg_px4_tuning_get(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    jb.field("mc_roll_p", ffi_get_param(core, "MC_ROLL_P"));
    jb.field("mc_roll_rate_p", ffi_get_param(core, "MC_ROLLRATE_P"));
    jb.field("mc_pitch_p", ffi_get_param(core, "MC_PITCH_P"));
    jb.field("mc_pitch_rate_p", ffi_get_param(core, "MC_PITCHRATE_P"));
    jb.field("mc_yaw_p", ffi_get_param(core, "MC_YAW_P"));
    jb.field("mc_yaw_rate_p", ffi_get_param(core, "MC_YAWRATE_P"));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_px4_tuning_set(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    if (!json_config || json_config[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty config");
    cJSON* root = cJSON_Parse(json_config);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    int written = 0;
    auto try_set = [&](const char* json_key, const char* param_name) {
        cJSON* v = cJSON_GetObjectItemCaseSensitive(root, json_key);
        if (v && cJSON_IsNumber(v)) {
            if (ffi_set_param(core, param_name, (float)v->valuedouble)) written++;
        }
    };
    try_set("mc_roll_p", "MC_ROLL_P");
    try_set("mc_roll_rate_p", "MC_ROLLRATE_P");
    try_set("mc_pitch_p", "MC_PITCH_P");
    try_set("mc_pitch_rate_p", "MC_PITCHRATE_P");
    try_set("mc_yaw_p", "MC_YAW_P");
    try_set("mc_yaw_rate_p", "MC_YAWRATE_P");
    cJSON_Delete(root);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("written", written);
    jb.end_object();
    return to_cstr(jb.str());
}
