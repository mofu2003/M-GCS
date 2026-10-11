/**
 * test_mavlink_protocol.cc - MAVLink protocol layer tests (no hardware)
 *
 * Tests MavlinkAdapter encode/decode. Pattern inspired by QGC MockLink + MP.
 *
 * Covers: Heartbeat (AP+PX4), Param protocol, Command ACK, GPS, Attitude,
 *   RC, Servo, Pressure, Battery, Vibration, Statustext, GlobalPos, HomePos,
 *   Multi-message interleave, Heartbeat watchdog, Send path.
 */

#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include "ardupilotmega/mavlink.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <algorithm>
#include <array>

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(name, expr) do { \
    if (expr) { g_passed++; fprintf(stderr, "  [PASS] %s\n", name); } \
    else { g_failed++; fprintf(stderr, "  [FAIL] %s\n", name); } \
} while(0)

static std::vector<uint8_t> pack_msg(mavlink_message_t& msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    return std::vector<uint8_t>(buf, buf + len);
}

struct TestFixture {
    mg::core::transport::InMemoryLink link;
    mg::core::mavlink::MavlinkAdapter adapter;
    TestFixture() {
        link.open();
        link.set_receive_callback([this](const uint8_t* data, size_t size) {
            adapter.feed_raw(data, size);
        });
        adapter.set_send_callback([this](const uint8_t* data, size_t size) -> bool {
            return link.send(data, size);
        });
    }
    void inject(mavlink_message_t& msg) {
        auto bytes = pack_msg(msg);
        link.inject_incoming(bytes.data(), bytes.size());
    }
};

static void test_heartbeat_ardupilot_fw() {
    fprintf(stderr, "\n[Group] Heartbeat - ArduPilot FixedWing\n");
    TestFixture f;
    bool received = false;
    mg::core::mavlink::HeartbeatInfo last;
    f.adapter.on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo& hb) { received = true; last = hb; });
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_FIXED_WING, MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 17, MAV_STATE_STANDBY);
    f.inject(msg);
    CHECK("AP FW heartbeat received", received);
    CHECK("AP FW sysid=1", last.system_id == 1);
    CHECK("AP FW type=1 (fixed_wing)", last.vehicle_type == 1);
    CHECK("AP FW autopilot=3", last.autopilot_type == 3);
    CHECK("AP FW not armed", !last.armed);
    CHECK("AP FW custom_mode=17", last.custom_mode == 17);
    CHECK("AP FW flight_mode not empty", strlen(last.flight_mode) > 0);
}

static void test_heartbeat_px4_copter() {
    fprintf(stderr, "\n[Group] Heartbeat - PX4 Copter Armed\n");
    TestFixture f;
    bool received = false;
    mg::core::mavlink::HeartbeatInfo last;
    f.adapter.on_heartbeat([&](const mg::core::mavlink::HeartbeatInfo& hb) { received = true; last = hb; });
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_PX4,
        MAV_MODE_FLAG_SAFETY_ARMED | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 50593792, MAV_STATE_ACTIVE);
    f.inject(msg);
    CHECK("PX4 heartbeat received", received);
    CHECK("PX4 autopilot=12", last.autopilot_type == 12);
    CHECK("PX4 type=2 (quadrotor)", last.vehicle_type == 2);
    CHECK("PX4 armed", last.armed);
    CHECK("PX4 flight_mode decoded", strlen(last.flight_mode) > 0);
}

static void test_param_value_decode() {
    fprintf(stderr, "\n[Group] Param Protocol - PARAM_VALUE\n");
    TestFixture f;
    int cnt = 0;
    f.adapter.on_param([&](const mg::core::mavlink::ParamInfo&) { cnt++; });
    mavlink_message_t msg;
    auto pack_param = [&](const char* name, float value, uint8_t type,
                          uint16_t count, uint16_t index) {
        mavlink_param_value_t pv{};
        std::snprintf(pv.param_id, sizeof(pv.param_id), "%s", name);
        pv.param_value = value;
        pv.param_type = type;
        pv.param_count = count;
        pv.param_index = index;
        mavlink_msg_param_value_encode(1, 1, &msg, &pv);
        f.inject(msg);
    };
    pack_param("SERVO5_FUNCTION", 4.0f, MAV_PARAM_TYPE_UINT16, 3, 0);
    pack_param("ARMING_CHECK", 1.0f, MAV_PARAM_TYPE_INT32, 3, 1);
    pack_param("BATT_CAPACITY", 3300.0f, MAV_PARAM_TYPE_REAL32, 3, 2);
    CHECK("3 params received", cnt == 3);
    CHECK("params_received=3", f.adapter.params_received() == 3);
    auto p1 = f.adapter.get_param_by_name("SERVO5_FUNCTION");
    CHECK("SERVO5_FUNCTION found", p1.has_value());
    CHECK("SERVO5_FUNCTION=4", p1.has_value() && p1->value == 4.0f);
    CHECK("SERVO5_FUNCTION type UINT16", p1.has_value() && p1->type == MAV_PARAM_TYPE_UINT16);
    auto p2 = f.adapter.get_param_by_name("BATT_CAPACITY");
    CHECK("BATT_CAPACITY=3300", p2.has_value() && p2->value == 3300.0f);
    CHECK("NONEXISTENT=nullopt", !f.adapter.get_param_by_name("NONEXISTENT").has_value());
    CHECK("all_params()=3", f.adapter.all_params().size() == 3);
}

static void test_param_name_encoding() {
    fprintf(stderr, "\n[Group] Param Name Encoding Safety\n");
    TestFixture f;
    mavlink_message_t msg;
    mavlink_param_value_t pv;
    memset(&pv, 0, sizeof(pv));
    memcpy(pv.param_id, "SERIAL6_PROTOCOL", 16);
    pv.param_value = 2.0f;
    pv.param_type = MAV_PARAM_TYPE_UINT8;
    pv.param_index = 99;
    pv.param_count = 100;
    mavlink_msg_param_value_encode(1, 1, &msg, &pv);
    f.inject(msg);
    auto p = f.adapter.get_param_by_name("SERIAL6_PROTOCOL");
    CHECK("16-char param decoded", p.has_value());
    CHECK("value=2", p.has_value() && p->value == 2.0f);
    if (p.has_value()) {
        CHECK("name length=16", p->name.length() == 16);
        CHECK("name matches exactly", p->name == "SERIAL6_PROTOCOL");
    }
}

