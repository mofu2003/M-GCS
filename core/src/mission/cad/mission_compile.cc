#include "mission_document_internal.h"

#include <cmath>

namespace mg::core::mission::cad::detail {

namespace {

CameraSpec survey_camera_spec(const MissionSurveySpec& survey) {
    CameraSpec camera;
    camera.focal_length_mm = survey.focal_length_mm;
    camera.sensor_width_mm = survey.sensor_width_mm;
    camera.sensor_height_mm = survey.sensor_height_mm;
    camera.image_width_px = survey.image_width_px;
    camera.image_height_px = survey.image_height_px;
    camera.min_overlap_pct = survey.forward_overlap_pct;
    camera.side_overlap_pct = survey.side_overlap_pct;
    return camera;
}

SurveyConfig survey_grid_config(const MissionEntity& entity) {
    const MissionSurveySpec& spec = entity.survey;
    SurveyConfig config;
    config.center_lat_deg = entity.latitude;
    config.center_lon_deg = entity.longitude;
    config.width_m = spec.width_m;
    config.length_m = spec.length_m;
    config.altitude_m = spec.altitude_m;
    config.speed_ms = spec.speed_ms;
    config.angle_deg = spec.angle_deg;
    config.cross_grid = spec.cross_grid;
    config.return_to_start = spec.return_to_start;
    config.camera = survey_camera_spec(spec);
    return config;
}

CorridorConfig corridor_config(const MissionEntity& entity) {
    const MissionSurveySpec& spec = entity.survey;
    CorridorConfig config;
    config.center_lat_deg = entity.latitude;
    config.center_lon_deg = entity.longitude;
    config.end_lat_deg = spec.end_latitude;
    config.end_lon_deg = spec.end_longitude;
    config.width_m = spec.corridor_width_m;
    config.altitude_m = spec.altitude_m;
    config.speed_ms = spec.speed_ms;
    config.side_overlap_pct = spec.side_overlap_pct;
    config.return_to_start = spec.return_to_start;
    config.camera = survey_camera_spec(spec);
    return config;
}

} // namespace

SurveyResult expand_survey_entity(const MissionEntity& entity) {
    if (entity.type == "corridor") {
        return SurveyCalculator::generate_corridor_scan(corridor_config(entity));
    }
    if (entity.survey.cross_grid) {
        return SurveyCalculator::generate_cross_grid(survey_grid_config(entity));
    }
    return SurveyCalculator::generate_grid(survey_grid_config(entity));
}

MissionCompileResult compile_document_impl(const MissionDocument& document) {
    MissionCompileResult result;
    result.report = validate_document(document);
    if (result.report.has_errors()) {
        return result;
    }

    for (const MissionEntity& entity : document.entities) {
        if (!entity.enabled) {
            continue;
        }

        if (is_parameterized_survey_type(entity.type)) {
            // Parameterized entities store intent and expand through one shared
            // calculator path so compile and estimate cannot diverge.
            const SurveyResult generated = expand_survey_entity(entity);
            if (!generated.valid || generated.items.empty()) {
                add_diagnostic(
                    result.report,
                    DiagnosticSeverity::Error,
                    "entity.survey_expansion_failed",
                    generated.error.empty()
                        ? "Survey entity produced no mission items"
                        : generated.error,
                    entity.id,
                    "survey");
                result.plan.valid = false;
                result.valid = false;
                return result;
            }
            for (MissionItem item : generated.items) {
                item.seq = static_cast<uint16_t>(result.plan.items.size());
                result.plan.items.push_back(item);
                result.source_entity_ids.push_back(entity.id);
            }
            continue;
        }

        MissionItem item{};
        item.seq = static_cast<uint16_t>(result.plan.items.size());
        item.frame = entity.frame;
        item.command = entity.command;
        item.autocontinue = entity.autocontinue;
        item.param1 = entity.params[0];
        item.param2 = entity.params[1];
        item.param3 = entity.params[2];
        item.param4 = entity.params[3];
        if (std::isfinite(entity.latitude)) {
            item.x = static_cast<int32_t>(std::llround(entity.latitude * 1e7));
        }
        if (std::isfinite(entity.longitude)) {
            item.y = static_cast<int32_t>(std::llround(entity.longitude * 1e7));
        }
        item.z = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        item.mission_type = 0;
        result.plan.items.push_back(item);
        result.source_entity_ids.push_back(entity.id);
    }

    result.plan.valid = !result.plan.items.empty();
    result.valid = result.plan.valid;
    return result;
}

} // namespace mg::core::mission::cad::detail

