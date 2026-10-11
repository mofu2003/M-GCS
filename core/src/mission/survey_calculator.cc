/**
 * survey_calculator.cc - Survey/grid mission planning implementation
 *
 * Generates parallel flight lines for aerial survey.
 * Uses Haversine distance and trigonometry for accurate grid generation.
 */
#include "core/mission/survey_calculator.h"
#include "core/mission/mission_item_factory.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mg::core::mission {

// ================================================================
// Grid generation
// ================================================================

SurveyResult SurveyCalculator::generate_grid(const SurveyConfig& config) {
    SurveyResult result;

    if (config.width_m <= 0 || config.length_m <= 0 || config.altitude_m <= 0) {
        result.error = "invalid dimensions or altitude";
        return result;
    }

    // Calculate camera parameters
    float trigger_dist = calculate_trigger_distance(config.camera, config.altitude_m,
                                                     config.camera.min_overlap_pct);
    float line_spacing = calculate_line_spacing(config.camera, config.altitude_m,
                                                 config.camera.side_overlap_pct);

    // Ensure minimum line spacing
    if (line_spacing < 5.0f) line_spacing = 5.0f;

    // Grid rotation angle
    double angle_rad = config.angle_deg * M_PI / 180.0;

    // Number of flight lines needed to cover width
    int num_lines = static_cast<int>(std::ceil(config.width_m / line_spacing)) + 1;
    double half_width = config.width_m / 2.0;
    double half_length = config.length_m / 2.0;

    // Generate waypoints for each flight line
    std::vector<MissionItem> waypoints;

    for (int i = 0; i < num_lines; i++) {
        // Y position along width (centered at 0)
        double y = -half_width + (i * line_spacing);
        if (y > half_width) y = half_width;

        // Start and end points in local frame (centered at survey center)
        double start_lat, start_lon, end_lat, end_lon;

        // Start point (negative half_length)
        meters_to_deg_offset(config.center_lat_deg, config.center_lon_deg,
                             -half_length, y, start_lat, start_lon);

        // End point (positive half_length)
        meters_to_deg_offset(config.center_lat_deg, config.center_lon_deg,
                             half_length, y, end_lat, end_lon);

        // Apply rotation around center
        rotate_point(start_lat, start_lon,
                     config.center_lat_deg, config.center_lon_deg,
                     angle_rad);
        rotate_point(end_lat, end_lon,
                     config.center_lat_deg, config.center_lon_deg,
                     angle_rad);

        // Add waypoints for this flight line
        if (i % 2 == 0) {
            // Forward pass
            waypoints.push_back(MissionItemFactory::create_waypoint(
                start_lat, start_lon, config.altitude_m, 0.0f, 5.0f));
            waypoints.push_back(MissionItemFactory::create_waypoint(
                end_lat, end_lon, config.altitude_m, 0.0f, 5.0f));
        } else {
            // Reverse pass (for efficient coverage)
            waypoints.push_back(MissionItemFactory::create_waypoint(
                end_lat, end_lon, config.altitude_m, 0.0f, 5.0f));
            waypoints.push_back(MissionItemFactory::create_waypoint(
                start_lat, start_lon, config.altitude_m, 0.0f, 5.0f));
        }
    }

    // Calculate statistics
    result.flight_line_count = num_lines;
    result.waypoint_count = static_cast<int>(waypoints.size());
    result.area_m2 = config.width_m * config.length_m;

    // Calculate total distance
    double total_dist = 0;
    for (size_t i = 1; i < waypoints.size(); i++) {
        double lat1 = waypoints[i-1].x / 1e7;
        double lon1 = waypoints[i-1].y / 1e7;
        double lat2 = waypoints[i].x / 1e7;
        double lon2 = waypoints[i].y / 1e7;
        total_dist += MissionItemFactory::haversine_distance(lat1, lon1, lat2, lon2);
    }
    result.total_distance_m = total_dist;

    // Calculate estimated time
    if (config.speed_ms > 0) {
        result.estimated_time_s = total_dist / config.speed_ms;
    }

    // Estimate photo count
    if (trigger_dist > 0) {
        result.photo_count = static_cast<int>(total_dist / trigger_dist);
    }

    // Build mission items: camera trigger at start + waypoints
    std::vector<MissionItem> mission_items;

    // Camera trigger distance command at start
    mission_items.push_back(MissionItemFactory::create_camera_trigger(trigger_dist));

    // Speed command
    mission_items.push_back(MissionItemFactory::create_speed_change(config.speed_ms));

    // All waypoints
    for (auto& wp : waypoints) {
        mission_items.push_back(wp);
    }

    // RTL if requested
    if (config.return_to_start) {
        mission_items.push_back(MissionItemFactory::create_rtl());
    }

    // Re-sequence
    for (size_t i = 0; i < mission_items.size(); i++) {
        mission_items[i].seq = static_cast<uint16_t>(i);
    }

    result.items = mission_items;
    result.valid = true;
    return result;
}

