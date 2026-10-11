/**
 * mission_manager.cc - Mission management implementation
 *
 * Provides high-level mission editing, validation, statistics,
 * and import/export functionality.
 */

// Ensure M_PI is defined on MSVC/MinGW
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include "core/mission/mission_manager.h"
#include "core/log/logging.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>

// cJSON for JSON import/export
#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

namespace mg::core::mission {

static constexpr double kEarthRadiusM = 6371000.0; // Earth radius in meters
static constexpr double kDegToRad = M_PI / 180.0;
static constexpr double kRadToDeg = 180.0 / M_PI;
static constexpr float kDefaultCruiseSpeed = 15.0f;  // m/s

// ================================================================
// Constructor / Destructor
// ================================================================

MissionManager::MissionManager() = default;
MissionManager::~MissionManager() = default;

// ================================================================
// Mission plan access
// ================================================================

MissionPlan MissionManager::get_plan() const {
    mg::core::platform::LockGuard lk(mtx_);
    return plan_;
}

void MissionManager::set_plan(const MissionPlan& plan) {
    {
        mg::core::platform::LockGuard lk(mtx_);
        plan_ = plan;
        ensure_sequential();
    }
    notify_changed();
}

void MissionManager::clear() {
    {
        mg::core::platform::LockGuard lk(mtx_);
        plan_.clear();
    }
    notify_changed();
}

bool MissionManager::is_valid() const {
    mg::core::platform::LockGuard lk(mtx_);
    if (plan_.items.empty()) return false;
    // Must have at least one navigation command
    for (const auto& item : plan_.items) {
        if (item.command >= 16 && item.command <= 22) return true; // NAV_* range
    }
    return false;
}

size_t MissionManager::item_count() const {
    mg::core::platform::LockGuard lk(mtx_);
    return plan_.items.size();
}

// ================================================================
// Waypoint editing
// ================================================================

void MissionManager::add_item(const MissionItem& item) {
    {
        mg::core::platform::LockGuard lk(mtx_);
        MissionItem new_item = item;
        new_item.seq = static_cast<uint16_t>(plan_.items.size());
        plan_.items.push_back(new_item);
    }
    notify_changed();
}

void MissionManager::insert_item(size_t index, const MissionItem& item) {
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (index > plan_.items.size()) return;
        MissionItem new_item = item;
        plan_.items.insert(plan_.items.begin() + index, new_item);
        ensure_sequential();
    }
    notify_changed();
}

void MissionManager::update_item(size_t index, const MissionItem& item) {
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (index >= plan_.items.size()) return;
        plan_.items[index] = item;
        plan_.items[index].seq = static_cast<uint16_t>(index);
    }
    notify_changed();
}

void MissionManager::remove_item(size_t index) {
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (index >= plan_.items.size()) return;
        plan_.items.erase(plan_.items.begin() + index);
        ensure_sequential();
    }
    notify_changed();
}

void MissionManager::move_item(size_t from_index, size_t to_index) {
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (from_index >= plan_.items.size() || to_index >= plan_.items.size()) return;
        if (from_index == to_index) return;

        MissionItem item = plan_.items[from_index];
        plan_.items.erase(plan_.items.begin() + from_index);
        plan_.items.insert(plan_.items.begin() + to_index, item);
        ensure_sequential();
    }
    notify_changed();
}

void MissionManager::resequence() {
    {
        mg::core::platform::LockGuard lk(mtx_);
        ensure_sequential();
    }
    notify_changed();
}

void MissionManager::ensure_sequential() {
    // Must hold mtx_
    for (size_t i = 0; i < plan_.items.size(); i++) {
        plan_.items[i].seq = static_cast<uint16_t>(i);
    }
}

// ================================================================
// Waypoint factory methods
// ================================================================

MissionItem MissionManager::create_waypoint(double lat_deg, double lon_deg, float alt_m,
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

MissionItem MissionManager::create_takeoff(float alt_m, float pitch_deg) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavTakeoff);
    item.param1 = pitch_deg;
    item.param4 = std::numeric_limits<float>::quiet_NaN();
    item.z = alt_m;
    item.autocontinue = 1;
    return item;
}

MissionItem MissionManager::create_land(double lat_deg, double lon_deg) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavLand);
    item.x = static_cast<int32_t>(lat_deg * 1e7);
    item.y = static_cast<int32_t>(lon_deg * 1e7);
    item.autocontinue = 0; // Don't auto-continue after land
    return item;
}

MissionItem MissionManager::create_rtl() {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
    item.autocontinue = 0;
    return item;
}

