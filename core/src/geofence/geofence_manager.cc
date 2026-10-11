/**
 * geofence_manager.cc - Geofence management implementation
 * 地理围栏管理 — 多边形/圆形围栏判定、JSON 序列化
 */
#include "core/geofence/geofence_manager.h"
#include "core/log/logging.h"
#include <cmath>
#include <algorithm>
#include <sstream>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

static constexpr double kEarthRadiusM = 6371000.0;
static constexpr double kDegToRad = M_PI / 180.0;

namespace mg::core::geofence {

GeofenceManager::GeofenceManager() = default;
GeofenceManager::~GeofenceManager() = default;

// ================================================================
// Point management
// ================================================================

void GeofenceManager::add_point(double lat, double lon, float alt) {
    mg::core::platform::LockGuard lk(mtx_);
    points_.push_back({lat, lon, alt});
}

void GeofenceManager::remove_point(size_t index) {
    mg::core::platform::LockGuard lk(mtx_);
    if (index < points_.size()) {
        points_.erase(points_.begin() + index);
    }
}

void GeofenceManager::update_point(size_t index, double lat, double lon, float alt) {
    mg::core::platform::LockGuard lk(mtx_);
    if (index < points_.size()) {
        points_[index] = {lat, lon, alt};
    }
}

void GeofenceManager::clear_points() {
    mg::core::platform::LockGuard lk(mtx_);
    points_.clear();
}

std::vector<FencePoint> GeofenceManager::get_points() const {
    mg::core::platform::LockGuard lk(mtx_);
    return points_;
}

size_t GeofenceManager::point_count() const {
    mg::core::platform::LockGuard lk(mtx_);
    return points_.size();
}

// ================================================================
// Configuration
// ================================================================

FenceConfig GeofenceManager::get_config() const {
    mg::core::platform::LockGuard lk(mtx_);
    return config_;
}

void GeofenceManager::set_config(const FenceConfig& config) {
    mg::core::platform::LockGuard lk(mtx_);
    config_ = config;
}

// ================================================================
// Validation
// ================================================================

FenceValidation GeofenceManager::validate() const {
    FenceValidation result;
    std::vector<FencePoint> snapshot;

    {
        mg::core::platform::LockGuard lk(mtx_);
        snapshot = points_;
    }

    if (snapshot.size() < 3) {
        result.valid = false;
        result.error = "Need at least 3 points for a fence";
        return result;
    }

    if (snapshot.size() > 200) {
        result.valid = false;
        result.error = "Too many points (max 200)";
        return result;
    }

    // Check for degenerate polygon (all points collinear)
    // Simple check: verify that not all points are on the same line
    bool all_collinear = true;
    if (snapshot.size() >= 3) {
        double lat0 = snapshot[0].lat, lon0 = snapshot[0].lon;
        double lat1 = snapshot[1].lat, lon1 = snapshot[1].lon;
        for (size_t i = 2; i < snapshot.size(); i++) {
            double cross = (snapshot[i].lat - lat0) * (lon1 - lon0) -
                          (snapshot[i].lon - lon0) * (lat1 - lat0);
            if (std::abs(cross) > 1e-10) {
                all_collinear = false;
                break;
            }
        }
    }

    if (all_collinear) {
        result.valid = false;
        result.error = "All points are collinear (not a valid polygon)";
        return result;
    }

    // Check for self-intersection (simplified: check consecutive segments)
    for (size_t i = 0; i < snapshot.size(); i++) {
        size_t j = (i + 1) % snapshot.size();
        double dist = haversine(snapshot[i].lat, snapshot[i].lon,
                                snapshot[j].lat, snapshot[j].lon);
        if (dist < 1.0) {
            result.warnings.push_back(
                "Points " + std::to_string(i) + " and " + std::to_string(j) +
                " are very close (" + std::to_string(dist) + "m)");
        }
    }

    // Calculate area and perimeter using the snapshot (no lock needed, helpers are static)
    result.area_m2 = shoelace_area(snapshot);
    if (snapshot.size() >= 2) {
        double perimeter = 0.0;
        for (size_t i = 0; i < snapshot.size(); i++) {
            size_t j = (i + 1) % snapshot.size();
            perimeter += haversine(snapshot[i].lat, snapshot[i].lon,
                                   snapshot[j].lat, snapshot[j].lon);
        }
        result.perimeter_m = perimeter;
    }

    if (result.area_m2 < 100.0) {
        result.warnings.push_back("Fence area is very small (< 100m²)");
    }

    return result;
}

bool GeofenceManager::is_inside_fence(double lat, double lon) const {
    mg::core::platform::LockGuard lk(mtx_);
    return ray_cast(lat, lon, points_);
}

double GeofenceManager::distance_to_fence(double lat, double lon) const {
    mg::core::platform::LockGuard lk(mtx_);
    if (points_.size() < 2) return std::numeric_limits<double>::max();

    double min_dist = std::numeric_limits<double>::max();

    for (size_t i = 0; i < points_.size(); i++) {
        size_t j = (i + 1) % points_.size();
        // Point-to-line-segment distance
        double seg_dist = point_to_segment_distance(lat, lon,
            points_[i].lat, points_[i].lon,
            points_[j].lat, points_[j].lon);
        if (seg_dist < min_dist) min_dist = seg_dist;
    }

    return min_dist;
}

// ================================================================
// Area/Perimeter
// ================================================================

double GeofenceManager::calculate_area() const {
    mg::core::platform::LockGuard lk(mtx_);
    return shoelace_area(points_);
}

double GeofenceManager::calculate_perimeter() const {
    mg::core::platform::LockGuard lk(mtx_);
    if (points_.size() < 2) return 0.0;

    double perimeter = 0.0;
    for (size_t i = 0; i < points_.size(); i++) {
        size_t j = (i + 1) % points_.size();
        perimeter += haversine(points_[i].lat, points_[i].lon,
                               points_[j].lat, points_[j].lon);
    }
    return perimeter;
}

// ================================================================
// Mission item conversion
// ================================================================

std::vector<mg::core::mission::MissionItem> GeofenceManager::to_mission_items() const {
    mg::core::platform::LockGuard lk(mtx_);
    std::vector<mg::core::mission::MissionItem> items;

    // Return point (first item)
    if (!points_.empty()) {
        mg::core::mission::MissionItem ret_point{};
        ret_point.frame = 3; // GlobalRelAlt
        ret_point.command = 5000; // MAV_CMD_NAV_FENCE_RETURN_POINT
        ret_point.x = static_cast<int32_t>(points_[0].lat * 1e7);
        ret_point.y = static_cast<int32_t>(points_[0].lon * 1e7);
        ret_point.z = points_[0].alt;
        items.push_back(ret_point);
    }

    // Polygon vertices
    for (const auto& pt : points_) {
        mg::core::mission::MissionItem item{};
        item.frame = 3; // GlobalRelAlt
        item.command = 5001; // MAV_CMD_NAV_FENCE_POLYGON_VERTEX_INCLUSION
        item.param1 = 0; // Circle radius (0 = polygon)
        item.x = static_cast<int32_t>(pt.lat * 1e7);
        item.y = static_cast<int32_t>(pt.lon * 1e7);
        item.z = pt.alt;
        items.push_back(item);
    }

    return items;
}

void GeofenceManager::from_mission_items(const std::vector<mg::core::mission::MissionItem>& items) {
    mg::core::platform::LockGuard lk(mtx_);
    points_.clear();

    for (const auto& item : items) {
        if (item.command == 5001) { // FENCE_POLYGON_VERTEX_INCLUSION
            FencePoint pt;
            pt.lat = item.x / 1e7;
            pt.lon = item.y / 1e7;
            pt.alt = item.z;
            points_.push_back(pt);
        }
    }
}

// ================================================================
// Import/Export
// ================================================================

std::string GeofenceManager::export_json() const {
    mg::core::platform::LockGuard lk(mtx_);
    cJSON* root = cJSON_CreateObject();

    // Config
    cJSON* config = cJSON_AddObjectToObject(root, "config");
    cJSON_AddBoolToObject(config, "enabled", config_.enabled);
    cJSON_AddNumberToObject(config, "max_altitude", config_.max_altitude);
    cJSON_AddNumberToObject(config, "min_altitude", config_.min_altitude);
    cJSON_AddNumberToObject(config, "action", config_.action);
    cJSON_AddBoolToObject(config, "return_at_min", config_.return_at_min);
    cJSON_AddNumberToObject(config, "breach_distance", config_.breach_distance);

    // Points
    cJSON* pts = cJSON_AddArrayToObject(root, "points");
    for (const auto& pt : points_) {
        cJSON* p = cJSON_CreateObject();
        cJSON_AddNumberToObject(p, "lat", pt.lat);
        cJSON_AddNumberToObject(p, "lon", pt.lon);
        cJSON_AddNumberToObject(p, "alt", pt.alt);
        cJSON_AddItemToArray(pts, p);
    }

    // Stats (use static helpers directly to avoid re-acquiring the lock)
    cJSON_AddNumberToObject(root, "area_m2", shoelace_area(points_));
    double perimeter = 0.0;
    if (points_.size() >= 2) {
        for (size_t i = 0; i < points_.size(); i++) {
            size_t j = (i + 1) % points_.size();
            perimeter += haversine(points_[i].lat, points_[i].lon,
                                   points_[j].lat, points_[j].lon);
        }
    }
    cJSON_AddNumberToObject(root, "perimeter_m", perimeter);
    cJSON_AddNumberToObject(root, "point_count", points_.size());

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);

