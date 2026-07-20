/**
 * test_telemetry_ring_buffer.cc - Unit tests for TelemetryRingBuffer (no hardware)
 *
 * Tests: append, ring overflow, multi-series, stats, JSON output.
 */
#include "core/telemetry/telemetry_ring_buffer.h"
#include <iostream>
#include <cmath>
#include <string>

static int passed = 0;
static int failed = 0;

#define TEST(name, expr) do { \
    if (expr) { passed++; std::cout << "  PASS: " << name << std::endl; } \
    else { failed++; std::cout << "  FAIL: " << name << std::endl; } \
} while(0)

#define TEST_NEAR(name, got, expected, eps) \
    TEST(name, std::abs((got) - (expected)) < (eps))

int main() {
    std::cout << "=== TelemetryRingBuffer Unit Tests ===" << std::endl;

    // ---- Basic register and append ----
    {
        std::cout << "\n[Group] Register and append" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);

        buf.register_series("altitude", "m");
        TEST("series names has altitude",
             buf.get_series_names().size() == 1 && buf.get_series_names()[0] == "altitude");

        buf.append("altitude", 1000, 10.0f);
        buf.append("altitude", 1100, 12.5f);
        buf.append("altitude", 1200, 15.0f);

        TEST("total points = 3", buf.total_points() == 3);

        auto pts = buf.get_series("altitude");
        TEST("get_series returns 3 points", pts.size() == 3);
        TEST_NEAR("first value", pts[0].value, 10.0f, 1e-5f);
        TEST_NEAR("second value", pts[1].value, 12.5f, 1e-5f);
        TEST_NEAR("third value", pts[2].value, 15.0f, 1e-5f);
        TEST("first timestamp", pts[0].timestamp_ms == 1000);
    }

    // ---- Ring buffer overflow ----
    {
        std::cout << "\n[Group] Ring buffer overflow" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(5);

        buf.register_series("speed", "m/s");
        for (int i = 0; i < 10; i++) {
            buf.append("speed", i * 100, static_cast<float>(i));
        }

        TEST("total = max (5)", buf.total_points() == 5);

        auto pts = buf.get_series("speed");
        TEST("series returns 5 points", pts.size() == 5);
        // Should contain values 5,6,7,8,9 (oldest overwritten)
        TEST_NEAR("oldest is 5", pts[0].value, 5.0f, 1e-5f);
        TEST_NEAR("newest is 9", pts[4].value, 9.0f, 1e-5f);
        TEST("oldest timestamp", pts[0].timestamp_ms == 500);
        TEST("newest timestamp", pts[4].timestamp_ms == 900);
    }

    // ---- get_series(last_n) ----
    {
        std::cout << "\n[Group] get_series(last_n)" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);

        buf.register_series("alt", "m");
        for (int i = 0; i < 20; i++) {
            buf.append("alt", i * 100, static_cast<float>(i));
        }

        auto last3 = buf.get_series("alt", 3);
        TEST("last 3 returns 3", last3.size() == 3);
        TEST_NEAR("last3[0] = 17", last3[0].value, 17.0f, 1e-5f);
        TEST_NEAR("last3[2] = 19", last3[2].value, 19.0f, 1e-5f);

        auto last100 = buf.get_series("alt", 100);
        TEST("requesting more than available returns all", last100.size() == 20);
    }

    // ---- get_latest ----
    {
        std::cout << "\n[Group] get_latest" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("v", "");
        TEST("latest empty = 0", buf.get_latest("v") == 0.0f);

        buf.append("v", 0, 42.0f);
        TEST_NEAR("latest after append", buf.get_latest("v"), 42.0f, 1e-5f);

        buf.append("v", 100, 99.0f);
        TEST_NEAR("latest updated", buf.get_latest("v"), 99.0f, 1e-5f);
    }

    // ---- Multi-series ----
    {
        std::cout << "\n[Group] Multi-series" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);

        buf.register_series("alt", "m");
        buf.register_series("speed", "m/s");
        buf.register_series("battery", "V");

        buf.append("alt", 0, 100.0f);
        buf.append("speed", 0, 15.0f);
        buf.append("battery", 0, 12.6f);
        buf.append("alt", 100, 110.0f);

        TEST("3 series registered", buf.get_series_names().size() == 3);
        TEST("alt = 2 points", buf.get_series("alt").size() == 2);
        TEST("speed = 1 point", buf.get_series("speed").size() == 1);
        TEST("battery = 1 point", buf.get_series("battery").size() == 1);
        TEST("total = 4", buf.total_points() == 4);
    }

    // ---- Stats ----
    {
        std::cout << "\n[Group] Stats" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("data", "units");

        buf.append("data", 0, 10.0f);
        buf.append("data", 100, 20.0f);
        buf.append("data", 200, 30.0f);
        buf.append("data", 300, 40.0f);

        auto stats = buf.stats_json("data");
        TEST("stats has name", stats.find("\"name\":\"data\"") != std::string::npos);
        TEST("stats has count=4", stats.find("\"count\":4") != std::string::npos);
        TEST("stats has min", stats.find("\"min\":") != std::string::npos);
        TEST("stats has max", stats.find("\"max\":") != std::string::npos);
        TEST("stats has avg", stats.find("\"avg\":") != std::string::npos);
        TEST("stats has latest", stats.find("\"latest\":") != std::string::npos);
    }

    // ---- Clear ----
    {
        std::cout << "\n[Group] Clear" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("a", "");
        buf.register_series("b", "");
        buf.append("a", 0, 1.0f);
        buf.append("b", 0, 2.0f);

        TEST("total = 2 before clear", buf.total_points() == 2);
        buf.clear();
        TEST("total = 0 after clear", buf.total_points() == 0);

        buf.append("a", 0, 5.0f);
        TEST("append after clear works", buf.total_points() == 1);
        TEST_NEAR("value after clear", buf.get_latest("a"), 5.0f, 1e-5f);
    }

    // ---- Unknown series ----
    {
        std::cout << "\n[Group] Unknown series" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        TEST("unknown series names empty", buf.get_series_names().empty());
        TEST("unknown get_series empty", buf.get_series("nope").empty());
        TEST("unknown get_latest = 0", buf.get_latest("nope") == 0.0f);
        TEST("append to unknown is no-op", (buf.append("nope", 0, 1.0f), buf.total_points() == 0));
    }

    // ---- JSON output ----
    {
        std::cout << "\n[Group] JSON output" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("d", "");
        buf.append("d", 100, 1.5f);
        buf.append("d", 200, 2.5f);

        auto json = buf.to_json("d");
        TEST("json starts with [", json.front() == '[');
        TEST("json ends with ]", json.back() == ']');
        TEST("json has timestamp", json.find("\"t\":100") != std::string::npos);
        TEST("json has value", json.find("\"v\":") != std::string::npos);
    }

    // ---- Duplicate register is no-op ----
    {
        std::cout << "\n[Group] Duplicate register" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("x", "m");
        buf.append("x", 0, 1.0f);
        buf.register_series("x", "km");
        TEST("duplicate register doesn't clear data", buf.total_points() == 1);
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}