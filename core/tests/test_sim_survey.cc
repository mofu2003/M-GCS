/**
 * test_sim_survey.cc - Survey generation tests (pure computation, no mock FC)
 *
 * Tests SurveyCalculator::generate_grid / generate_cross_grid and the
 * mg_survey_generate FFI bridge.
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include "core/mission/survey_calculator.h"
#include "core/mission/mission_manager.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace mg::core::mission;

static int g_pass = 0, g_fail = 0;

#define CHECK(c, m) do { \
    if (c) { g_pass++; fprintf(stderr, "  [PASS] %s\n", m); } \
    else   { g_fail++; fprintf(stderr, "  [FAIL] %s\n", m); } \
} while(0)

static bool json_has(const char* json, const char* key) {
    return json && strstr(json, key) != nullptr;
}

// ================================================================
// Test 1: basic grid generation with known center
// ================================================================

static void test_grid_basic() {
    fprintf(stderr, "\n[Group] Grid basic generation\n");

    SurveyConfig cfg;
    cfg.center_lat_deg = 39.9;
    cfg.center_lon_deg = 116.4;
    cfg.width_m  = 100.0;
    cfg.length_m = 100.0;
    cfg.altitude_m = 50.0f;
    cfg.speed_ms   = 5.0f;
    cfg.angle_deg  = 0.0;
    cfg.cross_grid = false;
    cfg.return_to_start = true;

    SurveyResult r = SurveyCalculator::generate_grid(cfg);
    CHECK(r.valid, "generate_grid returns valid for 100x100m survey");
    CHECK(r.waypoint_count > 0, "waypoint_count > 0");

    // Verify waypoint longitudes are near 116.4 (not near 0)
    bool lons_near_116 = true;
    for (const auto& item : r.items) {
        // MissionItem stores lat/lon as degE7 integers
        if (item.command == 16) {  // NAV_WAYPOINT
            double lon = item.y / 1e7;
            if (std::fabs(lon - 116.4) > 0.1) {
                lons_near_116 = false;
                break;
            }
        }
    }
    CHECK(lons_near_116, "waypoint longitudes are near 116.4 (not near 0)");

    // Verify latitudes are near 39.9
    bool lats_near_39 = true;
    for (const auto& item : r.items) {
        if (item.command == 16) {
            double lat = item.x / 1e7;
            if (std::fabs(lat - 39.9) > 0.01) {
                lats_near_39 = false;
                break;
            }
        }
    }
    CHECK(lats_near_39, "waypoint latitudes are near 39.9");

    CHECK(r.flight_line_count > 0, "flight_line_count > 0");
    CHECK(r.total_distance_m > 0, "total_distance_m > 0");
    CHECK(r.area_m2 > 0, "area_m2 > 0");
}

// ================================================================
// Test 2: cross grid produces ~2x the waypoints
// ================================================================

static void test_cross_grid() {
    fprintf(stderr, "\n[Group] Cross grid generation\n");

    SurveyConfig cfg;
    cfg.center_lat_deg = 39.9;
    cfg.center_lon_deg = 116.4;
    cfg.width_m  = 100.0;
    cfg.length_m = 100.0;
    cfg.altitude_m = 50.0f;
    cfg.speed_ms   = 5.0f;
    cfg.cross_grid = false;

    SurveyResult single = SurveyCalculator::generate_grid(cfg);
    CHECK(single.valid, "single grid valid");

    SurveyResult cross = SurveyCalculator::generate_cross_grid(cfg);
    CHECK(cross.valid, "cross grid valid");

    // Cross grid should have roughly 2x the flight lines / waypoints
    CHECK(cross.flight_line_count >= single.flight_line_count,
          "cross grid has >= flight lines than single");
    CHECK(cross.flight_line_count <= single.flight_line_count * 2 + 2,
          "cross grid has <= 2x flight lines (plus tolerance)");

    // Cross grid total distance should be significantly more
    CHECK(cross.total_distance_m > single.total_distance_m * 1.5,
          "cross grid total_distance > 1.5x single");
}

// ================================================================
// Test 3: zero dimensions -> invalid
// ================================================================

static void test_zero_dimensions() {
    fprintf(stderr, "\n[Group] Zero/invalid dimensions\n");

    SurveyConfig cfg;
    cfg.center_lat_deg = 39.9;
    cfg.center_lon_deg = 116.4;
    cfg.width_m  = 0.0;
    cfg.length_m = 100.0;
    cfg.altitude_m = 50.0f;

    SurveyResult r = SurveyCalculator::generate_grid(cfg);
    CHECK(!r.valid, "zero width -> invalid");

    cfg.width_m  = 100.0;
    cfg.length_m = 0.0;
    r = SurveyCalculator::generate_grid(cfg);
    CHECK(!r.valid, "zero length -> invalid");

    cfg.length_m  = 100.0;
    cfg.altitude_m = 0.0f;
    r = SurveyCalculator::generate_grid(cfg);
    CHECK(!r.valid, "zero altitude -> invalid");

    // Negative dimensions
    cfg.width_m   = -50.0;
    cfg.altitude_m = 50.0f;
    r = SurveyCalculator::generate_grid(cfg);
    CHECK(!r.valid, "negative width -> invalid");
}

// ================================================================
// Test 4: camera calculations
// ================================================================

static void test_camera_calculations() {
    fprintf(stderr, "\n[Group] Camera calculations\n");

    CameraSpec cam;
    cam.focal_length_mm = 35.0f;
    cam.sensor_width_mm = 23.5f;
    cam.sensor_height_mm = 15.6f;

    // GSD at 50m
    float gsd = SurveyCalculator::calculate_gsd(cam, 50.0f);
    CHECK(gsd > 0, "GSD is positive");
    // GSD = (50 * 23.5) / (35 * 6000) = 1175 / 210000 = 0.0056 m/px
    CHECK(gsd > 0.001f && gsd < 0.1f, "GSD at 50m is reasonable (0.001-0.1 m/px)");

    // Trigger distance
    float trigger_dist = SurveyCalculator::calculate_trigger_distance(cam, 50.0f, 70.0f);
    CHECK(trigger_dist > 0, "trigger distance > 0");

    // Line spacing
    float spacing = SurveyCalculator::calculate_line_spacing(cam, 50.0f, 60.0f);
    CHECK(spacing > 0, "line spacing > 0");

    // Altitude for target GSD
    float alt = SurveyCalculator::altitude_for_gsd(cam, 5.0f);  // 5 cm/px
    CHECK(alt > 0, "altitude_for_gsd returns positive altitude");
}

// ================================================================
// Test 5: result structure
// ================================================================
static void test_survey_json_roundtrip() {
    fprintf(stderr, "\n[Group] Survey result structure\n");

    SurveyConfig cfg;
    cfg.center_lat_deg = 39.9;
    cfg.center_lon_deg = 116.4;
    cfg.width_m  = 100.0;
    cfg.length_m = 100.0;
    cfg.altitude_m = 50.0f;

    SurveyResult r = SurveyCalculator::generate_grid(cfg);
    CHECK(r.valid, "survey valid");

    // Verify all items have sequential seq numbers
    bool seq_ok = true;
    for (size_t i = 0; i < r.items.size(); i++) {
        if (r.items[i].seq != (uint16_t)i) {
            seq_ok = false;
            break;
        }
    }
    CHECK(seq_ok, "mission items have sequential seq numbers");

    // First items should be camera trigger + speed change
    bool has_trigger = false, has_speed = false;
    for (const auto& item : r.items) {
        if (item.command == 206) has_trigger = true;  // DO_SET_CAM_TRIGG_DIST
        if (item.command == 178) has_speed = true;    // DO_CHANGE_SPEED
    }
    CHECK(has_trigger, "has camera trigger command (206)");
    CHECK(has_speed, "has speed change command (178)");

    // Last item should be RTL (command 20)
    CHECK(!r.items.empty() && r.items.back().command == 20,
          "last item is RTL (command 20)");
}

// ================================================================
// Test 6: FFI bridge generation
// ================================================================
static void test_survey_ffi() {
    fprintf(stderr, "\n[Group] Survey FFI bridge\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    const char* grid_cfg =
        R"({"center_lat":39.9,"center_lon":116.4,"width":100,"length":100,"altitude":50,"speed":5,"return_to_start":true})";
    char* r = mg_survey_generate(core, grid_cfg);
    CHECK(r != nullptr, "mg_survey_generate returns non-null");
    CHECK(json_has(r, "\"valid\":true"), "mg_survey_generate returns valid=true");
    CHECK(json_has(r, "\"waypoint_count\""), "mg_survey_generate returns waypoint_count");
    CHECK(json_has(r, "\"waypoints\""), "mg_survey_generate returns waypoints");
    if (r) mg_free_string(r);

    const char* corridor_cfg =
        R"({"start_lat":39.9,"start_lon":116.4,"end_lat":39.91,"end_lon":116.41,"width":50,"altitude":50,"speed":5})";
    r = mg_survey_generate_corridor(core, corridor_cfg);
    CHECK(r != nullptr, "mg_survey_generate_corridor returns non-null");
    CHECK(json_has(r, "\"valid\":true"), "mg_survey_generate_corridor returns valid=true");
    CHECK(json_has(r, "\"flight_line_count\""), "mg_survey_generate_corridor returns flight_line_count");
    if (r) mg_free_string(r);

    r = mg_survey_generate(core, "{bad json");
    CHECK(r != nullptr, "mg_survey_generate invalid json returns non-null");
    CHECK(json_has(r, "\"valid\":false"), "mg_survey_generate invalid json returns valid=false");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Main
// ================================================================

int main() {
    fprintf(stderr, "=== Survey Simulation Tests ===\n");
    test_grid_basic();
    test_cross_grid();
    test_zero_dimensions();
    test_camera_calculations();
    test_survey_json_roundtrip();
    test_survey_ffi();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
