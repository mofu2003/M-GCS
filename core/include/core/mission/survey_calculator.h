/**
 * survey_calculator.h - Survey/grid mission planning
 *
 * Generates flight lines for aerial survey (photogrammetry).
 * Supports:
 *   - Simple grid (parallel flight lines)
 *   - Cross grid (two passes at 90 degrees)
 *   - Camera trigger distance calculation
 *   - Flight time / distance estimation
 *   - Polygon area calculation
 *
 * Design: inspired by QGC SurveyComplexItem.
 */
#pragma once

#include "core/mission/mission_types.h"
#include <vector>

namespace mg::core::mission {

/// Camera specification for survey planning
struct CameraSpec {
    float focal_length_mm = 35.0f;    // Lens focal length
    float sensor_width_mm = 23.5f;    // Sensor physical width
    float sensor_height_mm = 15.6f;   // Sensor physical height
    float image_width_px = 6000;      // Image width in pixels
    float image_height_px = 4000;     // Image height in pixels
    float min_overlap_pct = 70.0f;    // Minimum forward overlap (%)
    float side_overlap_pct = 60.0f;   // Minimum side overlap (%)
};

/// Survey configuration
struct SurveyConfig {
    double center_lat_deg = 0.0;      // Survey area center latitude
    double center_lon_deg = 0.0;      // Survey area center longitude
    double width_m = 100.0;           // Survey area width (meters)
    double length_m = 100.0;          // Survey area length (meters)
    float  altitude_m = 50.0f;        // Flight altitude (meters AGL)
    float  speed_ms = 5.0f;           // Flight speed (m/s)
    double angle_deg = 0.0;           // Grid rotation angle (degrees from north)
    bool   cross_grid = false;        // Generate cross-grid (90 degree second pass)
    bool   return_to_start = true;    // Add RTL at end
    CameraSpec camera;
};

/// Corridor scan configuration
struct CorridorConfig {
    double center_lat_deg = 0.0;    // Start point latitude
    double center_lon_deg = 0.0;    // Start point longitude
    double end_lat_deg = 0.0;       // End point latitude
    double end_lon_deg = 0.0;       // End point longitude
    float  width_m = 50.0f;         // Corridor width (meters)
    float  altitude_m = 50.0f;      // Flight altitude
    float  speed_ms = 5.0f;         // Flight speed
    float  side_overlap_pct = 60.0f; // Side overlap percentage
    bool   return_to_start = true;
    CameraSpec camera;
};

/// Survey calculation result
struct SurveyResult {
    std::vector<MissionItem> items;   // Generated mission items
    double total_distance_m = 0.0;    // Total flight distance
    double estimated_time_s = 0.0;    // Estimated flight time
    int    flight_line_count = 0;     // Number of flight lines
    int    waypoint_count = 0;        // Total waypoints
    double area_m2 = 0.0;            // Survey area
    int    photo_count = 0;          // Estimated photos
    bool   valid = false;
    std::string error;
};

class SurveyCalculator {
public:
    /// Generate a simple grid survey mission
    static SurveyResult generate_grid(const SurveyConfig& config);

    /// Generate a cross-grid survey mission (two passes at 90 degrees)
    static SurveyResult generate_cross_grid(const SurveyConfig& config);

    /// Generate a corridor scan mission (linear path with parallel sweeps)
    static SurveyResult generate_corridor_scan(const CorridorConfig& config);

    /// Calculate camera ground sampling distance (GSD) at given altitude
    static float calculate_gsd(const CameraSpec& camera, float altitude_m);

    /// Calculate required altitude for target GSD
    static float altitude_for_gsd(const CameraSpec& camera, float target_gsd_cm);

    /// Calculate image forward overlap at given speed and trigger interval
    static float calculate_forward_overlap(const CameraSpec& camera, float altitude_m,
                                          float speed_ms, float trigger_interval_s);

    /// Calculate camera trigger distance for desired overlap
    static float calculate_trigger_distance(const CameraSpec& camera, float altitude_m,
                                            float overlap_pct);

    /// Calculate side distance between flight lines for desired overlap
    static float calculate_line_spacing(const CameraSpec& camera, float altitude_m,
                                        float side_overlap_pct);

private:
    /// Rotate a point around center by angle (radians)
    static void rotate_point(double& lat, double& lon,
                             double center_lat, double center_lon,
                             double angle_rad);

    /// Convert meters to lat/lon offset (approximate)
    static void meters_to_deg_offset(double lat_deg, double lon_deg,
                                     double meters_north, double meters_east,
                                     double& out_lat_deg, double& out_lon_deg);
};

} // namespace mg::core::mission