    return result;
}

bool GeofenceManager::import_json(const std::string& json) {
    mg::core::platform::LockGuard lk(mtx_);
    cJSON* root = mg::core::json::parse(json.c_str());
    if (!root) return false;

    // Parse config
    cJSON* config = cJSON_GetObjectItem(root, "config");
    if (config) {
        cJSON* enabled = cJSON_GetObjectItem(config, "enabled");
        cJSON* max_alt = cJSON_GetObjectItem(config, "max_altitude");
        cJSON* min_alt = cJSON_GetObjectItem(config, "min_altitude");
        cJSON* action = cJSON_GetObjectItem(config, "action");
        cJSON* return_min = cJSON_GetObjectItem(config, "return_at_min");
        cJSON* breach = cJSON_GetObjectItem(config, "breach_distance");

        if (enabled && cJSON_IsBool(enabled)) config_.enabled = cJSON_IsTrue(enabled);
        if (max_alt && cJSON_IsNumber(max_alt)) config_.max_altitude = static_cast<float>(max_alt->valuedouble);
        if (min_alt && cJSON_IsNumber(min_alt)) config_.min_altitude = static_cast<float>(min_alt->valuedouble);
        if (action && cJSON_IsNumber(action)) config_.action = static_cast<uint8_t>(action->valueint);
        if (return_min && cJSON_IsBool(return_min)) config_.return_at_min = cJSON_IsTrue(return_min);
        if (breach && cJSON_IsNumber(breach)) config_.breach_distance = static_cast<float>(breach->valuedouble);
    }

    // Parse points
    cJSON* pts = cJSON_GetObjectItem(root, "points");
    if (pts && cJSON_IsArray(pts)) {
        points_.clear();
        cJSON* p = nullptr;
        cJSON_ArrayForEach(p, pts) {
            FencePoint pt;
            cJSON* lat = cJSON_GetObjectItem(p, "lat");
            cJSON* lon = cJSON_GetObjectItem(p, "lon");
            cJSON* alt = cJSON_GetObjectItem(p, "alt");
            if (lat && cJSON_IsNumber(lat)) pt.lat = lat->valuedouble;
            if (lon && cJSON_IsNumber(lon)) pt.lon = lon->valuedouble;
            if (alt && cJSON_IsNumber(alt)) pt.alt = static_cast<float>(alt->valuedouble);
            points_.push_back(pt);
        }
    }

    cJSON_Delete(root);
    return true;
}