MissionItem MissionManager::create_speed_change(float speed_ms) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    item.param1 = speed_ms;   // Speed in m/s
    item.param2 = 0.0f;       // 0 = airspeed, 1 = groundspeed
    item.autocontinue = 1;
    return item;
}

MissionItem MissionManager::create_camera_trigger(float distance_m) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::DoSetCamTrigDist);
    item.param1 = distance_m;
    item.autocontinue = 1;
    return item;
}

MissionItem MissionManager::create_loiter_unlim(double lat_deg, double lon_deg, float alt_m,
                                                 float radius_m) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavLoiterUnlim);
    item.param3 = radius_m;  // Radius in meters (positive = clockwise)
    item.x = static_cast<int32_t>(lat_deg * 1e7);
    item.y = static_cast<int32_t>(lon_deg * 1e7);
    item.z = alt_m;
    item.autocontinue = 0;
    return item;
}

MissionItem MissionManager::create_loiter_turns(double lat_deg, double lon_deg, float alt_m,
                                                 uint8_t turns, float radius_m) {
    MissionItem item{};
    item.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(NavCommand::NavLoiterTurns);
    item.param1 = static_cast<float>(turns);  // Number of turns
    item.param3 = radius_m;                    // Radius in meters
    item.x = static_cast<int32_t>(lat_deg * 1e7);
    item.y = static_cast<int32_t>(lon_deg * 1e7);
    item.z = alt_m;
    item.autocontinue = 1;
    return item;
}

// ================================================================
// Mission validation
// ================================================================

MissionValidation MissionManager::validate() const {
    mg::core::platform::LockGuard lk(mtx_);
    MissionValidation result;

    if (plan_.items.empty()) {
        result.add_error("Mission is empty");
        return result;
    }

    // Check each item
    for (size_t i = 0; i < plan_.items.size(); i++) {
        auto item_result = validate_item(plan_.items[i], i);
        result.warnings.insert(result.warnings.end(),
                               item_result.warnings.begin(), item_result.warnings.end());
        result.errors.insert(result.errors.end(),
                             item_result.errors.begin(), item_result.errors.end());
    }

    // Check sequence numbers
    for (size_t i = 0; i < plan_.items.size(); i++) {
        if (plan_.items[i].seq != i) {
            result.add_warning("Item " + std::to_string(i) + " has wrong sequence number");
        }
    }

    // Check for takeoff as first item (recommended)
    if (!plan_.items.empty()) {
        bool first_is_takeoff =
            (plan_.items[0].command == static_cast<uint16_t>(NavCommand::NavTakeoff) ||
             plan_.items[0].command == static_cast<uint16_t>(NavCommand::NavVtolTakeoff));
        bool first_is_waypoint = (plan_.items[0].command == static_cast<uint16_t>(NavCommand::NavWaypoint));
        if (!first_is_takeoff && !first_is_waypoint) {
            result.add_warning("First item should be TAKEOFF, VTOL_TAKEOFF or WAYPOINT");
        }
    }

    // Check for landing as last item (recommended)
    if (!plan_.items.empty()) {
        bool last_is_land =
            (plan_.items.back().command == static_cast<uint16_t>(NavCommand::NavLand) ||
             plan_.items.back().command == static_cast<uint16_t>(NavCommand::NavVtolLand));
        bool last_is_rtl = (plan_.items.back().command == static_cast<uint16_t>(NavCommand::NavReturnToLaunch));
        if (!last_is_land && !last_is_rtl) {
            result.add_warning("Last item should be LAND, VTOL_LAND or RTL");
        }
    }

    if (result.errors.empty()) {
        result.valid = true;
    }

    return result;
}

