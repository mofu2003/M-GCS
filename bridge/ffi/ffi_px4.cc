#define MGCORE_EXPORTS
/**
 * ffi_px4.cc - PX4-specific APIs
 *
 * PX4 专有配置读取/写入：执行器映射、模式槽、安全断路器、PID 调参。
 *
 * 定位（docs/features/px4-parity.md S5）：这 7 个导出是 PX4 便捷封装，属
 * FFI 内部能力，前端没有独立 UI 计划——通用参数页（mg_params_* /
 * mg_config_*）配合 `px4` 描述符 profile 已能读写同一批飞控参数，并展示
 * 分组/范围/枚举。保留导出是给原生工具与测试一个按语义分组的视图，同时
 * 不再新增平行的大 UI。全部只对 PX4（autopilot_type==12）有效，其余固件
 * 一律返回 NOT_SUPPORTED。
 *
 * 参数名事实来源：PX4-Autopilot 源码快照（e7b1d486e5）与 QGC 的
 * PX4ParameterFactMetaData 快照。历史实现把安全断路器写死成
 * CBRK_AIRSPD_CHK / CBRK_GPS_CHK / CBRK_IO_SAFETY 三个字段，其中
 * CBRK_GPS_CHK 在两个来源里都不存在——永远读 0、set 静默失败；执行器映射
 * 只读前 8 路，而 PX4 主输出函数参数有 16 路。S5 改为按飞控实际参数缓存
 * 报告与校验，避免伪造字段。
 */
#include "ffi_internal.h"
#include "core/telemetry/json_builder.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

/// PX4 主输出函数参数 PWM_MAIN_FUNC1..16（PX4 主输出最多 16 路）。缺失通道
/// 按 PX4 默认值 0 = Disabled 处理。
constexpr int kPx4MainFunctionCount = 16;

/// PX4 模式槽参数 COM_FLTMODE1..6。
constexpr int kPx4FlightModeSlotCount = 6;

/// 参数是否已经由参数缓存读回。缓存里没有时不写，也不把“已发送”当成成功。
bool param_cached(mg_core_t* core, const std::string& name) {
    auto* mav = get_active_mavlink(core);
    return mav != nullptr && mav->get_param_by_name(name.c_str()).has_value();
}

/// PX4 断路器参数名形如 CBRK_IO_SAFETY（CBRK_ 前缀 + 大写字母/数字/下划线）。
bool is_cbrk_name(const std::string& name) {
    constexpr size_t kPrefixLen = 5;  // "CBRK_"
    if (name.size() <= kPrefixLen || name.compare(0, kPrefixLen, "CBRK_") != 0) return false;
    for (size_t i = kPrefixLen; i < name.size(); ++i) {
        const char c = name[i];
        const bool valid = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (!valid) return false;
    }
    return true;
}

}  // namespace

