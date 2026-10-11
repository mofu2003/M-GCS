/**
 * mission_item_factory.h - Small builders for compiled MissionItem values
 *
 * These helpers construct individual MISSION_ITEM_INT entries plus the
 * Haversine distance used by the survey and corridor generators.
 *
 * They replace the old MissionManager class, whose editing, validation,
 * statistics and JSON/CSV import/export duties are owned by the
 * MissionDocument under core/mission/cad instead.
 */
#pragma once

#include "core/mission/mission_types.h"

namespace mg::core::mission {

struct MissionItemFactory {
    /// Create a NAV_WAYPOINT item
    static MissionItem create_waypoint(double lat_deg, double lon_deg, float alt_m,
                                       float hold_time_s = 0.0f,
                                       float accept_radius_m = 2.0f);

    /// Create a RTL item
    static MissionItem create_rtl();

    /// Create a DO_SET_SPEED item
    static MissionItem create_speed_change(float speed_ms);

    /// Create a DO_SET_CAM_TRIGG_DIST item
    static MissionItem create_camera_trigger(float distance_m);

    /// Distance between two lat/lon points (Haversine formula), in meters
    static double haversine_distance(double lat1_deg, double lon1_deg,
                                     double lat2_deg, double lon2_deg);
};

} // namespace mg::core::mission
