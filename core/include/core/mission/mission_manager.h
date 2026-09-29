/**
 * mission_manager.h - Mission management with editing, validation, and persistence
 *
 * Legacy compiled-plan management that provides:
 *   - Waypoint editing (add, remove, update, reorder)
 *   - Mission validation (altitude, distance, command constraints)
 *   - Mission statistics (total distance, estimated time, etc.)
 *   - Waypoint templates (common mission patterns)
 *   - Thread-safe access to mission state
 *
 * This sits above the raw MAVLink mission protocol in MavlinkAdapter.
 * It manages the compiled MissionPlan and remains available for compatibility,
 * tests, and low-level tooling. New frontend editing should use the
 * MissionDocument under core/mission/cad instead; mission_doc_apply owns
 * offline edits, revision, undo/redo, validation and compilation.
 *
 * Design: inspired by QGC MissionController + MissionItemTableModel.
 */
#pragma once

#include "core/mission/mission_types.h"
#include "core/platform/platform_mutex.h"
#include <functional>
#include <string>
#include <vector>
#include <cmath>

namespace mg::core::mission {

/// Mission validation result
struct MissionValidation {
    bool valid = true;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;

    void add_warning(const std::string& msg) { warnings.push_back(msg); }
    void add_error(const std::string& msg) { errors.push_back(msg); valid = false; }
};

/// Mission statistics
struct MissionStats {
    double total_distance_m = 0.0;      // Total flight distance (meters)
    double total_altitude_change_m = 0.0; // Total altitude gain/loss
    double estimated_time_s = 0.0;      // Estimated flight time (seconds)
    double min_altitude_m = 0.0;        // Minimum altitude
    double max_altitude_m = 0.0;        // Maximum altitude
    double home_lat = 0.0;              // Home position (degrees)
    double home_lon = 0.0;
    size_t waypoint_count = 0;          // Number of NAV_WAYPOINT items
    size_t total_item_count = 0;        // Total items (including DO_* commands)
};

/// Waypoint template for common patterns
struct WaypointTemplate {
    std::string name;
    std::string description;
    std::vector<MissionItem> items;
};

/// Callback for mission changes
using MissionChangedCallback = std::function<void()>;

class MissionManager {
public:
    MissionManager();
    ~MissionManager();

    // Non-copyable
    MissionManager(const MissionManager&) = delete;
    MissionManager& operator=(const MissionManager&) = delete;

    // ================================================================
    // Mission plan access (thread-safe)
    // ================================================================

    /// Get a copy of the current mission plan
    MissionPlan get_plan() const;

    /// Set the mission plan (replaces current)
    void set_plan(const MissionPlan& plan);

    /// Clear all mission items
    void clear();

    /// Check if mission is valid (non-empty, has at least one takeoff or waypoint)
    bool is_valid() const;

    /// Get item count
    size_t item_count() const;

    // ================================================================
    // Waypoint editing
    // ================================================================

    /// Add a waypoint at the end
    void add_item(const MissionItem& item);

    /// Insert a waypoint at index
    void insert_item(size_t index, const MissionItem& item);

    /// Update an existing waypoint
    void update_item(size_t index, const MissionItem& item);

    /// Remove a waypoint by index
    void remove_item(size_t index);

    /// Move a waypoint from one index to another
    void move_item(size_t from_index, size_t to_index);

    /// Re-sequence all items (0-based contiguous numbering)
    void resequence();

    // ================================================================
    // Waypoint factory methods
    // ================================================================

    /// Create a NAV_WAYPOINT item
    static MissionItem create_waypoint(double lat_deg, double lon_deg, float alt_m,
                                        float hold_time_s = 0.0f, float accept_radius_m = 2.0f);

    /// Create a TAKEOFF item
    static MissionItem create_takeoff(float alt_m, float pitch_deg = 15.0f);

    /// Create a LAND item
    static MissionItem create_land(double lat_deg, double lon_deg);

    /// Create a RTL item
    static MissionItem create_rtl();

    /// Create a DO_SET_SPEED item
    static MissionItem create_speed_change(float speed_ms);

    /// Create a DO_SET_CAM_TRIGG_DIST item
    static MissionItem create_camera_trigger(float distance_m);

    /// Create a LOITER_UNLIM item (circle at current position)
    static MissionItem create_loiter_unlim(double lat_deg, double lon_deg, float alt_m,
                                           float radius_m = 50.0f);

    /// Create a LOITER_TURNS item
    static MissionItem create_loiter_turns(double lat_deg, double lon_deg, float alt_m,
                                           uint8_t turns = 3, float radius_m = 50.0f);

    // ================================================================
    // Mission validation
    // ================================================================

    /// Validate the current mission
    MissionValidation validate() const;

    /// Validate a single item
    static MissionValidation validate_item(const MissionItem& item, size_t index);

    // ================================================================
    // Mission statistics
    // ================================================================

    /// Calculate mission statistics
    MissionStats calculate_stats() const;

    // ================================================================
    // Distance calculation helpers
    // ================================================================

    /// Calculate distance between two lat/lon points (Haversine formula)
    static double haversine_distance(double lat1_deg, double lon1_deg,
                                      double lat2_deg, double lon2_deg);

    /// Calculate bearing between two points (degrees)
    static double calculate_bearing(double lat1_deg, double lon1_deg,
                                     double lat2_deg, double lon2_deg);

    // ================================================================
    // Waypoint templates
    // ================================================================

    /// Get available templates
    static std::vector<WaypointTemplate> get_templates();

    /// Create a simple survey pattern (grid)
    static MissionItem create_survey_grid(double center_lat, double center_lon,
                                           double width_m, double length_m,
                                           float alt_m, double spacing_m = 30.0f);

    // ================================================================
    // Import/Export (QGC .plan format compatible)
    // ================================================================

    /// Export mission to JSON string (QGC .plan format)
    std::string export_json() const;

    /// Import mission from JSON string
    bool import_json(const std::string& json);

    /// Export to simple CSV format (for basic tools)
    std::string export_csv() const;

    /// Import from simple CSV format
    bool import_csv(const std::string& csv);

    // ================================================================
    // Callbacks
    // ================================================================

    /// Set callback for mission changes
    void set_changed_callback(MissionChangedCallback cb);

private:
    mutable mg::core::platform::Mutex mtx_;
    MissionPlan plan_;
    MissionChangedCallback changed_cb_;

    void notify_changed();
    void ensure_sequential();
};

} // namespace mg::core::mission