namespace mg::core::mission::cad {

namespace {

// MAV_CMD_NAV_FENCE_* / MAV_FRAME_* 常量，取值与 QGC GeoFenceManager 一致。
constexpr uint16_t kFenceReturnPoint = 5000;
constexpr uint16_t kFencePolygonVertexInclusion = 5001;
constexpr uint16_t kFencePolygonVertexExclusion = 5002;
constexpr uint16_t kFenceCircleInclusion = 5003;
constexpr uint16_t kFenceCircleExclusion = 5004;
constexpr uint8_t kFrameGlobal = 0;             // MAV_FRAME_GLOBAL
constexpr uint8_t kFrameGlobalRelAlt = 3;       // MAV_FRAME_GLOBAL_RELATIVE_ALT
constexpr uint8_t kMissionTypeFence = 1;        // MAV_MISSION_TYPE_FENCE
constexpr uint16_t kNavRallyPoint = 5100;       // MAV_CMD_NAV_RALLY_POINT
constexpr uint8_t kMissionTypeRally = 2;        // MAV_MISSION_TYPE_RALLY

bool fence_coordinate_valid(const MissionGeoPoint& point) {
    return std::isfinite(point.latitude) &&
           point.latitude >= -90.0 && point.latitude <= 90.0 &&
           std::isfinite(point.longitude) &&
           point.longitude >= -180.0 && point.longitude <= 180.0;
}

MissionItem fence_item(uint8_t frame, uint16_t command, double latitude,
                       double longitude) {
    MissionItem item{};
    item.frame = frame;
    item.command = command;
    item.autocontinue = 0;
    item.x = static_cast<int32_t>(std::llround(latitude * 1e7));
    item.y = static_cast<int32_t>(std::llround(longitude * 1e7));
    item.mission_type = kMissionTypeFence;
    return item;
}

} // namespace

bool compile_geo_fence_plan(
    const MissionGeoFence& fence,
    MissionPlan& plan,
    std::string& error) {
    error.clear();
    plan.clear();

    const bool has_breach = fence.breach_return_set;
    if (fence.polygons.empty() && fence.circles.empty() && !has_breach) {
        error = "geo fence is empty";
        return false;
    }

    for (size_t polygon_index = 0; polygon_index < fence.polygons.size();
         ++polygon_index) {
        const MissionGeoFencePolygon& polygon = fence.polygons[polygon_index];
        if (polygon.vertices.size() < 3) {
            error = "geo fence polygon " + std::to_string(polygon_index) +
                    " requires at least three vertices";
            plan.clear();
            return false;
        }
        const uint16_t command = polygon.inclusion
            ? kFencePolygonVertexInclusion
            : kFencePolygonVertexExclusion;
        for (size_t vertex_index = 0; vertex_index < polygon.vertices.size();
             ++vertex_index) {
            const MissionGeoPoint& vertex = polygon.vertices[vertex_index];
            if (!fence_coordinate_valid(vertex)) {
                error = "geo fence polygon " + std::to_string(polygon_index) +
                        " vertex " + std::to_string(vertex_index) +
                        " is invalid";
                plan.clear();
                return false;
            }
            MissionItem item =
                fence_item(kFrameGlobal, command, vertex.latitude,
                           vertex.longitude);
            item.param1 = static_cast<float>(polygon.vertices.size());
            item.seq = static_cast<uint16_t>(plan.items.size());
            plan.items.push_back(item);
        }
    }

    for (size_t circle_index = 0; circle_index < fence.circles.size();
         ++circle_index) {
        const MissionGeoFenceCircle& circle = fence.circles[circle_index];
        if (!fence_coordinate_valid(circle.center)) {
            error = "geo fence circle " + std::to_string(circle_index) +
                    " center is invalid";
            plan.clear();
            return false;
        }
        if (!std::isfinite(circle.radius_m) || circle.radius_m <= 0.0) {
            error = "geo fence circle " + std::to_string(circle_index) +
                    " radius must be positive";
            plan.clear();
            return false;
        }
        MissionItem item = fence_item(
            kFrameGlobal,
            circle.inclusion ? kFenceCircleInclusion : kFenceCircleExclusion,
            circle.center.latitude,
            circle.center.longitude);
        item.param1 = static_cast<float>(circle.radius_m);
        item.seq = static_cast<uint16_t>(plan.items.size());
        plan.items.push_back(item);
    }

    if (has_breach) {
        if (!fence_coordinate_valid(fence.breach_return)) {
            error = "geo fence breach-return point is invalid";
            plan.clear();
            return false;
        }
        if (!std::isfinite(
                static_cast<double>(fence.breach_return_altitude_m))) {
            error = "geo fence breach-return altitude is invalid";
            plan.clear();
            return false;
        }
        MissionItem item = fence_item(
            kFrameGlobalRelAlt,
            kFenceReturnPoint,
            fence.breach_return.latitude,
            fence.breach_return.longitude);
        item.z = fence.breach_return_altitude_m;
        item.seq = static_cast<uint16_t>(plan.items.size());
        plan.items.push_back(item);
    }

    plan.valid = !plan.items.empty();
    return plan.valid;
}

bool compile_rally_plan(
    const std::vector<MissionRallyPoint>& rally_points,
    MissionPlan& plan,
    std::string& error) {
    error.clear();
    plan.clear();

    if (rally_points.empty()) {
        error = "rally point list is empty";
        return false;
    }

    for (size_t index = 0; index < rally_points.size(); ++index) {
        const MissionRallyPoint& rally = rally_points[index];
        const MissionGeoPoint coordinate{rally.latitude, rally.longitude};
        if (!fence_coordinate_valid(coordinate)) {
            error = "rally point " + std::to_string(index) + " is invalid";
            plan.clear();
            return false;
        }
        if (!std::isfinite(static_cast<double>(rally.altitude_m))) {
            error = "rally point " + std::to_string(index) +
                    " altitude is invalid";
            plan.clear();
            return false;
        }
        MissionItem item{};
        item.seq = static_cast<uint16_t>(plan.items.size());
        item.frame = kFrameGlobalRelAlt;
        item.command = kNavRallyPoint;
        item.autocontinue = 0;
        item.x = static_cast<int32_t>(std::llround(rally.latitude * 1e7));
        item.y = static_cast<int32_t>(std::llround(rally.longitude * 1e7));
        item.z = rally.altitude_m;
        item.mission_type = kMissionTypeRally;
        plan.items.push_back(item);
    }

    plan.valid = !plan.items.empty();
    return plan.valid;
}

} // namespace mg::core::mission::cad
