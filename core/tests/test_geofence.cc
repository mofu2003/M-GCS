/**
 * test_geofence.cc - Unit tests for GeofenceManager (no hardware required)
 *
 * Tests: point-in-polygon, area calculation, validation, import/export.
 */
#include "core/geofence/geofence_manager.h"
#include <iostream>
#include <cmath>
#include <cassert>

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; std::cout << "  PASS: " << name << std::endl; } \
    else { failed++; std::cout << "  FAIL: " << name << std::endl; } \
} while(0)

#define TEST_NEAR(name, got, expected, eps) \
    TEST(name, std::abs((got) - (expected)) < (eps))

int main() {
    std::cout << "=== GeofenceManager Unit Tests ===" << std::endl;

    mg::core::geofence::GeofenceManager gm;

    // ---- Point management ----
    {
        std::cout << "\n[Group] Point management" << std::endl;
        TEST("initial empty", gm.point_count() == 0);

        gm.add_point(39.9, 116.4, 100.0);
        gm.add_point(39.91, 116.4, 100.0);
        gm.add_point(39.91, 116.41, 100.0);
        gm.add_point(39.9, 116.41, 100.0);
        TEST("4 points added", gm.point_count() == 4);

        auto pts = gm.get_points();
        TEST("points retrievable", pts.size() == 4);
        TEST_NEAR("point 0 lat", pts[0].lat, 39.9, 1e-10);
        TEST_NEAR("point 0 lon", pts[0].lon, 116.4, 1e-10);
        TEST_NEAR("point 0 alt", pts[0].alt, 100.0f, 1e-5f);

        gm.remove_point(1);
        TEST("remove reduces count", gm.point_count() == 3);

        gm.update_point(0, 39.95, 116.45, 200.0);
        auto pts2 = gm.get_points();
        TEST_NEAR("update lat", pts2[0].lat, 39.95, 1e-10);
        TEST_NEAR("update alt", pts2[0].alt, 200.0f, 1e-5f);

        gm.clear_points();
        TEST("clear empties", gm.point_count() == 0);
    }

    // ---- Point-in-polygon (ray casting) ----
    {
        std::cout << "\n[Group] Point-in-polygon (ray casting)" << std::endl;
        // Simple square: (0,0) (0,1) (1,1) (1,0)
        gm.clear_points();
        gm.add_point(0, 0);
        gm.add_point(0, 1);
        gm.add_point(1, 1);
        gm.add_point(1, 0);

        TEST("center is inside", gm.is_inside_fence(0.5, 0.5));
        TEST("outside left", !gm.is_inside_fence(0.5, -0.1));
        TEST("outside right", !gm.is_inside_fence(0.5, 1.1));
        TEST("outside above", !gm.is_inside_fence(1.1, 0.5));
        TEST("outside below", !gm.is_inside_fence(-0.1, 0.5));

        // Triangle
        gm.clear_points();
        gm.add_point(0, 0);
        gm.add_point(0, 10);
        gm.add_point(10, 5);

        TEST("triangle center inside", gm.is_inside_fence(3, 5));
        TEST("triangle outside", !gm.is_inside_fence(8, 7));
    }

    // ---- Area calculation ----
    {
        std::cout << "\n[Group] Area calculation" << std::endl;
        // 1 degree x 1 degree square at equator
        gm.clear_points();
        gm.add_point(0, 0);
        gm.add_point(0, 1);
        gm.add_point(1, 1);
        gm.add_point(1, 0);

        double area = gm.calculate_area();
        // 1 deg lat ~ 111km, 1 deg lon at equator ~ 111km, area ~ 12321 km2
        TEST("area > 0", area > 0);
        TEST_NEAR("area ~12321 km2", area, 12321000000.0, 500000000.0);
    }

    // ---- Perimeter ----
    {
        std::cout << "\n[Group] Perimeter" << std::endl;
        gm.clear_points();
        gm.add_point(0, 0);
        gm.add_point(0, 1);
        gm.add_point(1, 1);
        gm.add_point(1, 0);

        double perim = gm.calculate_perimeter();
        TEST("perimeter > 0", perim > 0);
        // 4 sides x ~111km each ~ 444km
        TEST_NEAR("perimeter ~444km", perim, 444000.0, 50000.0);
    }

    // ---- Validation ----
    {
        std::cout << "\n[Group] Validation" << std::endl;
        gm.clear_points();
        auto v1 = gm.validate();
        TEST("too few points", !v1.valid);
        TEST("has error", !v1.error.empty());

        gm.add_point(0, 0);
        gm.add_point(0, 1);
        gm.add_point(1, 1);
        gm.add_point(1, 0);
        auto v2 = gm.validate();
        TEST("valid square", v2.valid);
        TEST("no error", v2.error.empty());
        TEST("area computed", v2.area_m2 > 0);
        TEST("perimeter computed", v2.perimeter_m > 0);
    }

    // ---- JSON export/import round-trip ----
    {
        std::cout << "\n[Group] JSON round-trip" << std::endl;
        gm.clear_points();
        gm.add_point(39.9, 116.4, 50.0);
        gm.add_point(39.91, 116.4, 60.0);
        gm.add_point(39.91, 116.41, 70.0);

        mg::core::geofence::FenceConfig cfg;
        cfg.enabled = true;
        cfg.max_altitude = 120.0f;
        cfg.action = 0;
        gm.set_config(cfg);

        std::string json = gm.export_json();
        TEST("export non-empty", !json.empty());

        mg::core::geofence::GeofenceManager gm2;
        TEST("import succeeds", gm2.import_json(json));
        TEST("imported point count", gm2.point_count() == 3);

        auto imported_cfg = gm2.get_config();
        TEST("imported config enabled", imported_cfg.enabled);
        TEST_NEAR("imported max_alt", imported_cfg.max_altitude, 120.0f, 0.1f);

        auto pts = gm2.get_points();
        TEST_NEAR("imported lat", pts[0].lat, 39.9, 1e-10);
        TEST_NEAR("imported alt", pts[0].alt, 50.0f, 1e-5f);
    }

    // ---- KML export ----
    {
        std::cout << "\n[Group] KML export" << std::endl;
        gm.clear_points();
        gm.add_point(39.9, 116.4, 0);
        gm.add_point(39.91, 116.4, 0);
        gm.add_point(39.91, 116.41, 0);

        std::string kml = gm.export_kml("TestFence");
        TEST("kml has header", kml.find("<?xml") != std::string::npos);
        TEST("kml has name", kml.find("TestFence") != std::string::npos);
        TEST("kml has coordinates", kml.find("<coordinates>") != std::string::npos);
        TEST("kml is closed ring", kml.find("39.9,") != std::string::npos);
    }

    // ---- Haversine distance ----
    {
        std::cout << "\n[Group] Distance to fence" << std::endl;
        gm.clear_points();
        gm.add_point(0, 0);
        gm.add_point(0, 1);
        gm.add_point(1, 1);
        gm.add_point(1, 0);

        double d = gm.distance_to_fence(0.5, 0.5);
        TEST("distance from center > 0", d > 0);
        double d2 = gm.distance_to_fence(5, 5);
        TEST("distance from far point > distance from center", d2 > d);
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}