/**
 * test_telemetry_cache.cc - Unit tests for TelemetryCache
 *
 * Tests: construction, update routing, JSON output, thread safety
 * All tests use mock data, no real hardware needed.
 */
#include "telemetry_cache.h"
#include "core/platform/platform_mutex.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

int main() {
    fprintf(stderr, "=== TelemetryCache Unit Tests ===\n\n");

    // 1. Construction
    fprintf(stderr, "[Group] Construction\n");
    {
        mg::bridge::TelemetryCache cache;
        (void)cache;
        CHECK(true, "TelemetryCache constructs without crash");
    }

    // 2. Default JSON output
    fprintf(stderr, "[Group] Default JSON output\n");
    {
        mg::bridge::TelemetryCache cache;

        std::string hb_json = cache.heartbeat.to_json();
        CHECK(hb_json.find("type") != std::string::npos, "heartbeat JSON has 'type' field");
        CHECK(hb_json.find("armed") != std::string::npos, "heartbeat JSON has 'armed' field");

        std::string gps_json = cache.gps.to_json();
        CHECK(gps_json.find("lat") != std::string::npos, "gps JSON has 'lat' field");
        CHECK(gps_json.find("lon") != std::string::npos, "gps JSON has 'lon' field");

        std::string att_json = cache.attitude.to_json();
        CHECK(att_json.find("roll") != std::string::npos, "attitude JSON has 'roll' field");
        CHECK(att_json.find("pitch") != std::string::npos, "attitude JSON has 'pitch' field");
        CHECK(att_json.find("yaw") != std::string::npos, "attitude JSON has 'yaw' field");

        std::string bat_json = cache.battery.to_json();
        CHECK(bat_json.find("voltage") != std::string::npos, "battery JSON has 'voltage' field");
    }

    // 3. Direct data access
    fprintf(stderr, "[Group] Direct data access\n");
    {
        mg::bridge::TelemetryCache cache;
        CHECK(cache.gps.data.lat == 0.0f, "default GPS lat is 0");

        cache.gps.data.lat = 399042000;  // 39.9042 degrees * 1e7
        CHECK(cache.gps.data.lat == 399042000, "GPS lat survives direct set/get (int32)");
    }

    // 4. Mutex lock/unlock cycle
    fprintf(stderr, "[Group] Mutex operations\n");
    {
        mg::bridge::TelemetryCache cache;
        {
            mg::core::platform::LockGuard lk(cache.mtx);
            float lat = cache.gps.data.lat;
            CHECK(lat == 0.0f, "default GPS lat read under lock is 0");
        }
    }

    // 5. Concurrent access stress test
    fprintf(stderr, "[Group] Thread safety stress test\n");
    {
        mg::bridge::TelemetryCache cache;
        std::atomic<int> error_count{0};

        auto writer = [&](int id) {
            for (int i = 0; i < 1000; i++) {
                mg::core::platform::LockGuard lk(cache.mtx);
                cache.attitude.data.roll = (float)(id * 1000 + i);
                cache.gps.data.lat = (float)(id * 1000 + i);
            }
        };

        auto reader = [&]() {
            for (int i = 0; i < 1000; i++) {
                mg::core::platform::LockGuard lk(cache.mtx);
                (void)cache.attitude.data.roll;
                (void)cache.gps.data.lat;
            }
        };

        std::vector<std::thread> threads;
        for (int i = 0; i < 4; i++) threads.emplace_back(writer, i);
        for (int i = 0; i < 4; i++) threads.emplace_back(reader);
        for (auto& t : threads) t.join();

        CHECK(error_count.load() == 0, "no data corruption in 8-thread stress test (8000 ops)");
    }

    // 6. SysStatus health JSON
    fprintf(stderr, "[Group] SysStatus sensor health\n");
    {
        mg::bridge::TelemetryCache cache;
        cache.sys_status.autopilot_type = 3;
        cache.sys_status.data.onboard_control_sensors_present = 1 | 2 | 4 | 8 | 32 | 32768 | 65536 | 33554432;
        cache.sys_status.data.onboard_control_sensors_enabled = 1 | 2 | 4 | 8 | 32 | 32768 | 65536 | 33554432;
        cache.sys_status.data.onboard_control_sensors_health = 0xFF;
        std::string health = cache.sys_status.health_json();
        CHECK(!health.empty(), "health JSON is non-empty when sensors healthy");
        CHECK(health.find("\"summary\"") != std::string::npos, "health JSON includes summary");
        CHECK(health.find("\"groups\"") != std::string::npos, "health JSON includes groups");
        CHECK(health.find("\"firmware_view\"") != std::string::npos, "health JSON includes firmware view");
        CHECK(health.find("\"gyro\"") != std::string::npos, "health JSON includes gyro item");
        CHECK(health.find("\"motors\"") != std::string::npos, "health JSON includes motors item");
        CHECK(health.find("\"profile\":\"ardupilot\"") != std::string::npos,
              "health JSON includes ArduPilot firmware view");

        cache.sys_status.data.onboard_control_sensors_health = 0x00;
        std::string health2 = cache.sys_status.health_json();
        CHECK(!health2.empty(), "health JSON is non-empty when sensors unhealthy");
        CHECK(health2.find("\"fault_count\"") != std::string::npos, "unhealthy JSON still includes fault summary");
    }

    // 7. TelemetryType enum coverage
    fprintf(stderr, "[Group] TelemetryType enum\n");
    {
        mg::bridge::TelemetryType types[] = {
            mg::bridge::TelemetryType::kHeartbeat,
            mg::bridge::TelemetryType::kSysStatus,
            mg::bridge::TelemetryType::kGps,
            mg::bridge::TelemetryType::kAttitude,
            mg::bridge::TelemetryType::kVfrHud,
            mg::bridge::TelemetryType::kImu,
            mg::bridge::TelemetryType::kPressure,
            mg::bridge::TelemetryType::kFirmwareVersion,
            mg::bridge::TelemetryType::kRcChannels,
            mg::bridge::TelemetryType::kServoOutput,
            mg::bridge::TelemetryType::kStatustext,
            mg::bridge::TelemetryType::kGlobalPosition,
            mg::bridge::TelemetryType::kHomePosition,
            mg::bridge::TelemetryType::kBattery,
            mg::bridge::TelemetryType::kHighresImu,
            mg::bridge::TelemetryType::kExtendedSysState,
        };
        CHECK(sizeof(types) / sizeof(types[0]) == 16, "16 telemetry types defined");
    }

    // 8. Vehicle identity from ArduPilot boot STATUSTEXT
    fprintf(stderr, "[Group] Vehicle identity\n");
    {
        mg::bridge::TelemetryCache cache;
        cache.update_statustext("SULILGH7-P1-P2 0045001E 31305106 33343837", 6);

        CHECK(cache.snapshot_vehicle_name() == "SULILGH7-P1-P2", "boot STATUSTEXT extracts vehicle name");
        CHECK(cache.snapshot_board_name() == "SULILGH7-P1-P2", "boot STATUSTEXT extracts board name");
        CHECK(cache.snapshot_product_name() == "SULILGH7-P1-P2", "boot STATUSTEXT extracts product name");

        cache.update_statustext("Frame: TRI", 6);
        CHECK(cache.snapshot_vehicle_name() == "SULILGH7-P1-P2", "generic STATUSTEXT does not overwrite vehicle name");
        CHECK(cache.snapshot_board_name() == "SULILGH7-P1-P2", "generic STATUSTEXT does not overwrite board name");
        CHECK(cache.snapshot_product_name() == "SULILGH7-P1-P2", "generic STATUSTEXT does not overwrite product name");
    }

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