MissionValidation MissionManager::validate_item(const MissionItem& item, size_t index) {
    MissionValidation result;
    std::string prefix = "Item " + std::to_string(index) + ": ";

    // Check if position is valid for position-based commands
    bool is_position_cmd = (item.command == static_cast<uint16_t>(NavCommand::NavWaypoint) ||
                           item.command == static_cast<uint16_t>(NavCommand::NavLoiterUnlim) ||
                           item.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns) ||
                           item.command == static_cast<uint16_t>(NavCommand::NavLoiterTime) ||
                           item.command == static_cast<uint16_t>(NavCommand::NavLand) ||
                           item.command == static_cast<uint16_t>(NavCommand::NavVtolTakeoff) ||
                           item.command == static_cast<uint16_t>(NavCommand::NavVtolLand));

    if (is_position_cmd) {
        double lat = item.x / 1e7;
        double lon = item.y / 1e7;
        if (lat < -90.0 || lat > 90.0) {
            result.add_error(prefix + "Invalid latitude: " + std::to_string(lat));
        }
        if (lon < -180.0 || lon > 180.0) {
            result.add_error(prefix + "Invalid longitude: " + std::to_string(lon));
        }
        if (item.z < 0) {
            result.add_warning(prefix + "Negative altitude: " + std::to_string(item.z));
        }
        if (item.z > 500) {
            result.add_warning(prefix + "Very high altitude: " + std::to_string(item.z) + "m");
        }
    }

    // Check loiter radius
    if (item.command == static_cast<uint16_t>(NavCommand::NavLoiterUnlim) ||
        item.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns)) {
        if (item.param3 < 0 || item.param3 > 500) {
            result.add_warning(prefix + "Unusual loiter radius: " + std::to_string(item.param3));
        }
    }

    return result;
}

// ================================================================
// Mission statistics
// ================================================================

MissionStats MissionManager::calculate_stats() const {
    mg::core::platform::LockGuard lk(mtx_);
    MissionStats stats;
    stats.total_item_count = plan_.items.size();

    if (plan_.items.empty()) return stats;

    double prev_lat = 0, prev_lon = 0;
    bool has_prev = false;

    for (const auto& item : plan_.items) {
        // Count navigation waypoints
        if (item.command == static_cast<uint16_t>(NavCommand::NavWaypoint) ||
            item.command == static_cast<uint16_t>(NavCommand::NavLoiterUnlim) ||
            item.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns) ||
            item.command == static_cast<uint16_t>(NavCommand::NavLoiterTime)) {
            stats.waypoint_count++;

            double lat = item.x / 1e7;
            double lon = item.y / 1e7;

            if (has_prev) {
                stats.total_distance_m += haversine_distance(prev_lat, prev_lon, lat, lon);
            }

            prev_lat = lat;
            prev_lon = lon;
            has_prev = true;
        }

        // Track altitude
        if (item.z > 0) {
            if (stats.min_altitude_m == 0 || item.z < stats.min_altitude_m) {
                stats.min_altitude_m = item.z;
            }
            if (item.z > stats.max_altitude_m) {
                stats.max_altitude_m = item.z;
            }
        }
    }

    stats.total_altitude_change_m = stats.max_altitude_m - stats.min_altitude_m;

    // Estimate flight time (assume cruise speed)
    float speed = kDefaultCruiseSpeed;
    stats.estimated_time_s = stats.total_distance_m / speed;
    // Add loiter time estimates
    for (const auto& item : plan_.items) {
        if (item.command == static_cast<uint16_t>(NavCommand::NavLoiterTime)) {
            stats.estimated_time_s += item.param1; // param1 = hold time in seconds
        }
        if (item.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns)) {
            // Estimate time for turns (rough: 10s per turn)
            stats.estimated_time_s += item.param1 * 10.0f;
        }
    }

    return stats;
}

// ================================================================
// Distance calculation helpers
// ================================================================

