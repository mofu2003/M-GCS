#include "mission_document_internal.h"

#include "core/mission/geo_math.h"

#include <algorithm>
#include <cmath>

namespace mg::core::mission::cad::detail {

MissionEstimate estimate_document_impl(const MissionDocument& document) {
    MissionEstimate estimate;
    bool has_previous = false;
    double previous_lat = 0.0;
    double previous_lon = 0.0;
    double current_speed = document.defaults.speed_ms;
    double photo_trigger_distance = 0.0;

    std::vector<ExpandedEntity> ordered;
    std::string expand_error;
    if (!expand_document_entities(document, ordered, expand_error)) {
        // Best-effort fallback: estimate the plain entities when a complex item
        // cannot be expanded, so the caller still gets a partial figure.
        ordered.clear();
        for (const MissionEntity& entity : document.entities) {
            ordered.push_back({entity, entity.id});
        }
    }

    for (const ExpandedEntity& expanded : ordered) {
        const MissionEntity& entity = expanded.entity;
        if (!entity.enabled) {
            continue;
        }
        ++estimate.enabled_entity_count;

        // Survey功能已删除：跳过 survey 实体
        if (is_parameterized_survey_type(entity.type)) {
            continue;
        }

        if (entity.command == static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            ++estimate.waypoint_count;
        } else {
            ++estimate.command_count;
        }

        const float altitude = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        if (std::isfinite(altitude) && altitude >= 0.0f) {
            if (estimate.min_altitude_m == 0.0 ||
                altitude < estimate.min_altitude_m) {
                estimate.min_altitude_m = altitude;
            }
            estimate.max_altitude_m =
                std::max(estimate.max_altitude_m, static_cast<double>(altitude));
        }

        if (position_command(entity.command) &&
            finite(entity.latitude) &&
            finite(entity.longitude)) {
            if (has_previous) {
                const double segment_distance = haversine_distance(
                    previous_lat,
                    previous_lon,
                    entity.latitude,
                    entity.longitude);
                estimate.total_distance_m += segment_distance;
                if (current_speed > 0.0) {
                    estimate.estimated_time_s +=
                        segment_distance / current_speed;
                }
                if (photo_trigger_distance > 0.0) {
                    estimate.photo_count += static_cast<int>(
                        std::floor(segment_distance / photo_trigger_distance));
                }
            }
            previous_lat = entity.latitude;
            previous_lon = entity.longitude;
            has_previous = true;
        }

        if (entity.command == static_cast<uint16_t>(NavCommand::DoSetSpeed) &&
            entity.params[1] > 0.0f &&
            (entity.params[0] == 0.0f || entity.params[0] == 1.0f)) {
            current_speed = entity.params[1];
        }
        if (entity.command ==
                static_cast<uint16_t>(NavCommand::DoSetCamTrigDist) &&
            entity.params[0] > 0.0f) {
            photo_trigger_distance = entity.params[0];
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            estimate.estimated_time_s += std::max(0.0f, entity.params[0]);
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTime)) {
            estimate.estimated_time_s += std::max(0.0f, entity.params[0]);
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns)) {
            estimate.estimated_time_s +=
                std::max(0.0f, entity.params[0]) * 10.0;
        }
    }

    return estimate;
}

} // namespace mg::core::mission::cad::detail
