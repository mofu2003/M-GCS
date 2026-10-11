#define MGCORE_EXPORTS
/**
 * ffi_geofence.cc - Geofence management FFI
 *
 * 地理围栏：配置、顶点管理、导入导出、验证。
 *
 * Routes through active vehicle's GeofenceManager via get_active_geofence().
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

static bool geofence_param(mg_core_t* core, const char* name, float* value) {
    auto* mav = get_active_mavlink(core);
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

MGCORE_API char* mg_geofence_set_config(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    cJSON* root = mg::core::json::parse(json);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");

    mg::core::geofence::FenceConfig config;

    cJSON* enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    cJSON* max_alt = cJSON_GetObjectItemCaseSensitive(root, "max_altitude");
    cJSON* min_alt = cJSON_GetObjectItemCaseSensitive(root, "min_altitude");
    cJSON* action = cJSON_GetObjectItemCaseSensitive(root, "action");
    cJSON* return_min = cJSON_GetObjectItemCaseSensitive(root, "return_at_min");
    cJSON* breach = cJSON_GetObjectItemCaseSensitive(root, "breach_distance");

    if (enabled && cJSON_IsBool(enabled)) config.enabled = cJSON_IsTrue(enabled);
    if (max_alt && cJSON_IsNumber(max_alt)) config.max_altitude = static_cast<float>(max_alt->valuedouble);
    if (min_alt && cJSON_IsNumber(min_alt)) config.min_altitude = static_cast<float>(min_alt->valuedouble);
    if (action && cJSON_IsNumber(action)) config.action = static_cast<uint8_t>(action->valueint);
    if (return_min && cJSON_IsBool(return_min)) config.return_at_min = cJSON_IsTrue(return_min);
    if (breach && cJSON_IsNumber(breach)) config.breach_distance = static_cast<float>(breach->valuedouble);

    cJSON_Delete(root);
    get_active_geofence(core).set_config(config);
    return ok();
}

MGCORE_API char* mg_geofence_add_point(mg_core_t* core, double lat, double lon, float alt) {
    CHECK_CORE(core);
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid lat/lon");
    }

    auto& fence = get_active_geofence(core);
    fence.add_point(lat, lon, alt);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("point_count", static_cast<int>(fence.point_count()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_geofence_remove_point(mg_core_t* core, int index) {
    CHECK_CORE(core);
    if (index < 0) return fail(MG_ERROR_INVALID_PARAM, "invalid index");

    get_active_geofence(core).remove_point(static_cast<size_t>(index));
    return ok();
}

MGCORE_API char* mg_geofence_get_points(mg_core_t* core) {
    CHECK_CORE(core);

    auto points = get_active_geofence(core).get_points();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();
    for (size_t i = 0; i < points.size(); i++) {
        mg::core::telemetry::JsonBuilder p;
        p.begin_object();
        p.field("index", static_cast<int>(i));
        p.field("lat", points[i].lat);
        p.field("lon", points[i].lon);
        p.field("alt", points[i].alt);
        p.end_object();
        jb.append_json(p.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_geofence_validate(mg_core_t* core) {
    CHECK_CORE(core);

    auto result = get_active_geofence(core).validate();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid", result.valid);
    if (!result.error.empty()) jb.field("message", result.error);
    jb.field("area_m2", result.area_m2);
    jb.field("perimeter_m", result.perimeter_m);

    mg::core::telemetry::JsonBuilder warnings;
    warnings.begin_array();
    for (const auto& w : result.warnings) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("msg", w);
        item.end_object();
        warnings.append_json(item.str());
    }
    warnings.end_array();
    jb.raw_field("warnings", warnings.str());

    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_geofence_export_json(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(get_active_geofence(core).export_json());
}

MGCORE_API char* mg_geofence_import_json(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    if (get_active_geofence(core).import_json(json)) {
        return ok();
    }
    return fail(MG_ERROR_INVALID_PARAM, "failed to parse JSON");
}

MGCORE_API char* mg_geofence_export_kml(mg_core_t* core, const char* name) {
    CHECK_CORE(core);
    return to_cstr(get_active_geofence(core).export_kml(name ? name : "Geofence"));
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