double MissionManager::haversine_distance(double lat1_deg, double lon1_deg,
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

double MissionManager::calculate_bearing(double lat1_deg, double lon1_deg,
                                          double lat2_deg, double lon2_deg) {
    double lat1 = lat1_deg * kDegToRad;
    double lat2 = lat2_deg * kDegToRad;
    double dlon = (lon2_deg - lon1_deg) * kDegToRad;

    double y = std::sin(dlon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) -
               std::sin(lat1) * std::cos(lat2) * std::cos(dlon);
    double bearing = std::atan2(y, x) * kRadToDeg;

    return std::fmod(bearing + 360.0, 360.0);
}

// ================================================================
// Waypoint templates
// ================================================================

std::vector<WaypointTemplate> MissionManager::get_templates() {
    std::vector<WaypointTemplate> templates;

    // Basic circular survey
    {
        WaypointTemplate t;
        t.name = "Basic Survey";
        t.description = "Simple rectangular survey pattern";
        t.items.push_back(create_takeoff(50.0f));
        t.items.push_back(create_waypoint(0.0, 0.0, 50.0f));
        t.items.push_back(create_waypoint(0.001, 0.0, 50.0f));
        t.items.push_back(create_waypoint(0.001, 0.001, 50.0f));
        t.items.push_back(create_waypoint(0.0, 0.001, 50.0f));
        t.items.push_back(create_rtl());
        templates.push_back(t);
    }

    // Orbit pattern
    {
        WaypointTemplate t;
        t.name = "Orbit";
        t.description = "Circle around a point";
        t.items.push_back(create_takeoff(50.0f));
        t.items.push_back(create_loiter_unlim(0.0, 0.0, 50.0f, 100.0f));
        templates.push_back(t);
    }

    // Mapping pattern
    {
        WaypointTemplate t;
        t.name = "Mapping";
        t.description = "Grid pattern for aerial mapping";
        t.items.push_back(create_takeoff(80.0f));
        t.items.push_back(create_camera_trigger(30.0f));
        t.items.push_back(create_speed_change(10.0f));
        // Grid lines would be added dynamically based on area
        t.items.push_back(create_rtl());
        templates.push_back(t);
    }

    return templates;
}

MissionItem MissionManager::create_survey_grid(double center_lat, double center_lon,
                                                double width_m, double length_m,
                                                float alt_m, double spacing_m) {
    // Create a simple grid pattern
    MissionItem item = create_waypoint(center_lat, center_lon, alt_m);
    // This is a simplified version - full grid would generate multiple waypoints
    return item;
}

// ================================================================
// Import/Export (JSON)
// ================================================================

std::string MissionManager::export_json() const {
    mg::core::platform::LockGuard lk(mtx_);

    cJSON* root = cJSON_CreateObject();
    cJSON* items = cJSON_AddArrayToObject(root, "items");

    for (const auto& item : plan_.items) {
        cJSON* item_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(item_json, "seq", item.seq);
        cJSON_AddNumberToObject(item_json, "frame", item.frame);
        cJSON_AddNumberToObject(item_json, "command", item.command);
        cJSON_AddNumberToObject(item_json, "current", item.current);
        cJSON_AddNumberToObject(item_json, "autocontinue", item.autocontinue);
        cJSON_AddNumberToObject(item_json, "param1", item.param1);
        cJSON_AddNumberToObject(item_json, "param2", item.param2);
        cJSON_AddNumberToObject(item_json, "param3", item.param3);
        cJSON_AddNumberToObject(item_json, "param4", item.param4);
        cJSON_AddNumberToObject(item_json, "x", item.x);
        cJSON_AddNumberToObject(item_json, "y", item.y);
        cJSON_AddNumberToObject(item_json, "z", item.z);
        cJSON_AddNumberToObject(item_json, "mission_type", item.mission_type);

        // Add human-readable fields
        cJSON_AddStringToObject(item_json, "command_name", command_name(item.command));
        cJSON_AddStringToObject(item_json, "frame_name", frame_name(item.frame));

        // Add lat/lon in degrees for convenience
        if (is_global_coordinate_frame(item.frame) &&
            mission_command_uses_location(item.command)) {
            cJSON_AddNumberToObject(item_json, "lat", item.x / 1e7);
            cJSON_AddNumberToObject(item_json, "lon", item.y / 1e7);
        }

        cJSON_AddItemToArray(items, item_json);
    }

    cJSON_AddNumberToObject(root, "count", plan_.items.size());
    cJSON_AddNumberToObject(root, "current_index", plan_.current_index);

    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);

    return result;
}

bool MissionManager::import_json(const std::string& json) {
    cJSON* root = mg::core::json::parse(json.c_str());
    if (!root) return false;

    cJSON* items = cJSON_GetObjectItem(root, "items");
    if (!items || !cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return false;
    }

    MissionPlan new_plan;
    int array_size = cJSON_GetArraySize(items);

    for (int i = 0; i < array_size; i++) {
        cJSON* item_json = cJSON_GetArrayItem(items, i);
        if (!cJSON_IsObject(item_json)) continue;

        MissionItem item{};

        auto get_int = [&](const char* key) -> int {
            cJSON* f = cJSON_GetObjectItemCaseSensitive(item_json, key);
            if (f && cJSON_IsNumber(f)) return f->valueint;
            return 0;
        };
        auto get_float = [&](const char* key) -> float {
            cJSON* f = cJSON_GetObjectItemCaseSensitive(item_json, key);
            if (f && cJSON_IsNumber(f)) return static_cast<float>(f->valuedouble);
            return 0.0f;
        };

        item.seq          = static_cast<uint16_t>(get_int("seq"));
        item.frame        = static_cast<uint8_t>(get_int("frame"));
        item.command      = static_cast<uint16_t>(get_int("command"));
        item.autocontinue = static_cast<uint8_t>(get_int("autocontinue"));
        item.current      = static_cast<uint8_t>(get_int("current"));
        item.x            = static_cast<int32_t>(get_int("x"));
        item.y            = static_cast<int32_t>(get_int("y"));
        item.z            = get_float("z");
        item.param1       = get_float("param1");
        item.param2       = get_float("param2");
        item.param3       = get_float("param3");
        item.param4       = get_float("param4");
        item.mission_type = static_cast<uint8_t>(get_int("mission_type"));

        // Support lat/lon in degrees (convert to degE7)
        cJSON* lat = cJSON_GetObjectItemCaseSensitive(item_json, "lat");
        cJSON* lon = cJSON_GetObjectItemCaseSensitive(item_json, "lon");
        if (lat && cJSON_IsNumber(lat)) item.x = static_cast<int32_t>(lat->valuedouble * 1e7);
        if (lon && cJSON_IsNumber(lon)) item.y = static_cast<int32_t>(lon->valuedouble * 1e7);

        new_plan.items.push_back(item);
    }

    cJSON_Delete(root);

    {
        mg::core::platform::LockGuard lk(mtx_);
        plan_ = new_plan;
        plan_.valid = true;
        ensure_sequential();
    }

    notify_changed();
    return true;
}