SurveyResult SurveyCalculator::generate_cross_grid(const SurveyConfig& config) {
    // Generate first pass
    SurveyResult pass1 = generate_grid(config);
    if (!pass1.valid) return pass1;

    // Generate second pass rotated 90 degrees
    SurveyConfig config2 = config;
    config2.angle_deg = config.angle_deg + 90.0;
    config2.return_to_start = false; // Don't RTL between passes
    SurveyResult pass2 = generate_grid(config2);
    if (!pass2.valid) return pass2;

    // Combine: pass1 items + pass2 items (skip duplicate camera trigger/speed)
    SurveyResult combined;
    combined.items = pass1.items;

    // Skip non-waypoint items (camera trigger, speed change) from pass2
    for (size_t i = 0; i < pass2.items.size(); i++) {
        auto item = pass2.items[i];
        // Skip camera trigger and speed change commands (they're already in pass1)
        if (item.command == 206 || item.command == 178) continue;
        item.seq = static_cast<uint16_t>(combined.items.size());
        combined.items.push_back(item);
    }

    // Add RTL at end
    if (config.return_to_start) {
        auto rtl = MissionItemFactory::create_rtl();
        rtl.seq = static_cast<uint16_t>(combined.items.size());
        combined.items.push_back(rtl);
    }

    combined.total_distance_m = pass1.total_distance_m + pass2.total_distance_m;
    combined.estimated_time_s = pass1.estimated_time_s + pass2.estimated_time_s;
    combined.flight_line_count = pass1.flight_line_count + pass2.flight_line_count;
    combined.waypoint_count = static_cast<int>(combined.items.size());
    combined.area_m2 = pass1.area_m2;
    combined.photo_count = pass1.photo_count + pass2.photo_count;
    combined.valid = true;
    return combined;
}

SurveyResult SurveyCalculator::generate_corridor_scan(const CorridorConfig& config) {
    SurveyResult result;

    if (config.width_m <= 0 || config.altitude_m <= 0) {
        result.error = "invalid width or altitude";
        return result;
    }

    // Corridor direction vector (in meters, approximate)
    double lat_rad = config.center_lat_deg * M_PI / 180.0;
    double dlat_m = (config.end_lat_deg - config.center_lat_deg) * 111132.92;
    double dlon_m = (config.end_lon_deg - config.center_lon_deg) * 111412.84 * std::cos(lat_rad);
    double corridor_len = std::sqrt(dlat_m * dlat_m + dlon_m * dlon_m);

    if (corridor_len < 1.0) {
        result.error = "corridor length too short";
        return result;
    }

    // Unit vectors: along corridor and perpendicular
    double ux = dlat_m / corridor_len;   // along (north component)
    double uy = dlon_m / corridor_len;   // along (east component)
    // Perpendicular (rotate 90 degrees clockwise): (uy, -ux)
    double px = uy;
    double py = -ux;

    // Line spacing from camera side overlap
    float line_spacing = calculate_line_spacing(config.camera, config.altitude_m,
                                                 config.side_overlap_pct);
    if (line_spacing < 5.0f) line_spacing = 5.0f;

    // Number of sweep lines across corridor width
    int num_lines = static_cast<int>(std::ceil(config.width_m / line_spacing)) + 1;
    double half_width = config.width_m / 2.0;

    // Generate waypoints for each flight line
    std::vector<MissionItem> waypoints;

    for (int i = 0; i < num_lines; i++) {
        // Lateral offset from corridor center
        double lateral = -half_width + (i * line_spacing);
        if (lateral > half_width) lateral = half_width;

        // Line start and end along corridor, offset laterally
        // Start of corridor + lateral perpendicular offset
        double start_north = lateral * px;
        double start_east  = lateral * py;
        double end_north   = corridor_len * ux + lateral * px;
        double end_east    = corridor_len * uy + lateral * py;

        double start_lat, start_lon, end_lat, end_lon;
        meters_to_deg_offset(config.center_lat_deg, config.center_lon_deg,
                             start_north, start_east, start_lat, start_lon);
        meters_to_deg_offset(config.center_lat_deg, config.center_lon_deg,
                             end_north, end_east, end_lat, end_lon);

        // Alternate direction for efficient flight (boustrophedon)
        if (i % 2 == 0) {
            waypoints.push_back(MissionItemFactory::create_waypoint(
                start_lat, start_lon, config.altitude_m, 0.0f, 5.0f));
            waypoints.push_back(MissionItemFactory::create_waypoint(
                end_lat, end_lon, config.altitude_m, 0.0f, 5.0f));
        } else {
            waypoints.push_back(MissionItemFactory::create_waypoint(
                end_lat, end_lon, config.altitude_m, 0.0f, 5.0f));
            waypoints.push_back(MissionItemFactory::create_waypoint(
                start_lat, start_lon, config.altitude_m, 0.0f, 5.0f));
        }
    }

    // Calculate statistics
    result.flight_line_count = num_lines;
    result.waypoint_count = static_cast<int>(waypoints.size());
    result.area_m2 = config.width_m * corridor_len;

    // Calculate total distance
    double total_dist = 0;
    for (size_t i = 1; i < waypoints.size(); i++) {
        double lat1 = waypoints[i-1].x / 1e7;
        double lon1 = waypoints[i-1].y / 1e7;
        double lat2 = waypoints[i].x / 1e7;
        double lon2 = waypoints[i].y / 1e7;
        total_dist += MissionItemFactory::haversine_distance(lat1, lon1, lat2, lon2);
    }
    result.total_distance_m = total_dist;

    // Estimated time
    if (config.speed_ms > 0) {
        result.estimated_time_s = total_dist / config.speed_ms;
    }

    // Photo count estimate
    float trigger_dist = calculate_trigger_distance(config.camera, config.altitude_m,
                                                     config.camera.min_overlap_pct);
    if (trigger_dist > 0) {
        result.photo_count = static_cast<int>(total_dist / trigger_dist);
    }

    // Build mission items: camera trigger + speed + waypoints + optional RTL
    std::vector<MissionItem> mission_items;
    mission_items.push_back(MissionItemFactory::create_camera_trigger(trigger_dist));
    mission_items.push_back(MissionItemFactory::create_speed_change(config.speed_ms));

    for (auto& wp : waypoints) {
        mission_items.push_back(wp);
    }

    if (config.return_to_start) {
        mission_items.push_back(MissionItemFactory::create_rtl());
    }

    // Re-sequence
    for (size_t i = 0; i < mission_items.size(); i++) {
        mission_items[i].seq = static_cast<uint16_t>(i);
    }

    result.items = mission_items;
    result.valid = true;
    return result;
}

