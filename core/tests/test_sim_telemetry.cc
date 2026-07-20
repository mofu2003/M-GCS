/**
 * test_sim_telemetry.cc - Telemetry simulation tests
 *
 * Uses MockFlightController to verify: heartbeat detection, GPS, attitude,
 * battery, VFR HUD telemetry flows through InMemoryLink → MavlinkAdapter.
 */
#include "mock_flight_controller.h"
#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include <cstdio>
#include <thread>
#include <chrono>

static int g_pass=0, g_fail=0;
#define CHECK(m,c) do{if(c){g_pass++;fprintf(stderr,"  [PASS] %s\n",m);}else{g_fail++;fprintf(stderr,"  [FAIL] %s\n",m);}}while(0)
#define CHECK_NEAR(n,a,b,e) CHECK(n, std::abs((a)-(b))<(e))

struct TestEnv {
    mg::core::transport::InMemoryLink link;
    mg::core::mavlink::MavlinkAdapter gcs;
    MockFlightController* mock = nullptr;

    TestEnv(MockFlightController::Config cfg = {}) {
        link.open();
        gcs.set_send_callback([this](const uint8_t* data, size_t size) -> bool {
            link.send(data, size);
            return true;
        });
        link.set_receive_callback([this](const uint8_t* data, size_t size) {
            gcs.feed_raw(data, size);
        });
        mock = new MockFlightController(link, cfg);
    }
    ~TestEnv() { delete mock; }

    // Drive mock + flush GCS→FC to mock
    void tick(int ms) {
        mock->tick(ms);
        auto out = link.take_outgoing();
        if (!out.empty()) mock->on_gcs_data(out.data(), out.size());
    }
};

static void test_heartbeat() {
    fprintf(stderr, "\n[Group] Heartbeat detection\n");
    TestEnv env;
    bool received = false;
    mg::core::mavlink::HeartbeatInfo hb;
    env.gcs.on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo& h) { received=true; hb=h; });

    env.tick(1100);  // trigger 1 Hz heartbeat
    CHECK("heartbeat received", received);
    CHECK("sysid=1", hb.system_id == 1);
    CHECK("autopilot=ArduPilot", hb.autopilot_type == 3);
    CHECK("vehicle=quadrotor", hb.vehicle_type == 2);
    CHECK("not armed", !hb.armed);
}

static void test_gps() {
    fprintf(stderr, "\n[Group] GPS telemetry\n");
    TestEnv env;
    bool received = false;
    mg::core::mavlink::GpsInfo gps;
    env.gcs.on_gps([&](const mg::core::mavlink::GpsInfo& g) { received=true; gps=g; });

    env.tick(200);  // 10 Hz telemetry
    CHECK("GPS received", received);
    CHECK("lat=Beijing", gps.lat == 399042000);
    CHECK("lon=Beijing", gps.lon == 1164074000);
    CHECK("sats=12", gps.satellites_visible == 12);
    CHECK("fix=3D", gps.fix_type == 3);
}

static void test_attitude() {
    fprintf(stderr, "\n[Group] Attitude telemetry\n");
    TestEnv env;
    env.mock->telem.roll = 0.25f;
    env.mock->telem.pitch = -0.1f;
    env.mock->telem.yaw = 1.57f;

    bool received = false;
    mg::core::mavlink::AttitudeInfo att;
    env.gcs.on_attitude([&](const mg::core::mavlink::AttitudeInfo& a) { received=true; att=a; });

    env.tick(150);
    CHECK("attitude received", received);
    CHECK_NEAR("roll", att.roll, 0.25f, 0.01f);
    CHECK_NEAR("pitch", att.pitch, -0.1f, 0.01f);
    CHECK_NEAR("yaw", att.yaw, 1.57f, 0.02f);
}

static void test_battery() {
    fprintf(stderr, "\n[Group] Battery telemetry\n");
    TestEnv env;
    env.mock->telem.voltage_v = 11.4f;
    env.mock->telem.battery_remaining = 65;

    bool received = false;
    mg::core::mavlink::BatteryInfo batt;
    env.gcs.on_battery([&](const mg::core::mavlink::BatteryInfo& b) { received=true; batt=b; });

    env.tick(1100);  // 1 Hz
    CHECK("battery received", received);
    CHECK_NEAR("voltage", batt.voltage_v, 11.4f, 0.1f);
    CHECK("remaining", batt.battery_remaining == 65);
}

static void test_vfr_hud() {
    fprintf(stderr, "\n[Group] VFR HUD\n");
    TestEnv env;
    env.mock->telem.airspeed = 8.5f;
    env.mock->telem.groundspeed = 7.2f;
    env.mock->telem.throttle = 55;
    env.mock->telem.heading = 27000;
    env.mock->telem.climb_m_s = 1.5f;

    bool received = false;
    mg::core::mavlink::VfrHudInfo vfr;
    env.gcs.on_vfr_hud([&](const mg::core::mavlink::VfrHudInfo& v) { received=true; vfr=v; });

    env.tick(150);
    CHECK("VFR received", received);
    CHECK_NEAR("airspeed", vfr.airspeed, 8.5f, 0.1f);
    CHECK_NEAR("groundspeed", vfr.groundspeed, 7.2f, 0.1f);
    CHECK("throttle=55", vfr.throttle == 55);
    CHECK_NEAR("climb", vfr.climb, 1.5f, 0.1f);
}

static void test_global_position() {
    fprintf(stderr, "\n[Group] Global position\n");
    TestEnv env;
    env.mock->telem.vx = 300;  // cm/s
    env.mock->telem.vy = -150;
    env.mock->telem.vz = 20;

    bool received = false;
    mg::core::mavlink::GlobalPositionInfo gpos;
    env.gcs.on_global_position([&](const mg::core::mavlink::GlobalPositionInfo& gp) { received=true; gpos=gp; });

    env.tick(150);
    CHECK("global pos received", received);
    CHECK("lat matches", gpos.lat == 399042000);
    CHECK("lon matches", gpos.lon == 1164074000);
    CHECK("alt matches", gpos.alt == 50000);
}

static void test_no_heartbeat_when_disabled() {
    fprintf(stderr, "\n[Group] Heartbeat disabled\n");
    TestEnv env;
    env.mock->set_heartbeat_enabled(false);
    bool received = false;
    env.gcs.on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo&) { received=true; });
    env.tick(2000);
    CHECK("no heartbeat when disabled", !received);
}

int main() {
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    fprintf(stderr, "=== Telemetry Simulation Tests ===\n");
    test_heartbeat();
    test_gps();
    test_attitude();
    test_battery();
    test_vfr_hud();
    test_global_position();
    test_no_heartbeat_when_disabled();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