static void test_param_error_rejects_pending_write() {
    fprintf(stderr, "\n[Group] Param Protocol - PARAM_ERROR\n");
    TestFixture f;
    const auto status = f.adapter.set_param_status(
        1, 1, "TEST_PARAM", 5.0f, MAV_PARAM_TYPE_REAL32);
    CHECK("PARAM_SET accepted", static_cast<bool>(status));

    mavlink_message_t msg;
    mavlink_msg_param_error_pack(
        1, 1, &msg, 255, 1, "TEST_PARAM", -1,
        MAV_PARAM_ERROR_VALUE_OUT_OF_RANGE);
    f.inject(msg);

    const auto history = f.adapter.param_change_history();
    const auto record = std::find_if(
        history.begin(), history.end(), [](const auto& item) {
            return item.name == "TEST_PARAM";
        });
    CHECK("PARAM_ERROR leaves a write record", record != history.end());
    CHECK("PARAM_ERROR marks write failed",
          record != history.end() && record->state == "failed");
    CHECK("PARAM_ERROR keeps rejection reason",
          record != history.end() &&
              record->error.find("out of range") != std::string::npos);
}

static void test_command_ack() {
    fprintf(stderr, "\n[Group] Command ACK - queued command\n");
    TestFixture f;
    bool ack_cb = false;
    f.adapter.on_command_ack([&](const mg::core::mavlink::CommandAckInfo&) { ack_cb = true; });

    const auto ticket = f.adapter.send_command_queued(
        1, 1, MAV_CMD_COMPONENT_ARM_DISARM,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

    std::atomic<bool> got_ack{false};
    std::atomic<uint8_t> ack_result{255};
    std::thread waiter([&]() {
        auto result = f.adapter.wait_for_command(ticket, 2000);
        if (result.has_value()) { got_ack = true; ack_result = result->result; }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mavlink_message_t msg;
    mavlink_msg_command_ack_pack(1, 1, &msg, MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    f.inject(msg);
    waiter.join();
    f.adapter.tick();  // 回调在锁外派发
    CHECK("ACK callback fired", ack_cb);
    CHECK("queued command returned the ACK", got_ack.load());
    CHECK("ACK result ACCEPTED", ack_result.load() == MAV_RESULT_ACCEPTED);
}

static void test_command_ack_timeout() {
    fprintf(stderr, "\n[Group] Command queue - wait timeout\n");
    TestFixture f;
    const auto ticket = f.adapter.send_command_queued(
        1, 1, MAV_CMD_NAV_WAYPOINT, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    auto start = std::chrono::steady_clock::now();
    auto result = f.adapter.wait_for_command(ticket, 200);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    CHECK("waiting without an ACK returns nullopt", !result.has_value());
    CHECK("wait respected the timeout", elapsed >= 150 && elapsed < 500);
    CHECK("an unacknowledged command can still be cancelled",
          f.adapter.cancel_command(ticket));
}

static void test_gps_decode() {
    fprintf(stderr, "\n[Group] GPS_RAW_INT decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_gps([&](const mg::core::mavlink::GpsInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_gps_raw_int_t g = {}; g.fix_type=3; g.lat=310000000; g.lon=1210000000; g.alt=100000; g.vel=150; g.cog=2500; g.satellites_visible=12; mavlink_msg_gps_raw_int_encode(1,1,&msg,&g); }
    f.inject(msg);
    CHECK("GPS received", received);
    auto gps = f.adapter.latest_gps();
    CHECK("fix_type=3 (3D)", gps.has_value() && gps->fix_type == 3);
    CHECK("lat=310000000", gps.has_value() && gps->lat == 310000000);
    CHECK("satellites=12", gps.has_value() && gps->satellites_visible == 12);
}

static void test_attitude_decode() {
    fprintf(stderr, "\n[Group] ATTITUDE decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_attitude([&](const mg::core::mavlink::AttitudeInfo&) { received = true; });
    mavlink_message_t msg;
    mavlink_msg_attitude_pack(1, 1, &msg, 0, 0.05f, -0.02f, 1.57f, 0.01f, -0.005f, 0.002f);
    f.inject(msg);
    CHECK("Attitude received", received);
    auto att = f.adapter.latest_attitude();
    CHECK("roll ~0.05", att.has_value() && fabs(att->roll - 0.05f) < 0.001f);
    CHECK("pitch ~-0.02", att.has_value() && fabs(att->pitch + 0.02f) < 0.001f);
    CHECK("yaw ~1.57", att.has_value() && fabs(att->yaw - 1.57f) < 0.001f);
}

static void test_rc_channels_decode() {
    fprintf(stderr, "\n[Group] RC_CHANNELS decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_rc_channels([&](const mg::core::mavlink::RcChannelsInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_rc_channels_t r = {}; r.chancount=8; r.rssi=255; r.chan1_raw=1500; r.chan2_raw=1500; r.chan3_raw=1100; r.chan4_raw=1500; r.chan5_raw=1800; r.chan6_raw=1200; r.chan7_raw=1500; r.chan8_raw=1500; mavlink_msg_rc_channels_encode(1,1,&msg,&r); }
    f.inject(msg);
    CHECK("RC received", received);
    auto rc = f.adapter.latest_rc_channels();
    CHECK("chancount=8", rc.has_value() && rc->chancount == 8);
    CHECK("chan[0]=1500", rc.has_value() && rc->chan[0] == 1500);
    CHECK("chan[2]=1100", rc.has_value() && rc->chan[2] == 1100);
}

static void test_servo_output_decode() {
    fprintf(stderr, "\n[Group] SERVO_OUTPUT_RAW decode\n");
    TestFixture f;
    int received = 0;
    f.adapter.on_servo_output([&](const mg::core::mavlink::ServoOutputInfo&) { received++; });
    mavlink_message_t msg;
    { mavlink_servo_output_raw_t s = {}; s.time_usec=123456; s.port=0; s.servo1_raw=1500; s.servo2_raw=1500; s.servo3_raw=1100; s.servo4_raw=1500; s.servo5_raw=1800; s.servo6_raw=1200; s.servo7_raw=1500; s.servo8_raw=1500; mavlink_msg_servo_output_raw_encode(1,1,&msg,&s); }
    f.inject(msg);
    CHECK("Servo received", received == 1);
    auto sv = f.adapter.latest_servo_output();
    CHECK("servo[0]=1500", sv.has_value() && sv->servo[0] == 1500);
    CHECK("servo[2]=1100", sv.has_value() && sv->servo[2] == 1100);
    CHECK("time_usec decoded", sv.has_value() && sv->time_usec == 123456);

    { mavlink_servo_output_raw_t s = {}; s.time_usec=654321; s.port=1; s.servo1_raw=1900; mavlink_msg_servo_output_raw_encode(1,1,&msg,&s); }
    f.inject(msg);
    sv = f.adapter.latest_servo_output();
    CHECK("port 1 does not fire 1-16 callback", received == 1);
    CHECK("port 1 does not overwrite servo 1", sv.has_value() && sv->servo[0] == 1500);
    CHECK("port 1 does not overwrite timestamp", sv.has_value() && sv->time_usec == 123456);
}

static void test_statustext_decode() {
    fprintf(stderr, "\n[Group] STATUSTEXT decode\n");
    TestFixture f;
    int count = 0;
    f.adapter.on_statustext([&](const std::string&, uint8_t) { count++; });
    mavlink_message_t msg;
    mavlink_msg_statustext_pack(1, 1, &msg, 4, "PreArm: GPS 1 no fix", 0, 0);
    f.inject(msg);
    mavlink_msg_statustext_pack(1, 1, &msg, 6, "Flight mode LOITER", 0, 0);
    f.inject(msg);
    CHECK("2 statustexts", count == 2);
    CHECK("last text matches", f.adapter.last_statustext() == "Flight mode LOITER");
    CHECK("severity=6 (info)", f.adapter.last_statustext_severity() == 6);
}

static void test_global_position_decode() {
    fprintf(stderr, "\n[Group] GLOBAL_POSITION_INT decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_global_position([&](const mg::core::mavlink::GlobalPositionInfo&) { received = true; });
    mavlink_message_t msg;
    mavlink_msg_global_position_int_pack(1, 1, &msg, 0, 310000000, 1210000000, 100000, 50000, 100, 200, 50, 5400);
    f.inject(msg);
    CHECK("Global pos received", received);
    auto gp = f.adapter.latest_global_position();
    CHECK("lat=310000000", gp.has_value() && gp->lat == 310000000);
    CHECK("alt=100000mm", gp.has_value() && gp->alt == 100000);
    CHECK("hdg=5400", gp.has_value() && gp->hdg == 5400);
}

static void test_home_position_decode() {
    fprintf(stderr, "\n[Group] HOME_POSITION decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_home_position([&](const mg::core::mavlink::HomePositionInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_home_position_t h = {}; h.latitude=310000000; h.longitude=1210000000; h.altitude=50000; h.q[0]=1; mavlink_msg_home_position_encode(1,1,&msg,&h); }
    f.inject(msg);
    CHECK("Home pos received", received);
    auto hp = f.adapter.latest_home_position();
    CHECK("home lat=310000000", hp.has_value() && hp->latitude == 310000000);
    CHECK("home alt=50000mm", hp.has_value() && hp->altitude == 50000);
}

/// SCALED_IMU/2/3 是三条独立消息，分别代表 IMU1/2/3。
/// 适配器要把实例号传下去，同时保持 latest_imu() 只代表主 IMU。
static void test_scaled_imu_instances() {
    fprintf(stderr, "\n[Group] SCALED_IMU 1..3 instance mapping\n");
    TestFixture f;
    std::vector<uint8_t> instances;
    std::vector<int16_t> accel_x;
    f.adapter.on_imu([&](const mg::core::mavlink::ImuInfo& info) {
        instances.push_back(info.instance);
        accel_x.push_back(info.xacc);
    });

    mavlink_message_t msg;
    {
        mavlink_scaled_imu_t imu = {};
        imu.time_boot_ms = 1000;
        imu.xacc = 100;
        imu.xmag = 250;
        mavlink_msg_scaled_imu_encode(1, 1, &msg, &imu);
    }
    f.inject(msg);
    {
        mavlink_scaled_imu2_t imu = {};
        imu.time_boot_ms = 1000;
        imu.xacc = 200;
        mavlink_msg_scaled_imu2_encode(1, 1, &msg, &imu);
    }
    f.inject(msg);
    {
        mavlink_scaled_imu3_t imu = {};
        imu.time_boot_ms = 1000;
        imu.xacc = 300;
        mavlink_msg_scaled_imu3_encode(1, 1, &msg, &imu);
    }
    f.inject(msg);

    CHECK("three IMU frames delivered", instances.size() == 3);
    CHECK("SCALED_IMU -> instance 0", instances.size() == 3 && instances[0] == 0);
    CHECK("SCALED_IMU2 -> instance 1", instances.size() == 3 && instances[1] == 1);
    CHECK("SCALED_IMU3 -> instance 2", instances.size() == 3 && instances[2] == 2);
    CHECK("instance payloads stay attributed to their message",
          accel_x.size() == 3 && accel_x[0] == 100 && accel_x[1] == 200 && accel_x[2] == 300);

    auto primary = f.adapter.latest_imu();
    CHECK("latest_imu() still reports the primary IMU",
          primary.has_value() && primary->instance == 0 && primary->xacc == 100);
    CHECK("latest_imu() keeps the primary magnetometer reading",
          primary.has_value() && primary->xmag == 250);
}

static void test_pressure_decode() {
    fprintf(stderr, "\n[Group] SCALED_PRESSURE decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_pressure([&](const mg::core::mavlink::PressureInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_scaled_pressure_t p = {}; p.press_abs=1013.25f; p.temperature=2500; mavlink_msg_scaled_pressure_encode(1,1,&msg,&p); }
    f.inject(msg);
    CHECK("Pressure received", received);
    auto pr = f.adapter.latest_pressure();
    CHECK("abs ~1013.25 hPa", pr.has_value() && fabs(pr->press_abs - 1013.25f) < 0.1f);
    CHECK("temp=2500 (25C)", pr.has_value() && pr->temperature == 2500);
}

static void test_sys_status_decode() {
    fprintf(stderr, "\n[Group] SYS_STATUS decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_sys_status([&](const mg::core::mavlink::SysStatusInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_sys_status_t s = {}; s.onboard_control_sensors_present=0x1FFF; s.onboard_control_sensors_enabled=0x14001; s.onboard_control_sensors_health=0x31FFEF; s.voltage_battery=12600; mavlink_msg_sys_status_encode(1,1,&msg,&s); }
    f.inject(msg);
    CHECK("SYS_STATUS received", received);
    auto ss = f.adapter.latest_sys_status();
    CHECK("sensors_present=0x1FFF", ss.has_value() && ss->onboard_control_sensors_present == 0x1FFF);
    CHECK("voltage=12600", ss.has_value() && ss->battery_voltage == 12600);
}
static void test_battery_status_decode() {
    fprintf(stderr, "\n[Group] BATTERY_STATUS decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_battery([&](const mg::core::mavlink::BatteryInfo&) { received = true; });
    mavlink_message_t msg;
    mavlink_battery_status_t b; memset(&b, 0, sizeof(b));
    b.battery_function = MAV_BATTERY_FUNCTION_ALL;
    b.type = MAV_BATTERY_TYPE_LION;
    b.voltages[0] = 12600; b.voltages[1] = UINT16_MAX; // single cell 12.6V
    b.current_battery = 1500;  // 15A
    b.current_consumed = 2500; // 2500mAh
    b.battery_remaining = 90;  // 90%
    mavlink_msg_battery_status_encode(1, 1, &msg, &b);
    f.inject(msg);
    CHECK("Battery received", received);
    auto bat = f.adapter.latest_battery();
    CHECK("voltage ~12.6V", bat.has_value() && fabs(bat->voltage_v - 12.6f) < 0.1f);
    CHECK("current ~15A", bat.has_value() && fabs(bat->current_a - 15.0f) < 0.5f);
    CHECK("consumed=2500mAh", bat.has_value() && bat->consumed_mah == 2500);
    CHECK("remaining=90", bat.has_value() && bat->battery_remaining == 90);
}

static void test_vibration_decode() {
    fprintf(stderr, "\n[Group] VIBRATION decode\n");
    TestFixture f;
    bool received = false;
    f.adapter.on_vibration([&](const mg::core::mavlink::VibrationInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_vibration_t v = {}; v.vibration_x=0.5f; v.vibration_y=0.3f; v.vibration_z=0.8f; v.clipping_0=10; v.clipping_1=20; v.clipping_2=30; mavlink_msg_vibration_encode(1,1,&msg,&v); }
    f.inject(msg);
    CHECK("Vibration received", received);
    auto vib = f.adapter.latest_vibration();
    CHECK("vib_x ~0.5", vib.has_value() && fabs(vib->vibration_x - 0.5f) < 0.01f);
    CHECK("clipping_0=10", vib.has_value() && vib->clipping_0 == 10);
}

static void test_multi_message_interleave() {
    fprintf(stderr, "\n[Group] Multi-message interleave (real FC traffic)\n");
    TestFixture f;
    int hb_cnt = 0, att_cnt = 0, gps_cnt = 0, param_cnt = 0;
    f.adapter.on_heartbeat([&](const auto&) { hb_cnt++; });
    f.adapter.on_attitude([&](const auto&) { att_cnt++; });
    f.adapter.on_gps([&](const auto&) { gps_cnt++; });
    f.adapter.on_param([&](const auto&) { param_cnt++; });
    for (int i = 0; i < 10; i++) {
        mavlink_message_t msg;
        mavlink_msg_attitude_pack(1, 1, &msg, i * 100, 0.01f * i, -0.005f * i, 1.57f, 0, 0, 0);
        f.inject(msg);
        if (i % 2 == 0) {
            { mavlink_gps_raw_int_t g = {}; g.fix_type=3; g.lat=310000000+i*100; g.lon=1210000000; g.alt=100000; g.satellites_visible=12; mavlink_msg_gps_raw_int_encode(1,1,&msg,&g); }
            f.inject(msg);
        }
        if (i == 0) {
            mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_SAFETY_ARMED | MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0, MAV_STATE_ACTIVE);
            f.inject(msg);
        }
        if (i == 5) {
            mavlink_msg_param_value_pack(1, 1, &msg, "ATC_RAT_PIT_P", 0.08f, MAV_PARAM_TYPE_REAL32, 0, 1);
            f.inject(msg);
        }
    }
    CHECK("10 attitudes", att_cnt == 10);
    CHECK("5 GPS updates", gps_cnt == 5);
    CHECK("1 heartbeat", hb_cnt == 1);
    CHECK("1 param", param_cnt == 1);
    CHECK("msg_count=17", f.adapter.message_count() == 17);
}

static void test_heartbeat_alive() {
    fprintf(stderr, "\n[Group] Heartbeat alive/age\n");
    TestFixture f;
    CHECK("age=0 initially", f.adapter.heartbeat_age_ms() == 0);
    CHECK("alive=false initially", !f.adapter.heartbeat_alive(5000));
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_STANDBY);
    f.inject(msg);
    CHECK("alive=true after HB", f.adapter.heartbeat_alive(5000));
    CHECK("age < 100ms", f.adapter.heartbeat_age_ms() < 100);
}

static void test_link_quality() {
    fprintf(stderr, "\n[Group] MAVLink link quality\n");
    TestFixture f;
    mavlink_message_t msg = {};

    auto inject_heartbeat = [&](uint8_t system_id, uint8_t component_id, uint8_t sequence) {
        mavlink_get_channel_status(MAVLINK_COMM_1)->current_tx_seq = sequence;
        mavlink_msg_heartbeat_pack_chan(
            system_id, component_id, MAVLINK_COMM_1, &msg, MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_STANDBY);
        f.inject(msg);
    };

    inject_heartbeat(1, 1, 10);
    inject_heartbeat(1, 1, 11);
    inject_heartbeat(1, 1, 14);
    const auto missing = f.adapter.link_quality();
    CHECK("link has enough samples", !missing.measuring);
    CHECK("link received=3", missing.received == 3);
    CHECK("link lost=2", missing.lost == 2);
    CHECK("link quality=60", missing.quality_pct == 60);

    TestFixture wrapping;
    auto inject_wrapping_heartbeat = [&](uint8_t sequence) {
        mavlink_get_channel_status(MAVLINK_COMM_1)->current_tx_seq = sequence;
        mavlink_msg_heartbeat_pack_chan(
            1, 1, MAVLINK_COMM_1, &msg, MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_ARDUPILOTMEGA,
            0, 0, MAV_STATE_STANDBY);
        wrapping.inject(msg);
    };
    inject_wrapping_heartbeat(254);
    inject_wrapping_heartbeat(255);
    inject_wrapping_heartbeat(0);
    const auto wrapped = wrapping.adapter.link_quality();
    CHECK("link wrap has enough samples", !wrapped.measuring);
    CHECK("link wrap has no loss", wrapped.lost == 0);
    CHECK("link wrap quality=100", wrapped.quality_pct == 100);
}

std::vector<uint8_t> pack_signed_heartbeat(
    uint8_t system_id,
    const std::array<uint8_t, 32>& key,
    uint8_t link_id,
    uint64_t timestamp_us) {
    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(
        system_id, 1, &message, MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        0, MAV_STATE_ACTIVE);

    mavlink_signing_t signing{};
    mavlink_signing_streams_t streams{};
    mavlink_status_t status{};
    signing.flags = MAVLINK_SIGNING_FLAG_SIGN_OUTGOING;
    signing.link_id = link_id;
    signing.timestamp = timestamp_us;
    std::copy(key.begin(), key.end(), signing.secret_key);
    status.signing = &signing;
    status.signing_streams = &streams;

    const auto* entry = mavlink_get_msg_entry(message.msgid);
    mavlink_finalize_message_buffer(
        &message, message.sysid, message.compid, &status,
        entry->min_msg_len, message.len, entry->crc_extra);
    return pack_msg(message);
}

static void test_signing_is_per_adapter() {
    fprintf(stderr, "\n[Group] Send path - MAVLink signing isolation\n");
    const std::array<uint8_t, 32> key = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80,
        0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x01,
    };

    TestFixture signed_link;
    TestFixture unsigned_link;
    CHECK("signed adapter accepts a valid signing key",
          signed_link.adapter.set_signing_key(key.data(), key.size()));
    CHECK("signing is enabled only on the configured adapter",
          signed_link.adapter.signing_enabled() &&
              !unsigned_link.adapter.signing_enabled());

    CHECK("signed adapter emits heartbeat",
          signed_link.adapter.send_heartbeat());
    const auto signed_bytes = signed_link.link.take_outgoing();
    CHECK("signed outgoing frame carries the signing flag",
          signed_bytes.size() > 2 &&
              (signed_bytes[2] & MAVLINK_IFLAG_SIGNED) != 0);

    const auto unsigned_heartbeat = pack_signed_heartbeat(
        9, key, 42, 1);
    std::vector<uint8_t> unsigned_bytes;
    {
        mavlink_message_t message{};
        mavlink_msg_heartbeat_pack(
            9, 1, &message, MAV_TYPE_QUADROTOR,
            MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
            0, MAV_STATE_ACTIVE);
        unsigned_bytes = pack_msg(message);
    }
    CHECK("signed adapter rejects unsigned inbound traffic",
          !signed_link.adapter.feed_raw(unsigned_bytes.data(),
                                        unsigned_bytes.size()));
    CHECK("unsigned adapter still accepts unsigned traffic",
          unsigned_link.adapter.feed_raw(unsigned_bytes.data(),
                                         unsigned_bytes.size()));

    constexpr uint64_t kSigningEpochMicros = 1420070400000000ULL;
    const uint64_t now_micros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    const uint64_t timestamp_ticks = (now_micros - kSigningEpochMicros) / 10ULL;
    const auto signed_heartbeat =
        pack_signed_heartbeat(9, key, 42, timestamp_ticks);
    CHECK("signed adapter accepts matching signed traffic",
          signed_link.adapter.feed_raw(signed_heartbeat.data(),
                                       signed_heartbeat.size()));
    const auto heartbeat = signed_link.adapter.latest_heartbeat();
    CHECK("signed heartbeat reaches telemetry",
          heartbeat.has_value() && heartbeat->system_id == 9);
    CHECK("signed adapter reports a valid signature",
          signed_link.adapter.signing_status() ==
              MAVLINK_SIGNING_STATUS_OK);

    signed_link.adapter.clear_signing_key();
    CHECK("clearing the key disables signing",
          !signed_link.adapter.signing_enabled());
    CHECK("cleared adapter accepts unsigned traffic again",
          signed_link.adapter.feed_raw(unsigned_bytes.data(),
                                       unsigned_bytes.size()));
    (void)unsigned_heartbeat;
}

static void test_signing_candidate_auto_detect() {
    fprintf(stderr, "\n[Group] Receive path - signing candidate auto-detect\n");
    const std::array<uint8_t, 32> key = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xba, 0xdc, 0xfe,
        0x0f, 0x1e, 0x2d, 0x3c, 0x4b, 0x5a, 0x69, 0x78,
        0x87, 0x96, 0xa5, 0xb4, 0xc3, 0xd2, 0xe1, 0xf0,
    };
    constexpr uint64_t kSigningEpochMicros = 1420070400000000ULL;
    const uint64_t now_micros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    const uint64_t timestamp_ticks = (now_micros - kSigningEpochMicros) / 10ULL;

    TestFixture fixture;
    mg::core::mavlink::SigningCandidate candidate;
    candidate.name = "KeyAlpha";
    candidate.key = key;
    fixture.adapter.set_signing_candidates({candidate});
    CHECK("candidate count is retained",
          fixture.adapter.signing_candidate_count() == 1);
    const auto first = pack_signed_heartbeat(21, key, 7, timestamp_ticks);
    CHECK("signed packet triggers candidate matching",
          fixture.adapter.feed_raw(first.data(), first.size()));
    CHECK("matching key is automatically enabled",
          fixture.adapter.signing_enabled());
    CHECK("auto-detected key name is retained",
          fixture.adapter.signing_key_name() == "KeyAlpha");
    CHECK("auto-detection is marked in status",
          fixture.adapter.signing_auto_detected());

    // The next matching packet goes through normal timestamp/stream checks.
    const auto second =
        pack_signed_heartbeat(21, key, 7, timestamp_ticks + 1);
    CHECK("auto-detected key validates later signed traffic",
          fixture.adapter.feed_raw(second.data(), second.size()));
    CHECK("auto-detected key reports OK after validation",
          fixture.adapter.signing_status() == MAVLINK_SIGNING_STATUS_OK);
}

static void test_signing_detection_rejects_replay_and_mixed_chunk() {
    std::array<uint8_t, 32> key{};
    key.fill(0x37);
    const auto now = static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    const auto ticks = (now - 1420070400000000ULL) / 10ULL;
    mg::core::mavlink::SigningCandidate candidate;
    candidate.name = "replay-test";
    candidate.key = key;
    TestFixture old;
    old.adapter.set_signing_candidates({candidate});
    const auto stale = pack_signed_heartbeat(21, key, 7, ticks - 12000000ULL);
    CHECK("old first signed heartbeat is rejected",
          !old.adapter.feed_raw(stale.data(), stale.size()));
    CHECK("stale packet does not update heartbeat", !old.adapter.latest_heartbeat());
    CHECK("stale packet does not select a key", !old.adapter.signing_enabled());

    TestFixture mixed;
    mixed.adapter.set_signing_candidates({candidate});
    int count = 0;
    mixed.adapter.on_heartbeat([&](const auto&) { ++count; });
    const auto valid = pack_signed_heartbeat(21, key, 7, ticks);
    auto chunk = valid;
    chunk.insert(chunk.end(), valid.begin(), valid.end());
    CHECK("valid first packet accepted", mixed.adapter.feed_raw(chunk.data(), chunk.size()));
    CHECK("duplicate first packet in same chunk is rejected", count == 1);
    CHECK("later replay is rejected", !mixed.adapter.feed_raw(valid.data(), valid.size()));
    CHECK("later replay does not update heartbeat", count == 1);
}

static void test_tx_sequence_is_isolated_per_adapter() {
    fprintf(stderr, "\n[Group] Send path - per-adapter sequence isolation\n");
    TestFixture first;
    TestFixture second;

    CHECK("first adapter sends first heartbeat", first.adapter.send_heartbeat());
    CHECK("second adapter sends first heartbeat", second.adapter.send_heartbeat());
    CHECK("first adapter sends second heartbeat", first.adapter.send_heartbeat());

    const auto first_frames = first.link.take_outgoing();
    const auto second_frames = second.link.take_outgoing();
    CHECK("both adapters produced MAVLink frames",
          first_frames.size() > 8 && second_frames.size() > 8);
    const uint8_t initial_sequence = first_frames[4];
    CHECK("each adapter starts its own TX sequence independently",
          second_frames[4] == initial_sequence);
    CHECK("one adapter advances only its own TX sequence",
          first_frames.size() > 25 && first_frames[25] == initial_sequence + 1);

    TestFixture third;
    CHECK("third adapter sends heartbeat", third.adapter.send_heartbeat());
    const auto third_frames = third.link.take_outgoing();
    CHECK("a later adapter does not inherit another adapter sequence",
          third_frames.size() > 8 && third_frames[4] == initial_sequence);
}

static void test_send_heartbeat() {
    fprintf(stderr, "\n[Group] Send path - GCS heartbeat\n");
    TestFixture f;
    bool ok = f.adapter.send_heartbeat();
    CHECK("send_heartbeat ok", ok);
    auto outgoing = f.link.take_outgoing();
    CHECK("outgoing bytes > 6", outgoing.size() > 6);
    mavlink_message_t msg = {};
    mavlink_status_t status = {};
    bool parsed = false;
    for (size_t i = 0; i < outgoing.size(); i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, outgoing[i], &msg, &status)) { parsed = true; break; }
    }
    CHECK("outgoing is valid MAVLink", parsed);
    CHECK("outgoing is HEARTBEAT (id=0)", parsed && msg.msgid == 0);
}

// A4 回归（ADR-0005）：持锁派发路径不得在 impl_->mtx 内调用 send_cb。
//
// 飞控发 MISSION_COUNT 时，adapter 会在持锁的 process() 里回 MISSION_REQUEST_INT。
// 这里把发送回调换成"发送过程中重入 adapter 状态查询"——那些调用要拿同一把锁，
// 发送若还在锁内就会自锁死。帧必须在 process() 出锁后才写出去。
static void test_send_callback_never_runs_under_the_adapter_lock() {
    fprintf(stderr, "\n[Group] A4 - send callback outside the adapter lock\n");

    mg::core::transport::InMemoryLink link;
    mg::core::mavlink::MavlinkAdapter adapter;
    link.open();
    link.set_receive_callback([&](const uint8_t* data, size_t size) {
        adapter.feed_raw(data, size);
    });

    int sends = 0;
    int reentries = 0;
    adapter.set_send_callback([&](const uint8_t* data, size_t size) -> bool {
        ++sends;
        // 重入：两个调用都要拿 impl_->mtx。
        adapter.latest_heartbeat();
        adapter.message_count();
        ++reentries;
        return link.send(data, size);
    });

    // 先由 GCS 发起下载：这一步本身就是公开发送路径的临时解锁发送。
    CHECK("mission download starts", adapter.mission_download(1, 1));
    (void)link.take_outgoing();  // 只统计随后由处理器发出的请求帧

    mavlink_message_t count{};
    mavlink_msg_mission_count_pack(1, 1, &count, 0, MAV_COMP_ID_AUTOPILOT1, 3,
                                   MAV_MISSION_TYPE_MISSION, 0);
    const auto bytes = pack_msg(count);
    link.inject_incoming(bytes.data(), bytes.size());

    mavlink_message_t msg{};
    mavlink_status_t status{};
    int requests = 0;
    const auto outgoing = link.take_outgoing();
    for (uint8_t byte : outgoing) {
        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status) &&
            msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT) {
            ++requests;
        }
    }

    CHECK("MISSION_REQUEST_INT is flushed after the lock is released", requests == 1);
    CHECK("the send callback ran", sends >= 1);
    CHECK("send callback re-entered adapter state without deadlocking",
          reentries == sends);
}

static void test_feed_raw_reports_valid_frames() {
    fprintf(stderr, "\n[Group] Receive path - valid frame detection\n");
    mg::core::mavlink::MavlinkAdapter adapter;

    const uint8_t noise[] = {0x01, 0x02, 0x03, 0x7f, 0xff};
    CHECK("noise is not valid MAVLink traffic",
          !adapter.feed_raw(noise, sizeof(noise)));

    mavlink_message_t msg{};
    mavlink_msg_heartbeat_pack(
        1, 1, &msg, MAV_TYPE_QUADROTOR, MAV_AUTOPILOT_ARDUPILOTMEGA,
        MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, 0, MAV_STATE_ACTIVE);
    const auto bytes = pack_msg(msg);
    CHECK("valid MAVLink frame is reported",
          adapter.feed_raw(bytes.data(), bytes.size()));
}

// QGC keeps parsing state per link; MP uses one parser per MAVLinkInterface.
// Split frames from independent links must never share MAVLINK_COMM_0 state.
static void test_independent_link_parsers() {
    fprintf(stderr, "\n[Group] Independent link parser state\n");
    mg::core::mavlink::MavlinkAdapter first, second;
    mavlink_message_t a{}, b{};
    mavlink_msg_heartbeat_pack(1, 1, &a, MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA, 1, 4, MAV_STATE_ACTIVE);
    mavlink_msg_heartbeat_pack(2, 1, &b, MAV_TYPE_FIXED_WING,
        MAV_AUTOPILOT_ARDUPILOTMEGA, 1, 15, MAV_STATE_ACTIVE);
    const auto bytes_a = pack_msg(a), bytes_b = pack_msg(b);
    const auto split_a = bytes_a.size() / 2, split_b = bytes_b.size() / 2;
    CHECK("link A partial frame", !first.feed_raw(bytes_a.data(), split_a));
    CHECK("link B partial frame", !second.feed_raw(bytes_b.data(), split_b));
    CHECK("link A completes its own frame",
          first.feed_raw(bytes_a.data() + split_a, bytes_a.size() - split_a));
    CHECK("link B completes its own frame",
          second.feed_raw(bytes_b.data() + split_b, bytes_b.size() - split_b));
    const auto hb_a = first.latest_heartbeat(), hb_b = second.latest_heartbeat();
    CHECK("link A contains only sysid 1", hb_a && hb_a->system_id == 1);
    CHECK("link B contains only sysid 2", hb_b && hb_b->system_id == 2);

    // Corruption on one link cannot poison the other or prevent recovery.
    auto corrupt = bytes_a;
    corrupt.back() ^= 0xff;
    CHECK("bad CRC is rejected", !first.feed_raw(corrupt.data(), corrupt.size()));
    CHECK("parser recovers after bad CRC", first.feed_raw(bytes_a.data(), bytes_a.size()));
    CHECK("other link still parses", second.feed_raw(bytes_b.data(), bytes_b.size()));
}


// ================================================================
// 相机 / 云台 / 视频流 设备状态缓存
//
// 这些消息此前只落日志、且 dispatcher 把 case 261/254 写成
// CAMERA_INFORMATION/CAMERA_CAPTURE_STATUS（实际分别是 STORAGE_INFORMATION/DEBUG），
// 导致相机状态永远收不到。这里同时钉住「正确的 ID 会被分发」与
// 「错误的 ID 不会被误当成相机消息」。
// ================================================================
static void test_camera_state_cache() {
    fprintf(stderr, "\n[Group] CAMERA_* state cache\n");
    TestFixture f;
    const uint8_t kSys = 1, kComp = MAV_COMP_ID_CAMERA;  // 100

    // CAMERA_INFORMATION (259)
    {
        mavlink_camera_information_t ci = {};
        ci.time_boot_ms = 1234;
        ci.firmware_version = 0x01020304;
        ci.focal_length = 3.5f;
        ci.sensor_size_h = 6.17f;
        ci.sensor_size_v = 4.55f;
        ci.flags = 0x00000003;  // CAPTURE_IMAGE | CAPTURE_VIDEO
        ci.resolution_h = 4000;
        ci.resolution_v = 3000;
        ci.lens_id = 2;
        ci.cam_definition_version = 7;
        ci.camera_device_id = 0;
        memcpy(ci.vendor_name, "ACME", 4);
        memcpy(ci.model_name, "CamX", 4);
        mavlink_message_t msg;
        mavlink_msg_camera_information_encode(kSys, kComp, &msg, &ci);
        f.inject(msg);
    }
    auto cameras = f.adapter.latest_cameras();
    CHECK("CAMERA_INFORMATION cached", cameras.size() == 1);
    if (!cameras.empty()) {
        CHECK("camera sysid/compid", cameras[0].system_id == kSys && cameras[0].component_id == kComp);
        CHECK("camera flags", cameras[0].flags == 0x3);
        CHECK("camera resolution_h", cameras[0].resolution_h == 4000);
        CHECK("camera focal_length", cameras[0].focal_length == 3.5f);
        CHECK("camera vendor_name", std::string(cameras[0].vendor_name) == "ACME");
        CHECK("camera model_name", std::string(cameras[0].model_name) == "CamX");
    }

    // CAMERA_SETTINGS (260)
    {
        mavlink_camera_settings_t cs = {};
        cs.time_boot_ms = 2000;
        cs.mode_id = 1;       // CAMERA_MODE_IMAGE
        cs.zoomLevel = 2.5f;
        cs.focusLevel = 0.25f;
        mavlink_message_t msg;
        mavlink_msg_camera_settings_encode(kSys, kComp, &msg, &cs);
        f.inject(msg);
    }
    auto settings = f.adapter.camera_settings(kSys, kComp);
    CHECK("CAMERA_SETTINGS cached", settings.has_value());
    if (settings) {
        CHECK("settings mode_id", settings->mode_id == 1);
        CHECK("settings zoom_level", settings->zoom_level == 2.5f);
    }

    // CAMERA_CAPTURE_STATUS (262)
    {
        mavlink_camera_capture_status_t st = {};
        st.time_boot_ms = 3000;
        st.image_status = 1;
        st.video_status = 1;
        st.image_count = 7;
        st.available_capacity = 1024.0f;
        mavlink_message_t msg;
        mavlink_msg_camera_capture_status_encode(kSys, kComp, &msg, &st);
        f.inject(msg);
    }
    auto capture = f.adapter.camera_capture_status(kSys, kComp);
    CHECK("CAMERA_CAPTURE_STATUS cached", capture.has_value());
    if (capture) {
        CHECK("capture recording", capture->video_status == 1);
        CHECK("capture image_status", capture->image_status == 1);
        CHECK("capture image_count", capture->image_count == 7);
        CHECK("capture available_capacity", capture->available_capacity == 1024.0f);
    }

    // STORAGE_INFORMATION (261)
    {
        mavlink_storage_information_t si = {};
        si.time_boot_ms = 4000;
        si.total_capacity = 32000.0f;
        si.used_capacity = 8000.0f;
        si.available_capacity = 24000.0f;
        si.storage_id = 0;
        si.storage_count = 1;
        si.status = 2;  // STORAGE_STATUS_READY
        strncpy(si.name, "SD", sizeof(si.name) - 1);
        mavlink_message_t msg;
        mavlink_msg_storage_information_encode(kSys, kComp, &msg, &si);
        f.inject(msg);
    }
    auto storage = f.adapter.camera_storage(kSys, kComp);
    CHECK("STORAGE_INFORMATION cached", storage.size() == 1);
    if (!storage.empty()) {
        CHECK("storage total", storage[0].total_capacity == 32000.0f);
        CHECK("storage name", std::string(storage[0].name) == "SD");
    }

    // CAMERA_IMAGE_CAPTURED (263) 用 image_index+1 抬高 photo 下限
    {
        mavlink_camera_image_captured_t ic = {};
        ic.time_boot_ms = 5000;
        ic.image_index = 9;
        mavlink_message_t msg;
        mavlink_msg_camera_image_captured_encode(kSys, kComp, &msg, &ic);
        f.inject(msg);
    }
    auto after = f.adapter.camera_capture_status(kSys, kComp);
    CHECK("CAMERA_IMAGE_CAPTURED raises photo count", after && after->image_count == 10);
}

// dispatcher 曾把 261/254 写错：261 应是 STORAGE_INFORMATION（不是 CAMERA_INFORMATION），
// 254 是 DEBUG（不是 CAMERA_CAPTURE_STATUS）。这里钉住两类 ID 不再互相串台。
static void test_camera_dispatcher_id_regression() {
    fprintf(stderr, "\n[Group] camera dispatcher ID regression (254/259/261/262)\n");
    TestFixture f;
    const uint8_t kSys = 1, kComp = 100;

    // 259 必须进 cameras；若 dispatcher 仍按旧代码把 259 当别的消息，这里会空。
    {
        mavlink_camera_information_t ci = {};
        ci.resolution_h = 1920;
        ci.resolution_v = 1080;
        mavlink_message_t msg;
        mavlink_msg_camera_information_encode(kSys, kComp, &msg, &ci);
        f.inject(msg);
    }
    CHECK("259 reaches camera cache", f.adapter.latest_cameras().size() == 1);

    // 261 是 STORAGE_INFORMATION：必须落在存储缓存，不能污染 cameras。
    {
        mavlink_storage_information_t si = {};
        si.storage_id = 0;
        si.total_capacity = 1000.0f;
        mavlink_message_t msg;
        mavlink_msg_storage_information_encode(kSys, kComp, &msg, &si);
        f.inject(msg);
    }
    CHECK("261 lands in storage, not cameras",
          f.adapter.camera_storage(kSys, kComp).size() == 1 &&
              f.adapter.latest_cameras().size() == 1);

    // 254 是 DEBUG：绝不能被当成 CAMERA_CAPTURE_STATUS。
    {
        mavlink_debug_t dbg = {};
        dbg.time_boot_ms = 10;
        dbg.ind = 3;
        dbg.value = 42.0f;
        mavlink_message_t msg;
        mavlink_msg_debug_encode(kSys, kComp, &msg, &dbg);
        f.inject(msg);
    }
    CHECK("254 DEBUG does not create capture status",
          !f.adapter.camera_capture_status(kSys, kComp).has_value());
}

static void test_gimbal_state_cache() {
    fprintf(stderr, "\n[Group] GIMBAL_* state cache\n");
    TestFixture f;
    const uint8_t kSys = 1, kComp = MAV_COMP_ID_GIMBAL;  // 154

    // GIMBAL_MANAGER_INFORMATION (280)
    {
        mavlink_gimbal_manager_information_t gi = {};
        gi.time_boot_ms = 1000;
        gi.cap_flags = 0x00000005;  // HAS_PITCH_AXIS | HAS_YAW_AXIS
        gi.pitch_min = -90.0f;
        gi.pitch_max = 30.0f;
        gi.yaw_min = -180.0f;
        gi.yaw_max = 180.0f;
        gi.gimbal_device_id = 0;
        mavlink_message_t msg;
        mavlink_msg_gimbal_manager_information_encode(kSys, kComp, &msg, &gi);
        f.inject(msg);
    }
    auto gimbals = f.adapter.latest_gimbals();
    CHECK("GIMBAL_MANAGER_INFORMATION cached", gimbals.size() == 1);
    if (!gimbals.empty()) {
        CHECK("gimbal cap_flags", gimbals[0].cap_flags == 0x5);
        CHECK("gimbal pitch range", gimbals[0].pitch_min == -90.0f && gimbals[0].pitch_max == 30.0f);
    }

    // GIMBAL_MANAGER_STATUS (281)
    {
        mavlink_gimbal_manager_status_t gs = {};
        gs.time_boot_ms = 1100;
        gs.flags = 0x2;
        gs.primary_control_sysid = 1;
        gs.primary_control_compid = 190;
        mavlink_message_t msg;
        mavlink_msg_gimbal_manager_status_encode(kSys, kComp, &msg, &gs);
        f.inject(msg);
    }
    auto gstatus = f.adapter.gimbal_manager_status(kSys, kComp);
    CHECK("GIMBAL_MANAGER_STATUS cached", gstatus.has_value());
    if (gstatus) {
        CHECK("gimbal manager flags", gstatus->flags == 0x2);
        CHECK("gimbal primary control", gstatus->primary_control_sysid == 1 && gstatus->primary_control_compid == 190);
    }

    // GIMBAL_DEVICE_INFORMATION (283)
    {
        mavlink_gimbal_device_information_t gi = {};
        gi.time_boot_ms = 1200;
        gi.cap_flags = 0x0001;
        strncpy(gi.vendor_name, "GMB", sizeof(gi.vendor_name) - 1);
        strncpy(gi.model_name, "G1", sizeof(gi.model_name) - 1);
        mavlink_message_t msg;
        mavlink_msg_gimbal_device_information_encode(kSys, kComp, &msg, &gi);
        f.inject(msg);
    }
    auto ginfo = f.adapter.gimbal_device_information(kSys, kComp);
    CHECK("GIMBAL_DEVICE_INFORMATION cached", ginfo.has_value());
    if (ginfo) {
        CHECK("gimbal device vendor", std::string(ginfo->vendor_name) == "GMB");
        CHECK("gimbal device model", std::string(ginfo->model_name) == "G1");
    }

    // GIMBAL_DEVICE_ATTITUDE_STATUS (285)，单位四元数 = 无旋转
    {
        mavlink_gimbal_device_attitude_status_t ga = {};
        ga.time_boot_ms = 1300;
        ga.q[0] = 1.0f; ga.q[1] = 0.0f; ga.q[2] = 0.0f; ga.q[3] = 0.0f;
        ga.flags = 0x00000004;
        ga.gimbal_device_id = 0;
        mavlink_message_t msg;
        mavlink_msg_gimbal_device_attitude_status_encode(kSys, kComp, &msg, &ga);
        f.inject(msg);
    }
    auto att = f.adapter.gimbal_device_attitude_status(kSys, kComp);
    CHECK("GIMBAL_DEVICE_ATTITUDE_STATUS cached", att.has_value());
    if (att) {
        CHECK("attitude quaternion identity", att->q[0] == 1.0f && att->q[1] == 0.0f);
        CHECK("attitude flags", att->flags == 0x4);
    }
}

static void test_video_stream_state() {
    fprintf(stderr, "\n[Group] VIDEO_STREAM_* state cache\n");
    TestFixture f;
    const uint8_t kSys = 1, kComp = MAV_COMP_ID_CAMERA;

    // VIDEO_STREAM_INFORMATION (269)
    {
        mavlink_video_stream_information_t vsi = {};
        vsi.stream_id = 0;
        vsi.type = 1;  // VIDEO_STREAM_TYPE_RTSP
        vsi.flags = 0x1;
        vsi.framerate = 30.0f;
        vsi.resolution_h = 1280;
        vsi.resolution_v = 720;
        vsi.bitrate = 4000000;
        strncpy(vsi.name, "front", sizeof(vsi.name) - 1);
        strncpy(vsi.uri, "rtsp://192.168.1.10:8554/main", sizeof(vsi.uri) - 1);
        mavlink_message_t msg;
        mavlink_msg_video_stream_information_encode(kSys, kComp, &msg, &vsi);
        f.inject(msg);
    }
    auto streams = f.adapter.latest_video_streams();
    CHECK("VIDEO_STREAM_INFORMATION cached", streams.size() == 1);
    if (!streams.empty()) {
        CHECK("stream uri", std::string(streams[0].uri) == "rtsp://192.168.1.10:8554/main");
        CHECK("stream resolution", streams[0].width == 1280 && streams[0].height == 720);
        CHECK("stream flags/running", (streams[0].flags & 0x1) != 0);
    }

    // VIDEO_STREAM_STATUS (270)
    {
        mavlink_video_stream_status_t vs = {};
        vs.stream_id = 0;
        vs.framerate = 30;
        vs.bitrate = 4000000;
        vs.resolution_h = 1280;
        vs.resolution_v = 720;
        mavlink_message_t msg;
        mavlink_msg_video_stream_status_encode(kSys, kComp, &msg, &vs);
        f.inject(msg);
    }
    auto statuses = f.adapter.latest_video_stream_status();
    CHECK("VIDEO_STREAM_STATUS cached", statuses.size() == 1);
    if (!statuses.empty()) {
        CHECK("stream status framerate", statuses[0].framerate == 30);
        CHECK("stream status bitrate", statuses[0].bitrate == 4000000);
    }
}

int main() {
    test_independent_link_parsers();
    fprintf(stderr, "=== MAVLink Protocol Layer Tests ===\n");
    fprintf(stderr, "(Inspired by QGC MockLink + MP MAVLinkInterface)\n\n");

    test_heartbeat_ardupilot_fw();
    test_heartbeat_px4_copter();
    test_param_value_decode();
    test_param_name_encoding();
    test_param_error_rejects_pending_write();
    test_command_ack();
    test_command_ack_timeout();
    test_gps_decode();
    test_attitude_decode();
    test_rc_channels_decode();
    test_servo_output_decode();
    test_statustext_decode();
    test_global_position_decode();
    test_home_position_decode();
    test_scaled_imu_instances();
    test_pressure_decode();
    test_sys_status_decode();
    test_battery_status_decode();
    test_vibration_decode();
    test_multi_message_interleave();
    test_heartbeat_alive();
    test_link_quality();
    test_send_heartbeat();
    test_send_callback_never_runs_under_the_adapter_lock();
    test_tx_sequence_is_isolated_per_adapter();
    test_signing_is_per_adapter();
    test_signing_candidate_auto_detect();
    test_signing_detection_rejects_replay_and_mixed_chunk();
    test_feed_raw_reports_valid_frames();
    test_camera_state_cache();
    test_camera_dispatcher_id_regression();
    test_gimbal_state_cache();
    test_video_stream_state();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return (g_failed > 0) ? 1 : 0;
}