std::string GeofenceManager::export_kml(const std::string& name) const {
    mg::core::platform::LockGuard lk(mtx_);
    std::ostringstream oss;
    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    oss << "<Document>\n";
    oss << "  <name>" << name << "</name>\n";
    oss << "  <Style id=\"geofence\">\n";
    oss << "    <LineStyle><color>ff0000ff</color><width>2</width></LineStyle>\n";
    oss << "    <PolyStyle><color>330000ff</color></PolyStyle>\n";
    oss << "  </Style>\n";
    oss << "  <Placemark>\n";
    oss << "    <styleUrl>#geofence</styleUrl>\n";
    oss << "    <Polygon>\n";
    oss << "      <outerBoundaryIs>\n";
    oss << "        <LinearRing>\n";
    oss << "          <coordinates>\n";

    for (const auto& pt : points_) {
        oss << "            " << pt.lon << "," << pt.lat << "," << pt.alt << "\n";
    }
    if (!points_.empty()) {
        oss << "            " << points_[0].lon << "," << points_[0].lat << "," << points_[0].alt << "\n";
    }

    oss << "          </coordinates>\n";
    oss << "        </LinearRing>\n";
    oss << "      </outerBoundaryIs>\n";
    oss << "    </Polygon>\n";
    oss << "  </Placemark>\n";
    oss << "</Document>\n";
    oss << "</kml>\n";

    return oss.str();
}

