/**
 * mission_item_factory.cc - Implementations for the MissionItem builders
 */

// Ensure M_PI is defined on MSVC/MinGW
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "core/mission/mission_item_factory.h"

#include <cmath>
#include <limits>

namespace mg::core::mission {

static constexpr double kEarthRadiusM = 6371000.0; // Earth radius in meters
static constexpr double kDegToRad = M_PI / 180.0;

MissionItem MissionItemFactory::create_waypoint(double lat_deg, double lon_deg, float alt_m,
                                                float hold_time_s, float accept_radius_m) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    item.param1 = hold_time_s;
    item.param2 = accept_radius_m;
    item.param3 = 0.0f;  // passes through
    item.param4 = std::numeric_limits<float>::quiet_NaN(); // yaw angle
    item.x = static_cast<int32_t>(lat_deg * 1e7);
    item.y = static_cast<int32_t>(lon_deg * 1e7);
    item.z = alt_m;
    item.autocontinue = 1;
    return item;
}

MissionItem MissionItemFactory::create_rtl() {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
    item.autocontinue = 0;
    return item;
}

MissionItem MissionItemFactory::create_speed_change(float speed_ms) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    item.param1 = 0.0f;       // Speed type: 0 = airspeed, 1 = groundspeed
    item.param2 = speed_ms;   // Speed in m/s (MAV_CMD_DO_CHANGE_SPEED param2)
    item.autocontinue = 1;
    return item;
}

MissionItem MissionItemFactory::create_camera_trigger(float distance_m) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::DoSetCamTrigDist);
    item.param1 = distance_m;
    item.autocontinue = 1;
    return item;
}

double MissionItemFactory::haversine_distance(double lat1_deg, double lon1_deg,
                                              double lat2_deg, double lon2_deg) {
    double lat1 = lat1_deg * kDegToRad;
    double lat2 = lat2_deg * kDegToRad;
    double dlat = (lat2 - lat1);
    double dlon = (lon2_deg - lon1_deg) * kDegToRad;

    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1) * std::cos(lat2) *
               std::sin(dlon / 2) * std::sin(dlon / 2);
    double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));

    return kEarthRadiusM * c;
}

} // namespace mg::core::mission
