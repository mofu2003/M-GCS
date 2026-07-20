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
#include "common/mavlink.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

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
    mavlink_msg_param_value_pack(1, 1, &msg, "SERVO5_FUNCTION", 4.0f, MAV_PARAM_TYPE_UINT16, 0, 3);
    f.inject(msg);
    mavlink_msg_param_value_pack(1, 1, &msg, "ARMING_CHECK", 1.0f, MAV_PARAM_TYPE_INT32, 1, 3);
    f.inject(msg);
    mavlink_msg_param_value_pack(1, 1, &msg, "BATT_CAPACITY", 3300.0f, MAV_PARAM_TYPE_REAL32, 2, 3);
    f.inject(msg);
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

static void test_command_ack() {
    fprintf(stderr, "\n[Group] Command ACK - wait_for_ack\n");
    TestFixture f;
    bool ack_cb = false;
    f.adapter.on_command_ack([&](const mg::core::mavlink::CommandAckInfo&) { ack_cb = true; });
    f.adapter.clear_pending_ack();
    std::atomic<bool> got_ack{false};
    std::atomic<uint8_t> ack_result{255};
    std::thread waiter([&]() {
        auto result = f.adapter.wait_for_ack(MAV_CMD_COMPONENT_ARM_DISARM, 2000);
        if (result.has_value()) { got_ack = true; ack_result = result->result; }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mavlink_message_t msg;
    mavlink_msg_command_ack_pack(1, 1, &msg, MAV_CMD_COMPONENT_ARM_DISARM, MAV_RESULT_ACCEPTED, 0, 0, 0, 0);
    f.inject(msg);
    waiter.join();
    CHECK("ACK callback fired", ack_cb);
    CHECK("wait_for_ack returned", got_ack.load());
    CHECK("ACK result ACCEPTED", ack_result.load() == MAV_RESULT_ACCEPTED);
}

static void test_command_ack_timeout() {
    fprintf(stderr, "\n[Group] Command ACK - timeout\n");
    TestFixture f;
    f.adapter.clear_pending_ack();
    auto start = std::chrono::steady_clock::now();
    auto result = f.adapter.wait_for_ack(MAV_CMD_NAV_WAYPOINT, 200);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    CHECK("timeout returns nullopt", !result.has_value());
    CHECK("timeout ~200ms", elapsed >= 150 && elapsed < 500);
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
    bool received = false;
    f.adapter.on_servo_output([&](const mg::core::mavlink::ServoOutputInfo&) { received = true; });
    mavlink_message_t msg;
    { mavlink_servo_output_raw_t s = {}; s.servo1_raw=1500; s.servo2_raw=1500; s.servo3_raw=1100; s.servo4_raw=1500; s.servo5_raw=1800; s.servo6_raw=1200; s.servo7_raw=1500; s.servo8_raw=1500; mavlink_msg_servo_output_raw_encode(1,1,&msg,&s); }
    f.inject(msg);
    CHECK("Servo received", received);
    auto sv = f.adapter.latest_servo_output();
    CHECK("servo[0]=1500", sv.has_value() && sv->servo[0] == 1500);
    CHECK("servo[2]=1100", sv.has_value() && sv->servo[2] == 1100);
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

int main() {
    fprintf(stderr, "=== MAVLink Protocol Layer Tests ===\n");
    fprintf(stderr, "(Inspired by QGC MockLink + MP MAVLinkInterface)\n\n");

    test_heartbeat_ardupilot_fw();
    test_heartbeat_px4_copter();
    test_param_value_decode();
    test_param_name_encoding();
    test_command_ack();
    test_command_ack_timeout();
    test_gps_decode();
    test_attitude_decode();
    test_rc_channels_decode();
    test_servo_output_decode();
    test_statustext_decode();
    test_global_position_decode();
    test_home_position_decode();
    test_pressure_decode();
    test_sys_status_decode();
    test_battery_status_decode();
    test_vibration_decode();
    test_multi_message_interleave();
    test_heartbeat_alive();
    test_link_quality();
    test_send_heartbeat();

    fprintf(stderr, "\n=== Results: %d/%d passed ===\n", g_passed, g_passed + g_failed);
    return (g_failed > 0) ? 1 : 0;
}
