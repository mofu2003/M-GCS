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