// ================================================================
// Import/Export (CSV)
// ================================================================

std::string MissionManager::export_csv() const {
    mg::core::platform::LockGuard lk(mtx_);
    std::ostringstream csv;

    csv << "seq,command,frame,lat,lon,alt,param1,param2,param3,param4,autocontinue\n";

    for (const auto& item : plan_.items) {
        double lat = item.x / 1e7;
        double lon = item.y / 1e7;

        csv << item.seq << ","
            << item.command << ","
            << static_cast<int>(item.frame) << ","
            << std::fixed << std::setprecision(7) << lat << ","
            << std::fixed << std::setprecision(7) << lon << ","
            << std::fixed << std::setprecision(2) << item.z << ","
            << item.param1 << ","
            << item.param2 << ","
            << item.param3 << ","
            << item.param4 << ","
            << static_cast<int>(item.autocontinue) << "\n";
    }

    return csv.str();
}

bool MissionManager::import_csv(const std::string& csv) {
    std::istringstream stream(csv);
    std::string line;
    MissionPlan new_plan;

    // Skip header line
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream line_stream(line);
        std::string token;
        MissionItem item{};
        int field = 0;
        bool parse_ok = true;

        while (std::getline(line_stream, token, ',')) {
            try {
                switch (field) {
                    case 0: item.seq = static_cast<uint16_t>(std::stoi(token)); break;
                    case 1: item.command = static_cast<uint16_t>(std::stoi(token)); break;
                    case 2: item.frame = static_cast<uint8_t>(std::stoi(token)); break;
                    case 3: {
                        double lat = std::stod(token);
                        if (lat < -90.0 || lat > 90.0) break;
                        item.x = static_cast<int32_t>(lat * 1e7);
                        break;
                    }
                    case 4: {
                        double lon = std::stod(token);
                        if (lon < -180.0 || lon > 180.0) break;
                        item.y = static_cast<int32_t>(lon * 1e7);
                        break;
                    }
                    case 5: item.z = std::stof(token); break;
                    case 6: item.param1 = std::stof(token); break;
                    case 7: item.param2 = std::stof(token); break;
                    case 8: item.param3 = std::stof(token); break;
                    case 9: item.param4 = std::stof(token); break;
                    case 10: item.autocontinue = static_cast<uint8_t>(std::stoi(token)); break;
                }
            } catch (const std::exception& e) {
                MGC_LOG_WARN("[Mission] CSV parse error at field %d: %s (token: %s)", field, e.what(), token.c_str());
                // Mark parse failure for critical fields (command and frame)
                if (field <= 2) parse_ok = false;
            }
            field++;
        }

        // Only add items with valid critical fields
        if (parse_ok) {
            new_plan.items.push_back(item);
        }
    }

    if (new_plan.items.empty()) return false;

    {
        mg::core::platform::LockGuard lk(mtx_);
        plan_ = new_plan;
        plan_.valid = true;
        ensure_sequential();
    }

    notify_changed();
    return true;
}

// ================================================================
// Callbacks
// ================================================================

void MissionManager::set_changed_callback(MissionChangedCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    changed_cb_ = std::move(cb);
}

void MissionManager::notify_changed() {
    MissionChangedCallback cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        cb = changed_cb_;
    }
    if (cb) cb();
}

} // namespace mg::core::mission
