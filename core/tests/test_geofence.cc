/**
 * test_geofence.cc - Unit tests for GeofenceManager (no hardware required)
 *
 * Tests: point-in-polygon, area calculation, validation, import/export.
 */
#include "core/geofence/geofence_manager.h"
#include <iostream>
#include <cmath>
#include <cassert>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& passed = mgcs_test::passed();
static int& failed = mgcs_test::failed();



int main() {
    std::cout << "=== GeofenceManager Unit Tests ===" << std::endl;

    mg::core::geofence::GeofenceManager gm;

    // ---- Point management ----
    {
        std::cout << "\n[Group] Point management" << std::endl;
        CHECK(gm.point_count() == 0, "initial empty");

        gm.add_point(39.9, 116.4, 100.0);
        gm.add_point(39.91, 116.4, 100.0);
        gm.add_point(39.91, 116.41, 100.0);
        gm.add_point(39.9, 116.41, 100.0);
        CHECK(gm.point_count() == 4, "4 points added");

        auto pts = gm.get_points();
        CHECK(pts.size() == 4, "points retrievable");
        CHECK_NEAR(pts[0].lat, 39.9, 1e-10, "point 0 lat");
        CHECK_NEAR(pts[0].lon, 116.4, 1e-10, "point 0 lon");
        CHECK_NEAR(pts[0].alt, 100.0f, 1e-5f, "point 0 alt");

        gm.remove_point(1);
        CHECK(gm.point_count() == 3, "remove reduces count");

        gm.update_point(0, 39.95, 116.45, 200.0);
        auto pts2 = gm.get_points();
        CHECK_NEAR(pts2[0].lat, 39.95, 1e-10, "update lat");
        CHECK_NEAR(pts2[0].alt, 200.0f, 1e-5f, "update alt");

        gm.clear_points();
        CHECK(gm.point_count() == 0, "clear empties");
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

        CHECK(gm.is_inside_fence(0.5, 0.5), "center is inside");
        CHECK(!gm.is_inside_fence(0.5, -0.1), "outside left");
        CHECK(!gm.is_inside_fence(0.5, 1.1), "outside right");
        CHECK(!gm.is_inside_fence(1.1, 0.5), "outside above");
        CHECK(!gm.is_inside_fence(-0.1, 0.5), "outside below");

        // Triangle
        gm.clear_points();
        gm.add_point(0, 0);
        gm.add_point(0, 10);
        gm.add_point(10, 5);

        CHECK(gm.is_inside_fence(3, 5), "triangle center inside");
        CHECK(!gm.is_inside_fence(8, 7), "triangle outside");
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
        CHECK(area > 0, "area > 0");
        CHECK_NEAR(area, 12321000000.0, 500000000.0, "area ~12321 km2");
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
        CHECK(perim > 0, "perimeter > 0");
        // 4 sides x ~111km each ~ 444km
        CHECK_NEAR(perim, 444000.0, 50000.0, "perimeter ~444km");
    }

    // ---- Validation ----
    {
        std::cout << "\n[Group] Validation" << std::endl;
        gm.clear_points();
        auto v1 = gm.validate();
        CHECK(!v1.valid, "too few points");
        CHECK(!v1.error.empty(), "has error");

        gm.add_point(0, 0);
        gm.add_point(0, 1);
        gm.add_point(1, 1);
        gm.add_point(1, 0);
        auto v2 = gm.validate();
        CHECK(v2.valid, "valid square");
        CHECK(v2.error.empty(), "no error");
        CHECK(v2.area_m2 > 0, "area computed");
        CHECK(v2.perimeter_m > 0, "perimeter computed");
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
        CHECK(!json.empty(), "export non-empty");

        mg::core::geofence::GeofenceManager gm2;
        CHECK(gm2.import_json(json), "import succeeds");
        CHECK(gm2.point_count() == 3, "imported point count");

        auto imported_cfg = gm2.get_config();
        CHECK(imported_cfg.enabled, "imported config enabled");
        CHECK_NEAR(imported_cfg.max_altitude, 120.0f, 0.1f, "imported max_alt");

        auto pts = gm2.get_points();
        CHECK_NEAR(pts[0].lat, 39.9, 1e-10, "imported lat");
        CHECK_NEAR(pts[0].alt, 50.0f, 1e-5f, "imported alt");
    }

    // ---- KML export ----
    {
        std::cout << "\n[Group] KML export" << std::endl;
        gm.clear_points();
        gm.add_point(39.9, 116.4, 0);
        gm.add_point(39.91, 116.4, 0);
        gm.add_point(39.91, 116.41, 0);

        std::string kml = gm.export_kml("TestFence");
        CHECK(kml.find("<?xml") != std::string::npos, "kml has header");
        CHECK(kml.find("TestFence") != std::string::npos, "kml has name");
        CHECK(kml.find("<coordinates>") != std::string::npos, "kml has coordinates");
        CHECK(kml.find("39.9,") != std::string::npos, "kml is closed ring");
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
        CHECK(d > 0, "distance from center > 0");
        double d2 = gm.distance_to_fence(5, 5);
        CHECK(d2 > d, "distance from far point > distance from center");
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}
