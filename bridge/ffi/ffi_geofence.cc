#define MGCORE_EXPORTS
/**
 * ffi_geofence.cc - 飞控围栏查询 FFI
 *
 * 只保留两个查询入口：本地围栏配置读回与点包含判断。
 * 围栏/Rally 的编辑、编译、上传与清除走任务文档路径
 * （mg_mission_doc_*_fence / mg_mission_doc_*_rally），不再有第二套直接编辑接口。
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"

static bool geofence_param(mg_core_t* core, const char* name, float* value) {
    auto mav = shared_active_mavlink(core);
    if (!mav || !name || !value) return false;
    auto p = mav->get_param_by_name(name);
    if (!p.has_value()) return false;
    *value = p->value;
    return true;
}

MGCORE_API char* mg_geofence_get_config(mg_core_t* core) {
    CHECK_CORE(core);

    auto config = get_active_geofence(core).get_config();
    float param = 0.0f;
    if (geofence_param(core, "FENCE_ENABLE", &param)) config.enabled = param != 0.0f;
    if (geofence_param(core, "FENCE_ALT_MAX", &param)) config.max_altitude = param;
    if (geofence_param(core, "FENCE_ALT_MIN", &param)) config.min_altitude = param;
    if (geofence_param(core, "FENCE_ACTION", &param)) config.action = static_cast<uint8_t>(param);
    if (geofence_param(core, "FENCE_MARGIN", &param)) config.breach_distance = param;

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("enabled", config.enabled);
    jb.field("max_altitude", config.max_altitude);
    jb.field("min_altitude", config.min_altitude);
    jb.field("action", (int)config.action);
    jb.field("return_at_min", config.return_at_min);
    jb.field("breach_distance", config.breach_distance);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_geofence_check(mg_core_t* core, double lat, double lon) {
    CHECK_CORE(core);
    auto& fence = get_active_geofence(core);
    bool inside = fence.is_inside_fence(lat, lon);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("inside_fence", inside);
    jb.field("lat", lat);
    jb.field("lon", lon);
    jb.end_object();
    return to_cstr(jb.str());
}