// ================================================================
// Private helpers
// ================================================================

double GeofenceManager::haversine(double lat1, double lon1, double lat2, double lon2) {
    double dlat = (lat2 - lat1) * kDegToRad;
    double dlon = (lon2 - lon1) * kDegToRad;
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1 * kDegToRad) * std::cos(lat2 * kDegToRad) *
               std::sin(dlon / 2) * std::sin(dlon / 2);
    return kEarthRadiusM * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
}

double GeofenceManager::shoelace_area(const std::vector<FencePoint>& pts) {
    if (pts.size() < 3) return 0.0;

    double area = 0.0;
    for (size_t i = 0; i < pts.size(); i++) {
        size_t j = (i + 1) % pts.size();
        area += pts[i].lon * pts[j].lat;
        area -= pts[j].lon * pts[i].lat;
    }
    area = std::abs(area) / 2.0;

    // Convert from degree² to m² (approximate)
    double avg_lat = 0.0;
    for (const auto& pt : pts) avg_lat += pt.lat;
    avg_lat /= pts.size();
    double cos_lat = std::cos(avg_lat * kDegToRad);

    double m2_per_deg_lon = kEarthRadiusM * cos_lat * kDegToRad;
    double m2_per_deg_lat = kEarthRadiusM * kDegToRad;

    return area * m2_per_deg_lon * m2_per_deg_lat;
}

bool GeofenceManager::ray_cast(double lat, double lon, const std::vector<FencePoint>& pts) {
    if (pts.size() < 3) return false;

    bool inside = false;
    for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
        if ((pts[i].lat > lat) != (pts[j].lat > lat) &&
            lon < (pts[j].lon - pts[i].lon) * (lat - pts[i].lat) /
                  (pts[j].lat - pts[i].lat) + pts[i].lon) {
            inside = !inside;
        }
    }
    return inside;
}

double GeofenceManager::point_to_segment_distance(double lat, double lon,
                                                   double seg_lat1, double seg_lon1,
                                                   double seg_lat2, double seg_lon2) {
    // Project point onto segment in Cartesian (meters) approximation
    double lat_rad = lat * kDegToRad;
    double cos_lat = std::cos(lat_rad);
    double m_per_deg_lon = kEarthRadiusM * cos_lat * kDegToRad;
    double m_per_deg_lat = kEarthRadiusM * kDegToRad;

    // Convert to meters relative to point
    double ax = (seg_lon1 - lon) * m_per_deg_lon;
    double ay = (seg_lat1 - lat) * m_per_deg_lat;
    double bx = (seg_lon2 - lon) * m_per_deg_lon;
    double by = (seg_lat2 - lat) * m_per_deg_lat;

    // Vector AB
    double dx = bx - ax;
    double dy = by - ay;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-20) {
        // Degenerate segment — return distance to the single point
        return std::sqrt(ax * ax + ay * ay);
    }

    // Project point (origin) onto segment AB
    // t = -dot(A, AB) / |AB|^2  (since P = origin)
    double t = -(ax * dx + ay * dy) / len_sq;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    // Closest point on segment
    double closest_x = ax + t * dx;
    double closest_y = ay + t * dy;

    return std::sqrt(closest_x * closest_x + closest_y * closest_y);
}

} // namespace mg::core::geofence
