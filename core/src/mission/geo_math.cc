/**
 * geo_math.cc - 任务相关的地理计算实现
 */

// Ensure M_PI is defined on MSVC/MinGW
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "core/mission/geo_math.h"

#include <cmath>

namespace mg::core::mission {

static constexpr double kEarthRadiusM = 6371000.0; // Earth radius in meters
static constexpr double kDegToRad = M_PI / 180.0;

double haversine_distance(double lat1_deg, double lon1_deg,
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
