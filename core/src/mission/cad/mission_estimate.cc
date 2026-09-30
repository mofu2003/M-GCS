#include "mission_document_internal.h"

#include "core/mission/mission_manager.h"

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

    for (const MissionEntity& entity : document.entities) {
        if (!entity.enabled) {
            continue;
        }
        ++estimate.enabled_entity_count;

        if (is_parameterized_survey_type(entity.type)) {
            const SurveyResult generated = expand_survey_entity(entity);
            if (!generated.valid) {
                continue;
            }
            estimate.waypoint_count += generated.waypoint_count;
            estimate.command_count += static_cast<int>(generated.items.size()) -
                                      generated.waypoint_count;
            estimate.total_distance_m += generated.total_distance_m;
            estimate.estimated_time_s += generated.estimated_time_s;
            estimate.photo_count += generated.photo_count;

            const double altitude = static_cast<double>(entity.survey.altitude_m);
            if (altitude >= 0.0) {
                if (estimate.min_altitude_m == 0.0 ||
                    altitude < estimate.min_altitude_m) {
                    estimate.min_altitude_m = altitude;
                }
                estimate.max_altitude_m = std::max(estimate.max_altitude_m, altitude);
            }

            // Use the final generated point as the next entity's start so the
            // estimate does not break at a survey block boundary.
            has_previous = false;
            for (const MissionItem& item : generated.items) {
                if (item.x == 0 && item.y == 0) {
                    continue;
                }
                previous_lat = static_cast<double>(item.x) / 1e7;
                previous_lon = static_cast<double>(item.y) / 1e7;
                has_previous = true;
            }
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
                const double segment_distance = MissionManager::haversine_distance(
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
