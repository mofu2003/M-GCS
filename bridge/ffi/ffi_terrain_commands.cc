#define MGCORE_EXPORTS
/**
 * Terrain FFI entrypoints.
 *
 * 地形高程缓存与剖面采样。core 不发起网络请求（见 core/terrain/terrain_store.h）：
 * 前端 provider 拉取高程后经 `mg_terrain_cache_update` 写入缓存，
 * `mg_terrain_query` / `mg_terrain_query_line` 只读缓存。缓存挂在 mg_core_t 上，
 * 因此跨 FFI 调用共享（旧实现每次调用新建查询对象，缓存不跨调用）。
 */
#include "ffi_internal.h"
#include "core/terrain/terrain_store.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

namespace {

/// 序列化一个地形采样点。未命中缓存时 `valid:false` 且 `elevation:null`，
/// 调用方据此决定是否向 provider 补拉高程。
mg::core::telemetry::JsonBuilder point_json(const mg::core::terrain::TerrainPoint& pt) {
    mg::core::telemetry::JsonBuilder obj;
    obj.begin_object();
    obj.field("lat", pt.lat_deg);
    obj.field("lon", pt.lon_deg);
    if (pt.valid) {
        obj.field("valid", true);
        obj.field("elevation", static_cast<double>(pt.elevation_m));
    } else {
        obj.field("valid", false);
        obj.raw_field("elevation", "null");
    }
    obj.end_object();
    return obj;
}

}  // namespace

MGCORE_API char* mg_terrain_query(mg_core_t* core, double lat, double lon) {
    CHECK_CORE(core);
    auto cached = core->terrain.lookup(lat, lon);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("lat", lat);
    jb.field("lon", lon);
    if (cached.has_value()) {
        jb.field("valid", true);
        jb.field("elevation", static_cast<double>(cached.value()));
    } else {
        jb.field("valid", false);
        jb.raw_field("elevation", "null");
    }
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_terrain_query_line(mg_core_t* core,
                                        double lat1, double lon1,
                                        double lat2, double lon2,
                                        int num_samples) {
    CHECK_CORE(core);
    auto profile = core->terrain.sample_line(lat1, lon1, lat2, lon2, num_samples);

    mg::core::telemetry::JsonBuilder points;
    points.begin_array();
    for (const auto& pt : profile.points) {
        points.append_object(point_json(pt));
    }
    points.end_array();

    mg::core::telemetry::JsonBuilder missing;
    missing.begin_array();
    for (size_t idx : profile.missing) {
        missing.append_json(std::to_string(idx));
    }
    missing.end_array();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("complete", profile.complete());
    jb.field("missing_count", static_cast<int>(profile.missing.size()));
    jb.raw_field("missing", missing.str());
    jb.raw_field("points", points.str());
    jb.end_object();
    return to_cstr(jb.str());
}

/// 把前端 provider 拉取到的高程批量写入缓存。
/// 请求体：{"points":[{"lat":..,"lon":..,"elevation":..}, ...]}
/// 缺少 lat/lon/elevation 的条目会被跳过；返回实际写入的条数。
MGCORE_API char* mg_terrain_cache_update(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    cJSON* root = mg::core::json::parse(json);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");

    cJSON* points = cJSON_GetObjectItemCaseSensitive(root, "points");
    if (!points || !cJSON_IsArray(points)) {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "missing 'points' array");
    }

    std::vector<mg::core::terrain::TerrainPoint> batch;
    cJSON* item = nullptr;
    cJSON_ArrayForEach(item, points) {
        cJSON* lat = cJSON_GetObjectItemCaseSensitive(item, "lat");
        cJSON* lon = cJSON_GetObjectItemCaseSensitive(item, "lon");
        cJSON* elev = cJSON_GetObjectItemCaseSensitive(item, "elevation");
        if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon) || !cJSON_IsNumber(elev)) continue;
        mg::core::terrain::TerrainPoint pt;
        pt.lat_deg = lat->valuedouble;
        pt.lon_deg = lon->valuedouble;
        pt.elevation_m = static_cast<float>(elev->valuedouble);
        pt.valid = true;
        batch.push_back(pt);
    }
    cJSON_Delete(root);

    size_t stored = core->terrain.store_batch(batch);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("stored", static_cast<int>(stored));
    jb.field("cache_size", static_cast<int>(core->terrain.size()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_terrain_cache_clear(mg_core_t* core) {
    CHECK_CORE(core);
    core->terrain.clear();
    return ok();
}