// PX4 执行器配置：读写 PWM_MAIN_FUNC1..16。
MGCORE_API char* mg_px4_actuator_get_config(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::core::telemetry::JsonBuilder funcs; funcs.begin_array();
    for (int i = 1; i <= kPx4MainFunctionCount; i++) {
        auto name = ffi_fmt_param("PWM_MAIN_FUNC%d", i);
        funcs.append_json(std::to_string(static_cast<int>(ffi_get_param(core, name.c_str()))));
    }
    funcs.end_array();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    jb.field("param_prefix", "PWM_MAIN_FUNC");
    jb.field("channel_count", kPx4MainFunctionCount);
    jb.raw_field("main_functions", funcs.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_px4_actuator_set_config(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    if (!json_config || json_config[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty config");
    cJSON* root = mg::core::json::parse(json_config);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    cJSON* funcs = cJSON_GetObjectItemCaseSensitive(root, "main_functions");
    if (!funcs || !cJSON_IsArray(funcs)) { cJSON_Delete(root); return fail(MG_ERROR_INVALID_PARAM, "main_functions array required"); }
    int written = 0;
    int size = cJSON_GetArraySize(funcs);
    for (int i = 0; i < size && i < kPx4MainFunctionCount; i++) {
        cJSON* item = cJSON_GetArrayItem(funcs, i);
        if (!item || !cJSON_IsNumber(item)) continue;
        auto name = ffi_fmt_param("PWM_MAIN_FUNC%d", i + 1);
        if (ffi_set_param(core, name.c_str(), static_cast<float>(item->valueint))) written++;
    }
    cJSON_Delete(root);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("written", written);
    jb.end_object();
    return to_cstr(jb.str());
}

// PX4 模式槽：只读 COM_FLTMODE1..6（见头文件说明，写入走通用参数页）。
MGCORE_API char* mg_px4_flight_mode_get_config(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::core::telemetry::JsonBuilder modes; modes.begin_array();
    for (int i = 1; i <= kPx4FlightModeSlotCount; i++) {
        auto name = ffi_fmt_param("COM_FLTMODE%d", i);
        modes.append_json(std::to_string(static_cast<int>(ffi_get_param(core, name.c_str()))));
    }
    modes.end_array();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    jb.field("param_prefix", "COM_FLTMODE");
    jb.field("slot_count", kPx4FlightModeSlotCount);
    jb.raw_field("flight_modes", modes.str());
    jb.end_object();
    return to_cstr(jb.str());
}

// PX4 安全：按飞控实际参数缓存报告断路器（CBRK_*），字段名就是参数名。
MGCORE_API char* mg_px4_safety_get(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    std::vector<std::string> names;
    if (auto* mav = get_active_mavlink(core)) {
        for (const auto& param : mav->all_params()) {
            if (is_cbrk_name(param.name)) names.push_back(param.name);
        }
    }
    std::sort(names.begin(), names.end());
    mg::core::telemetry::JsonBuilder breakers; breakers.begin_object();
    for (const auto& name : names) {
        breakers.field(name.c_str(), static_cast<int>(ffi_get_param(core, name.c_str())));
    }
    breakers.end_object();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("firmware", "PX4");
    jb.raw_field("circuit_breakers", breakers.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_px4_safety_set(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    if (!json_config || json_config[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty config");
    cJSON* root = mg::core::json::parse(json_config);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    cJSON* breakers = cJSON_GetObjectItemCaseSensitive(root, "circuit_breakers");
    if (!breakers || !cJSON_IsObject(breakers)) {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "circuit_breakers object required");
    }
    // 校验与收集放在写之前：不存在的参数必须报错，不能像旧实现那样静默 0 写。
    std::vector<std::pair<std::string, float>> writes;
    for (cJSON* item = breakers->child; item != nullptr; item = item->next) {
        const std::string name = item->string ? item->string : "";
        if (!is_cbrk_name(name) || !cJSON_IsNumber(item)) {
            cJSON_Delete(root);
            return fail(MG_ERROR_INVALID_PARAM, ("not a PX4 circuit breaker value: " + name).c_str());
        }
        if (!param_cached(core, name)) {
            cJSON_Delete(root);
            return fail(MG_ERROR_NOT_FOUND, ("PX4 vehicle has no parameter " + name).c_str());
        }
        writes.emplace_back(name, static_cast<float>(item->valuedouble));
    }
    cJSON_Delete(root);
    int written = 0;
    for (const auto& [name, value] : writes) {
        if (ffi_set_param(core, name.c_str(), value)) written++;
    }
    if (written != static_cast<int>(writes.size())) {
        return fail(MG_ERROR_SEND_FAILED, "circuit breaker parameter write could not be sent");
    }
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("written", written);
    jb.end_object();
    return to_cstr(jb.str());
}

// PX4 调参：读写 PID 参数（MC_ROLL_P 等，均存在于 PX4 参数集）。
MGCORE_API char* mg_px4_tuning_get(mg_core_t* core) {
    CHECK_CORE(core);
    if (!ffi_is_px4(core)) return fail(MG_ERROR_NOT_SUPPORTED, "not a PX4 vehicle");
    mg::core::telemetry::JsonBuilder jb;
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
    cJSON* root = mg::core::json::parse(json_config);
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
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("written", written);
    jb.end_object();
    return to_cstr(jb.str());
}
