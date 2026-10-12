/**
 * test_telemetry_ring_buffer.cc - Unit tests for TelemetryRingBuffer (no hardware)
 *
 * Tests: append, ring overflow, multi-series, stats, JSON output.
 */
#include "core/telemetry/telemetry_ring_buffer.h"
#include <iostream>
#include <cmath>
#include <string>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& passed = mgcs_test::passed();
static int& failed = mgcs_test::failed();



int main() {
    std::cout << "=== TelemetryRingBuffer Unit Tests ===" << std::endl;

    // ---- Basic register and append ----
    {
        std::cout << "\n[Group] Register and append" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);

        buf.register_series("altitude", "m");
        CHECK(buf.get_series_names().size() == 1 && buf.get_series_names()[0] == "altitude", "series names has altitude");

        buf.append("altitude", 1000, 10.0f);
        buf.append("altitude", 1100, 12.5f);
        buf.append("altitude", 1200, 15.0f);

        CHECK(buf.total_points() == 3, "total points = 3");

        auto pts = buf.get_series("altitude");
        CHECK(pts.size() == 3, "get_series returns 3 points");
        CHECK_NEAR(pts[0].value, 10.0f, 1e-5f, "first value");
        CHECK_NEAR(pts[1].value, 12.5f, 1e-5f, "second value");
        CHECK_NEAR(pts[2].value, 15.0f, 1e-5f, "third value");
        CHECK(pts[0].timestamp_ms == 1000, "first timestamp");
    }

    // ---- Ring buffer overflow ----
    {
        std::cout << "\n[Group] Ring buffer overflow" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(5);

        buf.register_series("speed", "m/s");
        for (int i = 0; i < 10; i++) {
            buf.append("speed", i * 100, static_cast<float>(i));
        }

        CHECK(buf.total_points() == 5, "total = max (5)");

        auto pts = buf.get_series("speed");
        CHECK(pts.size() == 5, "series returns 5 points");
        // Should contain values 5,6,7,8,9 (oldest overwritten)
        CHECK_NEAR(pts[0].value, 5.0f, 1e-5f, "oldest is 5");
        CHECK_NEAR(pts[4].value, 9.0f, 1e-5f, "newest is 9");
        CHECK(pts[0].timestamp_ms == 500, "oldest timestamp");
        CHECK(pts[4].timestamp_ms == 900, "newest timestamp");
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
        CHECK(last3.size() == 3, "last 3 returns 3");
        CHECK_NEAR(last3[0].value, 17.0f, 1e-5f, "last3[0] = 17");
        CHECK_NEAR(last3[2].value, 19.0f, 1e-5f, "last3[2] = 19");

        auto last100 = buf.get_series("alt", 100);
        CHECK(last100.size() == 20, "requesting more than available returns all");
    }

    // ---- get_latest ----
    {
        std::cout << "\n[Group] get_latest" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("v", "");
        CHECK(buf.get_latest("v") == 0.0f, "latest empty = 0");

        buf.append("v", 0, 42.0f);
        CHECK_NEAR(buf.get_latest("v"), 42.0f, 1e-5f, "latest after append");

        buf.append("v", 100, 99.0f);
        CHECK_NEAR(buf.get_latest("v"), 99.0f, 1e-5f, "latest updated");
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

        CHECK(buf.get_series_names().size() == 3, "3 series registered");
        CHECK(buf.get_series("alt").size() == 2, "alt = 2 points");
        CHECK(buf.get_series("speed").size() == 1, "speed = 1 point");
        CHECK(buf.get_series("battery").size() == 1, "battery = 1 point");
        CHECK(buf.total_points() == 4, "total = 4");
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
        CHECK(stats.find("\"name\":\"data\"") != std::string::npos, "stats has name");
        CHECK(stats.find("\"count\":4") != std::string::npos, "stats has count=4");
        CHECK(stats.find("\"min\":") != std::string::npos, "stats has min");
        CHECK(stats.find("\"max\":") != std::string::npos, "stats has max");
        CHECK(stats.find("\"avg\":") != std::string::npos, "stats has avg");
        CHECK(stats.find("\"latest\":") != std::string::npos, "stats has latest");
    }

    // ---- Clear ----
    {
        std::cout << "\n[Group] Clear" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("a", "");
        buf.register_series("b", "");
        buf.append("a", 0, 1.0f);
        buf.append("b", 0, 2.0f);

        CHECK(buf.total_points() == 2, "total = 2 before clear");
        buf.clear();
        CHECK(buf.total_points() == 0, "total = 0 after clear");

        buf.append("a", 0, 5.0f);
        CHECK(buf.total_points() == 1, "append after clear works");
        CHECK_NEAR(buf.get_latest("a"), 5.0f, 1e-5f, "value after clear");
    }

    // ---- Unknown series ----
    {
        std::cout << "\n[Group] Unknown series" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        CHECK(buf.get_series_names().empty(), "unknown series names empty");
        CHECK(buf.get_series("nope").empty(), "unknown get_series empty");
        CHECK(buf.get_latest("nope") == 0.0f, "unknown get_latest = 0");
        CHECK((buf.append("nope", 0, 1.0f), buf.total_points() == 0), "append to unknown is no-op");
    }

    // ---- JSON output ----
    {
        std::cout << "\n[Group] JSON output" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("d", "");
        buf.append("d", 100, 1.5f);
        buf.append("d", 200, 2.5f);

        auto json = buf.to_json("d");
        CHECK(json.front() == '[', "json starts with [");
        CHECK(json.back() == ']', "json ends with ]");
        CHECK(json.find("\"t\":100") != std::string::npos, "json has timestamp");
        CHECK(json.find("\"v\":") != std::string::npos, "json has value");
    }

    // ---- Duplicate register is no-op ----
    {
        std::cout << "\n[Group] Duplicate register" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        buf.register_series("x", "m");
        buf.append("x", 0, 1.0f);
        buf.register_series("x", "km");
        CHECK(buf.total_points() == 1, "duplicate register doesn't clear data");
    }

    // ---- JSON escaping ----
    // name/unit 是前端经遥测注册入口传进来的，手拼 JSON 时未转义会把引号/换行
    // 直接写进响应（注入面）。改用 JsonBuilder 后必须转义输出。
    // （注释里不写具体导出名：契约脚本把「测试文件里出现符号名」当作有调用方。）
    {
        std::cout << "\n[Group] JSON escaping" << std::endl;
        mg::core::telemetry::TelemetryRingBuffer buf(100);
        const std::string quoted_name = "a\"b";
        const std::string newline_unit = "u\nit";
        buf.register_series(quoted_name, newline_unit);
        buf.append(quoted_name, 1000, 1.5f);

        const auto stats = buf.stats_json(quoted_name);
        CHECK(stats.find("\"name\":\"a\\\"b\"") != std::string::npos, "stats escapes the quote in the series name");
        CHECK(stats.find("\"unit\":\"u\\nit\"") != std::string::npos, "stats escapes the newline in the unit");
        CHECK(stats.find("\"name\":\"a\"b\"") == std::string::npos, "stats does not emit the raw name as JSON");
        CHECK(stats.find("\"count\":1") != std::string::npos &&
                 stats.find("\"latest\":1.5") != std::string::npos, "stats keeps the numeric fields");
    }

    std::cout << "\n=== " << passed << " passed, " << failed << " failed ===" << std::endl;
    return failed > 0 ? 1 : 0;
}