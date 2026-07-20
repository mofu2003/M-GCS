/**
 * geofence_manager.h - Geofence management
 *
 * Manages geofence parameters and polygon data for ArduPilot.
 * ArduPilot stores geofence as mission items (FENCE_TYPE_POLYGON).
 *
 * Features:
 *   - Add/remove geofence points
 *   - Validate fence polygon
 *   - Calculate fence area
 *   - Check if a point is inside the fence
 *   - Export/import fence as mission items
 */
#pragma once

#include "core/mission/mission_types.h"
#include "core/platform/platform_mutex.h"
#include <vector>
#include <string>
#include <limits>

namespace mg::core::geofence {

/// A single geofence point (lat/lon in degrees)
struct FencePoint {
    double lat = 0.0;  // degrees
    double lon = 0.0;  // degrees
    float alt = 0.0f;  // meters (for inclusion zone ceiling)
};

/// Geofence configuration
struct FenceConfig {
    bool enabled = false;
    float max_altitude = 120.0f;    // meters
    float min_altitude = 0.0f;      // meters
    uint8_t action = 0;             // 0=RTL, 1=Land, 2=SmartRTL
    bool return_at_min = false;     // Return at minimum altitude
    float breach_distance = 10.0f;  // meters before action
};

/// Geofence validation result
struct FenceValidation {
    bool valid = true;
    std::string error;
    std::vector<std::string> warnings;
    double area_m2 = 0.0;           // Area in square meters
    double perimeter_m = 0.0;       // Perimeter in meters
};

class GeofenceManager {
public:
    GeofenceManager();
    ~GeofenceManager();

    // ================================================================
    // Fence point management
    // ================================================================

    /// Add a fence point
    void add_point(double lat, double lon, float alt = 0.0f);

    /// Remove a fence point by index
    void remove_point(size_t index);

    /// Update a fence point
    void update_point(size_t index, double lat, double lon, float alt = 0.0f);

    /// Clear all fence points
    void clear_points();

    /// Get all fence points
    std::vector<FencePoint> get_points() const;

    /// Get fence point count
    size_t point_count() const;

    // ================================================================
    // Fence configuration
    // ================================================================

    /// Get/set fence configuration
    FenceConfig get_config() const;
    void set_config(const FenceConfig& config);

    // ================================================================
    // Validation
    // ================================================================

    /// Validate the current fence
    FenceValidation validate() const;

    /// Check if a point is inside the fence (ray casting algorithm)
    bool is_inside_fence(double lat, double lon) const;

    /// Calculate distance from a point to the nearest fence edge
    double distance_to_fence(double lat, double lon) const;

    // ================================================================
    // Area/Perimeter calculation
    // ================================================================

    /// Calculate fence area using Shoelace formula (square meters)
    double calculate_area() const;

    /// Calculate fence perimeter (meters)
    double calculate_perimeter() const;

    // ================================================================
    // Mission item conversion
    // ================================================================

    /// Convert fence points to mission items (for upload to FC)
    std::vector<mg::core::mission::MissionItem> to_mission_items() const;

    /// Load fence from mission items (download from FC)
    void from_mission_items(const std::vector<mg::core::mission::MissionItem>& items);

    // ================================================================
    // Import/Export
    // ================================================================

    /// Export fence to JSON
    std::string export_json() const;

    /// Import fence from JSON
    bool import_json(const std::string& json);

    /// Export fence to KML (for Google Earth)
    std::string export_kml(const std::string& name = "Geofence") const;

private:
    mutable mg::core::platform::Mutex mtx_;
    std::vector<FencePoint> points_;
    FenceConfig config_;

    /// Haversine distance (meters)
    static double haversine(double lat1, double lon1, double lat2, double lon2);

    /// Shoelace formula for polygon area (square meters)
    static double shoelace_area(const std::vector<FencePoint>& pts);

    /// Ray casting algorithm for point-in-polygon
    static bool ray_cast(double lat, double lon, const std::vector<FencePoint>& pts);

    /// Point-to-line-segment distance (meters, approximate Cartesian)
    static double point_to_segment_distance(double lat, double lon,
                                            double seg_lat1, double seg_lon1,
                                            double seg_lat2, double seg_lon2);
};

} // namespace mg::core::geofence