// ================================================================
// Camera calculations
// ================================================================

float SurveyCalculator::calculate_gsd(const CameraSpec& camera, float altitude_m) {
    // GSD = (altitude * sensor_width) / (focal_length * image_width)
    // Result in meters per pixel
    return (altitude_m * camera.sensor_width_mm) /
           (camera.focal_length_mm * camera.image_width_px);
}

float SurveyCalculator::altitude_for_gsd(const CameraSpec& camera, float target_gsd_cm) {
    float target_gsd_m = target_gsd_cm / 100.0f;
    return (target_gsd_m * camera.focal_length_mm * camera.image_width_px) /
           camera.sensor_width_mm;
}

float SurveyCalculator::calculate_forward_overlap(const CameraSpec& camera, float altitude_m,
                                                  float speed_ms, float trigger_interval_s) {
    // Image width on ground = (altitude * sensor_width) / focal_length
    float image_width_ground = (altitude_m * camera.sensor_width_mm) /
                               camera.focal_length_mm;
    float distance_between = speed_ms * trigger_interval_s;
    if (distance_between <= 0) return 0;
    return (1.0f - distance_between / image_width_ground) * 100.0f;
}

float SurveyCalculator::calculate_trigger_distance(const CameraSpec& camera, float altitude_m,
                                                    float overlap_pct) {
    // trigger_distance = image_width_ground * (1 - overlap/100)
    float image_width_ground = (altitude_m * camera.sensor_width_mm) /
                               camera.focal_length_mm;
    return image_width_ground * (1.0f - overlap_pct / 100.0f);
}

float SurveyCalculator::calculate_line_spacing(const CameraSpec& camera, float altitude_m,
                                                float side_overlap_pct) {
    // image_height_ground = (altitude * sensor_height) / focal_length
    float image_height_ground = (altitude_m * camera.sensor_height_mm) /
                                camera.focal_length_mm;
    return image_height_ground * (1.0f - side_overlap_pct / 100.0f);
}

// ================================================================
// Geometry helpers
// ================================================================

void SurveyCalculator::meters_to_deg_offset(double lat_deg, double lon_deg,
                                             double meters_north, double meters_east,
                                             double& out_lat_deg, double& out_lon_deg) {
    // Approximate conversion (accurate for small distances)
    double lat_rad = lat_deg * M_PI / 180.0;
    double meters_per_deg_lat = 111132.92;
    double meters_per_deg_lon = 111412.84 * std::cos(lat_rad);

    out_lat_deg = lat_deg + meters_north / meters_per_deg_lat;
    out_lon_deg = lon_deg + meters_east / meters_per_deg_lon;
}

void SurveyCalculator::rotate_point(double& lat, double& lon,
                                     double center_lat, double center_lon,
                                     double angle_rad) {
    if (std::abs(angle_rad) < 1e-10) return; // No rotation needed

    // Convert to approximate meters from center
    double lat_rad = center_lat * M_PI / 180.0;
    double dx = (lon - center_lon) * 111412.84 * std::cos(lat_rad);
    double dy = (lat - center_lat) * 111132.92;

    // Rotate
    double cos_a = std::cos(angle_rad);
    double sin_a = std::sin(angle_rad);
    double new_dx = dx * cos_a - dy * sin_a;
    double new_dy = dx * sin_a + dy * cos_a;

    // Convert back to degrees
    lon = center_lon + new_dx / (111412.84 * std::cos(lat_rad));
    lat = center_lat + new_dy / 111132.92;
}

} // namespace mg::core::mission
