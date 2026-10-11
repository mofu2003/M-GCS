#define MGCORE_EXPORTS
/**
 * Survey planning FFI entrypoints.
 */
#include "ffi_internal.h"
#include "core/telemetry/json_serializers.h"
#include "mission_json_serializers.h"
#include "core/mission/survey_calculator.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

namespace {

void append_camera_spec_from_json(cJSON* root, mg::core::mission::CameraSpec& camera) {
    cJSON* cam = cJSON_GetObjectItemCaseSensitive(root, "camera");
    if (!cam) return;

    auto get_f_cam = [&](const char* key, float def) -> float {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(cam, key);
        return (f && cJSON_IsNumber(f)) ? static_cast<float>(f->valuedouble) : def;
    };

    camera.focal_length_mm = get_f_cam("focal_length", 35);
    camera.sensor_width_mm = get_f_cam("sensor_width", 23.5);
    camera.sensor_height_mm = get_f_cam("sensor_height", 15.6);
    camera.image_width_px = get_f_cam("image_width", 6000);
    camera.image_height_px = get_f_cam("image_height", 4000);
    camera.min_overlap_pct = get_f_cam("min_overlap", 70);
    camera.side_overlap_pct = get_f_cam("side_overlap", 60);
}

char* survey_result_json(const mg::core::mission::SurveyResult& result) {
    if (!result.valid) {
        mg::core::telemetry::JsonBuilder err;
        err.begin_object();
        err.field("valid", false);
        err.field("message", result.error);
        err.end_object();
        return to_cstr(err.str());
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid", true);
    jb.field("waypoint_count", result.waypoint_count);
    jb.field("flight_line_count", result.flight_line_count);
    jb.field("total_distance_m", result.total_distance_m);
    jb.field("estimated_time_s", result.estimated_time_s);
    jb.field("area_m2", result.area_m2);
    jb.field("photo_count", result.photo_count);

    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    for (const auto& item : result.items) {
        arr.append_json(mg::bridge::mission_item_json(item));
    }
    arr.end_array();
    jb.raw_field("waypoints", arr.str());
    jb.end_object();
    return to_cstr(jb.str());
}

mg::core::mission::SurveyConfig parse_survey_config(cJSON* json) {
    mg::core::mission::SurveyConfig cfg;
    if (!json) return cfg;

    auto get_d = [&](const char* key, double def) -> double {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(json, key);
        return (f && cJSON_IsNumber(f)) ? f->valuedouble : def;
    };
    auto get_f = [&](const char* key, float def) -> float {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(json, key);
        return (f && cJSON_IsNumber(f)) ? static_cast<float>(f->valuedouble) : def;
    };
    auto get_b = [&](const char* key, bool def) -> bool {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(json, key);
        return (f && cJSON_IsTrue(f)) ? true : (f && cJSON_IsFalse(f)) ? false : def;
    };

    cfg.center_lat_deg = get_d("center_lat", 0);
    cfg.center_lon_deg = get_d("center_lon", 0);
    cfg.width_m = get_d("width", 100);
    cfg.length_m = get_d("length", 100);
    cfg.altitude_m = get_f("altitude", 50);
    cfg.speed_ms = get_f("speed", 5);
    cfg.angle_deg = get_d("angle", 0);
    cfg.cross_grid = get_b("cross_grid", false);
    cfg.return_to_start = get_b("return_to_start", true);
    append_camera_spec_from_json(json, cfg.camera);
    return cfg;
}

} // namespace

MGCORE_API char* mg_survey_generate(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!json_config || json_config[0] == '\0')
        return to_cstr(R"({"valid":false,"message":"empty config"})");

    cJSON* root = mg::core::json::parse(json_config);
    if (!root) return to_cstr(R"({"valid":false,"message":"invalid JSON"})");

    auto cfg = parse_survey_config(root);
    cJSON_Delete(root);
    return survey_result_json(mg::core::mission::SurveyCalculator::generate_grid(cfg));
}

MGCORE_API char* mg_survey_generate_corridor(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    if (!json_config || json_config[0] == '\0')
        return to_cstr(R"({"valid":false,"message":"empty config"})");

    cJSON* root = mg::core::json::parse(json_config);
    if (!root) return to_cstr(R"({"valid":false,"message":"invalid JSON"})");

    mg::core::mission::CorridorConfig cfg;
    auto get_d = [&](const char* key, double def) -> double {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(root, key);
        return (f && cJSON_IsNumber(f)) ? f->valuedouble : def;
    };
    auto get_f = [&](const char* key, float def) -> float {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(root, key);
        return (f && cJSON_IsNumber(f)) ? static_cast<float>(f->valuedouble) : def;
    };
    auto get_b = [&](const char* key, bool def) -> bool {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(root, key);
        return (f && cJSON_IsTrue(f)) ? true : (f && cJSON_IsFalse(f)) ? false : def;
    };

    cfg.center_lat_deg = get_d("start_lat", 0);
    cfg.center_lon_deg = get_d("start_lon", 0);
    cfg.end_lat_deg    = get_d("end_lat", 0);
    cfg.end_lon_deg    = get_d("end_lon", 0);
    cfg.width_m        = get_f("width", 50);
    cfg.altitude_m     = get_f("altitude", 50);
    cfg.speed_ms       = get_f("speed", 5);
    cfg.side_overlap_pct = get_f("side_overlap", 60);
    cfg.return_to_start  = get_b("return_to_start", true);
    append_camera_spec_from_json(root, cfg.camera);

    cJSON_Delete(root);
    return survey_result_json(mg::core::mission::SurveyCalculator::generate_corridor_scan(cfg));
}
