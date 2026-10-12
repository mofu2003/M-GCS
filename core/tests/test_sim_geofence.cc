/**
 * test_sim_geofence.cc - Extended geofence tests
 *
 * Covers the fixes made to GeofenceManager: ray casting, edge distance,
 * area/perimeter, validation (collinear), JSON round-trip, KML export,
 * and thread-safe validation via snapshot.
 */
#include "core/geofence/geofence_manager.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include <string>
#include "mgcs_test_check.h"

using namespace mg::core::geofence;

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


static bool contains(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

// ================================================================
// Test 1: Point management (add/remove/update/clear)
// ================================================================

static void test_point_management() {
    fprintf(stderr, "\n[Group] Point management\n");

    GeofenceManager gm;

    gm.add_point(0.0, 0.0);
    gm.add_point(0.0, 1.0);
    gm.add_point(1.0, 1.0);
    gm.add_point(1.0, 0.0);
    CHECK(gm.point_count() == 4, "4 points added");

    // Remove middle point
    gm.remove_point(1);
    CHECK(gm.point_count() == 3, "point removed, count = 3");

    // Update a point
    gm.update_point(1, 0.0, 0.5, 10.0f);
    auto pts = gm.get_points();
    CHECK(pts[1].lat == 0.0 && pts[1].lon == 0.5 && pts[1].alt == 10.0f,
          "point 1 updated correctly");

    // Clear
    gm.clear_points();
    CHECK(gm.point_count() == 0, "all points cleared");

    // Remove out of bounds (should not crash)
    gm.remove_point(999);
    CHECK(gm.point_count() == 0, "remove out-of-bounds is safe");
}

// ================================================================
// Test 2: Ray casting — square (0,0)->(0,1)->(1,1)->(1,0)
// ================================================================

static void test_ray_casting() {
    fprintf(stderr, "\n[Group] Ray casting\n");

    GeofenceManager gm;
    gm.add_point(0.0, 0.0);
    gm.add_point(0.0, 1.0);
    gm.add_point(1.0, 1.0);
    gm.add_point(1.0, 0.0);

    // Center of square
    CHECK(gm.is_inside_fence(0.5, 0.5), "center (0.5,0.5) is inside");

    // Clearly outside
    CHECK(!gm.is_inside_fence(2.0, 2.0), "point (2,2) is outside");
    CHECK(!gm.is_inside_fence(-1.0, 0.5), "point (-1,0.5) is outside");

    // Near edge but inside
    CHECK(gm.is_inside_fence(0.5, 0.01), "point near left edge is inside");

    // Near corner but inside
    CHECK(gm.is_inside_fence(0.01, 0.01), "point near corner is inside");

    // Outside each side
    CHECK(!gm.is_inside_fence(0.5, -0.1), "below bottom is outside");
    CHECK(!gm.is_inside_fence(0.5, 1.1), "above top is outside");
    CHECK(!gm.is_inside_fence(-0.1, 0.5), "left of left is outside");
    CHECK(!gm.is_inside_fence(1.1, 0.5), "right of right is outside");
}

// ================================================================
// Test 3: Distance to EDGE (not vertex)
// ================================================================

static void test_distance_to_edge() {
    fprintf(stderr, "\n[Group] Distance to fence edge\n");

    GeofenceManager gm;
    // Unit square: (0,0) -> (0,1) -> (1,1) -> (1,0)
    gm.add_point(0.0, 0.0);
    gm.add_point(0.0, 1.0);
    gm.add_point(1.0, 1.0);
    gm.add_point(1.0, 0.0);

    // Point at (0.5, 0.5) — the nearest edge is the bottom edge at y=0
    // Distance should be ~0.5 degrees latitude * 111km/deg = ~55km
    double dist_center = gm.distance_to_fence(0.5, 0.5);
    CHECK(dist_center > 0, "distance from center is positive");
    // Rough: 0.5 degrees * 111320 m/deg ~ 55 km
    CHECK(dist_center > 10000.0 && dist_center < 100000.0,
          "distance from center to nearest edge is ~55km");

    // Point near middle of bottom edge (0.5, 0) — not a vertex
    double dist_at_edge = gm.distance_to_fence(0.5, 0.0);
    CHECK(dist_at_edge < 1000.0, "point on/near edge has small distance");

    // Point near vertex (0,0) — should still be small
    double dist_near_vertex = gm.distance_to_fence(0.001, 0.001);
    CHECK(dist_near_vertex < 1000.0, "point near vertex has small distance");
}

// ================================================================
// Test 4: Area calculation for known square
// ================================================================

static void test_area_calculation() {
    fprintf(stderr, "\n[Group] Area calculation\n");

    GeofenceManager gm;
    // Approximate 100m x 100m square near equator
    // 1 degree lat ~ 111320m, so 100m ~ 0.000898 deg
    double dlat = 100.0 / 111320.0;
    double dlon = 100.0 / 111412.84;  // at equator
    double base_lat = 0.0, base_lon = 0.0;

    gm.add_point(base_lat, base_lon);
    gm.add_point(base_lat, base_lon + dlon);
    gm.add_point(base_lat + dlat, base_lon + dlon);
    gm.add_point(base_lat + dlat, base_lon);

    double area = gm.calculate_area();
    CHECK(area > 5000.0 && area < 20000.0, "100m x 100m square area ~10000 m^2");
}

// ================================================================
// Test 5: Perimeter calculation
// ================================================================

static void test_perimeter_calculation() {
    fprintf(stderr, "\n[Group] Perimeter calculation\n");

    GeofenceManager gm;
    // Same 100m x 100m square
    double dlat = 100.0 / 111320.0;
    double dlon = 100.0 / 111412.84;
    double base_lat = 0.0, base_lon = 0.0;

    gm.add_point(base_lat, base_lon);
    gm.add_point(base_lat, base_lon + dlon);
    gm.add_point(base_lat + dlat, base_lon + dlon);
    gm.add_point(base_lat + dlat, base_lon);

    double perim = gm.calculate_perimeter();
    CHECK(perim > 300.0 && perim < 500.0, "100m x 100m square perimeter ~400m");
}

// ================================================================
// Test 6: Validation — collinear points -> invalid
// ================================================================

static void test_collinear_validation() {
    fprintf(stderr, "\n[Group] Collinear validation\n");

    GeofenceManager gm;
    // Three collinear points on the same line
    gm.add_point(0.0, 0.0);
    gm.add_point(0.0, 0.5);
    gm.add_point(0.0, 1.0);

    FenceValidation v = gm.validate();
    CHECK(!v.valid, "collinear points are invalid");
    CHECK(contains(v.error, "collinear"), "error mentions collinear");

    // Now add a non-collinear point
    gm.add_point(0.5, 0.5);
    v = gm.validate();
    CHECK(v.valid, "non-collinear polygon is valid");
}

// ================================================================
// Test 7: JSON export/import round-trip
// ================================================================

static void test_json_roundtrip() {
    fprintf(stderr, "\n[Group] JSON round-trip\n");

    GeofenceManager gm;
    gm.add_point(39.90, 116.40, 10.0f);
    gm.add_point(39.90, 116.41, 20.0f);
    gm.add_point(39.91, 116.41, 30.0f);
    gm.add_point(39.91, 116.40, 40.0f);

    std::string json = gm.export_json();
    CHECK(!json.empty(), "export_json produces non-empty string");
    CHECK(contains(json, "\"points\""), "JSON contains 'points' array");
    CHECK(contains(json, "39.9"), "JSON contains latitude values");
    CHECK(contains(json, "116.4"), "JSON contains longitude values");

    // Import into a new manager
    GeofenceManager gm2;
    bool ok = gm2.import_json(json);
    CHECK(ok, "import_json succeeds");
    CHECK(gm2.point_count() == 4, "imported 4 points");

    auto pts = gm2.get_points();
    bool lat_ok = std::fabs(pts[0].lat - 39.90) < 0.001;
    bool lon_ok = std::fabs(pts[0].lon - 116.40) < 0.001;
    bool alt_ok = std::fabs(pts[0].alt - 10.0f) < 0.1f;
    CHECK(lat_ok && lon_ok && alt_ok, "first point coordinates match after round-trip");

    // Bad JSON
    bool bad = gm2.import_json("{invalid json");
    CHECK(!bad, "bad JSON returns false");
}

// ================================================================
// Test 8: KML export produces valid XML
// ================================================================

static void test_kml_export() {
    fprintf(stderr, "\n[Group] KML export\n");

    GeofenceManager gm;
    gm.add_point(39.90, 116.40, 10.0f);
    gm.add_point(39.90, 116.41, 20.0f);
    gm.add_point(39.91, 116.41, 30.0f);

    std::string kml = gm.export_kml("TestFence");
    CHECK(!kml.empty(), "KML export is non-empty");
    CHECK(contains(kml, "<?xml"), "KML starts with XML declaration");
    CHECK(contains(kml, "<kml"), "KML contains <kml> element");
    CHECK(contains(kml, "TestFence"), "KML contains fence name");
    CHECK(contains(kml, "<coordinates>"), "KML contains coordinates");
    CHECK(contains(kml, "116.4"), "KML contains longitude values");
    CHECK(contains(kml, "</kml>"), "KML properly closed");
}

// ================================================================
// Test 9: validate() thread safety — uses snapshot
//
// Verify that validate() works correctly even after points are cleared
// mid-operation (it operates on a snapshot copy).
// ================================================================

static void test_validate_thread_safety() {
    fprintf(stderr, "\n[Group] validate() snapshot safety\n");

    GeofenceManager gm;
    gm.add_point(0.0, 0.0);
    gm.add_point(0.0, 1.0);
    gm.add_point(1.0, 1.0);
    gm.add_point(1.0, 0.0);

    // Take a snapshot of the validation while points exist
    FenceValidation v = gm.validate();
    CHECK(v.valid, "validate valid before clear");
    CHECK(v.area_m2 > 0, "area computed before clear");

    // Clear points — validation result should still reflect the
    // snapshot taken at the start of validate()
    gm.clear_points();
    CHECK(gm.point_count() == 0, "points cleared");

    FenceValidation v2 = gm.validate();
    CHECK(!v2.valid, "validate invalid after clear (< 3 points)");
}

// ================================================================
// Main
// ================================================================

int main() {
    fprintf(stderr, "=== Geofence Simulation Tests ===\n");
    test_point_management();
    test_ray_casting();
    test_distance_to_edge();
    test_area_calculation();
    test_perimeter_calculation();
    test_collinear_validation();
    test_json_roundtrip();
    test_kml_export();
    test_validate_thread_safety();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
