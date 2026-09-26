/**
 * json_serializers.h - JSON serialization for MAVLink telemetry types
 *
 * MAVLink 遥测数据结构到 JSON 的转换函数。
 *
 * Each function converts a core struct to a JSON string.
 * Used by ffi_api.cc to build event payloads and poll responses.
 *
 * Design rule: one function per type, pure functions, no side effects.
 */

#ifndef MGCORE_JSON_SERIALIZERS_H
#define MGCORE_JSON_SERIALIZERS_H

#include "json_builder.h"
#include <cmath>
#include "core/mavlink/mavlink_adapter.h"

namespace mg::bridge {

inline std::string heartbeat_json(const mg::core::mavlink::HeartbeatInfo& hb) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("system_id",      (int)hb.system_id);
    jb.field("vehicle_type",   (int)hb.vehicle_type);
    jb.field("autopilot_type", (int)hb.autopilot_type);
    jb.field("armed",          hb.armed);
    jb.field("base_mode",      (int)hb.base_mode);
    jb.field("custom_mode",    (int32_t)hb.custom_mode);
    jb.field("system_status",  (int)hb.system_status);
    jb.field("flight_mode",    std::string(hb.flight_mode));
    jb.end_object();
    return jb.str();
}

inline std::string sysstatus_json(const mg::core::mavlink::SysStatusInfo& ss) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("voltage_battery",   (int)ss.battery_voltage);
    jb.field("current_battery",   (int)ss.battery_current);
    jb.field("battery_remaining", (int)ss.battery_remaining);
    jb.end_object();
    return jb.str();
}

inline std::string gps_json(const mg::core::mavlink::GpsInfo& g) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("lat",        g.lat);
    jb.field("lon",        g.lon);
    jb.field("alt",        g.alt);
    jb.field("fix_type",   (int)g.fix_type);
    jb.field("satellites", (int)g.satellites_visible);
    jb.end_object();
    return jb.str();
}

inline std::string attitude_json(const mg::core::mavlink::AttitudeInfo& a) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("roll",       a.roll);
    jb.field("pitch",      a.pitch);
    jb.field("yaw",        a.yaw);
    jb.field("rollspeed",  a.rollspeed);
    jb.field("pitchspeed", a.pitchspeed);
    jb.field("yawspeed",   a.yawspeed);
    jb.end_object();
    return jb.str();
}

inline std::string vfr_hud_json(const mg::core::mavlink::VfrHudInfo& v) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("airspeed",    v.airspeed);
    jb.field("groundspeed", v.groundspeed);
    jb.field("alt",         v.alt);
    jb.field("climb",       v.climb);
    jb.field("heading",     (int)v.heading);
    jb.field("throttle",    (int)v.throttle);
    jb.end_object();
    return jb.str();
}

inline std::string imu_json(const mg::core::mavlink::ImuInfo& imu) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("instance", (int)imu.instance);
    jb.field("xacc",  (int)imu.xacc);
    jb.field("yacc",  (int)imu.yacc);
    jb.field("zacc",  (int)imu.zacc);
    jb.field("xgyro", (int)imu.xgyro);
    jb.field("ygyro", (int)imu.ygyro);
    jb.field("zgyro", (int)imu.zgyro);
    jb.field("xmag",  (int)imu.xmag);
    jb.field("ymag",  (int)imu.ymag);
    jb.field("zmag",  (int)imu.zmag);
    jb.field("temperature", (int)imu.temperature);
    jb.end_object();
    return jb.str();
}

inline std::string pressure_json(const mg::core::mavlink::PressureInfo& p) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("press_abs",   p.press_abs);
    jb.field("press_diff",  p.press_diff);
    jb.field("temperature", (int)p.temperature);
    jb.end_object();
    return jb.str();
}

inline std::string firmware_json(const mg::core::mavlink::HeartbeatInfo& hb) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("autopilot_type", (int)hb.autopilot_type);
    jb.field("vehicle_type",   (int)hb.vehicle_type);
    jb.field("system_id",      (int)hb.system_id);
    jb.field("flight_mode",    std::string(hb.flight_mode));
    jb.end_object();
    return jb.str();
}

inline std::string firmware_version_json(const mg::core::mavlink::FirmwareVersionInfo& fw) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("received",       fw.received);
    jb.field("major",          (int)fw.firmware_major);
    jb.field("minor",          (int)fw.firmware_minor);
    jb.field("patch",          (int)fw.firmware_patch);
    jb.field("type",           (int)fw.firmware_type);
    jb.field("vendor_id",      (int)fw.vendor_id);
    jb.field("product_id",     (int)fw.product_id);
    jb.field("uid",            (int64_t)fw.uid);
    jb.field("capabilities",   (int64_t)fw.capabilities);
    jb.field("git_hash",       fw.git_hash);
    jb.field("system_id",      (int)fw.system_id);
    jb.field("component_id",   (int)fw.component_id);
    jb.end_object();
    return jb.str();
}

inline std::string command_ack_json(const mg::core::mavlink::CommandAckInfo& ack) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("command",  (int)ack.command);
    jb.field("result",   (int)ack.result);
    jb.field("progress", (int)ack.progress);
    // Map result code to string
    const char* result_str = "unknown";
    switch (ack.result) {
        case 0: result_str = "accepted"; break;
        case 1: result_str = "temporarily_rejected"; break;
        case 2: result_str = "denied"; break;
        case 3: result_str = "unsupported"; break;
        case 4: result_str = "failed"; break;
        case 5: result_str = "in_progress"; break;
    }
    jb.field("result_str", result_str);
    jb.end_object();
    return jb.str();
}



inline std::string rc_channels_json(
        const mg::core::mavlink::RcChannelsInfo& rc,
        bool valid) {
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("chancount", (int)rc.chancount);
    jb.field("rssi",      (int)rc.rssi);
    jb.field("valid",     valid);
    mg::bridge::JsonBuilder arr;
    arr.begin_array();
    for (int i = 0; i < 18; i++) arr.append_json(std::to_string((int)rc.chan[i]));
    arr.end_array();
    jb.raw_field("channels", arr.str());
    jb.end_object();
    return jb.str();
}

inline std::string rc_channels_json(const mg::core::mavlink::RcChannelsInfo& rc) {
    return rc_channels_json(rc, rc.chancount > 0);
}

inline std::string servo_output_json(
        const mg::core::mavlink::ServoOutputInfo& srv,
        bool valid) {
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("port", (int)srv.port);
    jb.field("time_usec", static_cast<int64_t>(srv.time_usec));
    jb.field("valid", valid);
    mg::bridge::JsonBuilder arr;
    arr.begin_array();
    for (int i = 0; i < 16; i++) arr.append_json(std::to_string((int)srv.servo[i]));
    arr.end_array();
    jb.raw_field("servos", arr.str());
    jb.end_object();
    return jb.str();
}

inline std::string servo_output_json(const mg::core::mavlink::ServoOutputInfo& srv) {
    bool valid = false;
    for (const uint16_t value : srv.servo) {
        valid = valid || value > 0;
    }
    return servo_output_json(srv, valid);
}


inline std::string statustext_json(const mg::core::mavlink::StatustextInfo& st) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("severity", (int)st.severity);
    jb.field("text", std::string(st.text));
    // Severity label for convenience
    const char* label = "unknown";
    switch (st.severity) {
        case 0: label = "emergency"; break;
        case 1: label = "alert"; break;
        case 2: label = "critical"; break;
        case 3: label = "error"; break;
        case 4: label = "warning"; break;
        case 5: label = "notice"; break;
        case 6: label = "info"; break;
        case 7: label = "debug"; break;
    }
    jb.field("severity_label", label);
    jb.end_object();
    return jb.str();
}

inline std::string global_position_json(const mg::core::mavlink::GlobalPositionInfo& gp) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("lat",          gp.lat);
    jb.field("lon",          gp.lon);
    jb.field("alt",          gp.alt);
    jb.field("relative_alt", gp.relative_alt);
    jb.field("vx",           (int)gp.vx);
    jb.field("vy",           (int)gp.vy);
    jb.field("vz",           (int)gp.vz);
    jb.field("hdg",          (int)gp.hdg);
    jb.end_object();
    return jb.str();
}

inline std::string home_position_json(const mg::core::mavlink::HomePositionInfo& hp) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("latitude",  hp.latitude);
    jb.field("longitude", hp.longitude);
    jb.field("altitude",  hp.altitude);
    jb.field("x", hp.x);
    jb.field("y", hp.y);
    jb.field("z", hp.z);
    jb.field("approach_x", hp.approach_x);
    jb.field("approach_y", hp.approach_y);
    jb.field("approach_z", hp.approach_z);
    jb.end_object();
    return jb.str();
}

inline std::string battery_json(const mg::core::mavlink::BatteryInfo& b) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("voltage_v",        b.voltage_v);
    jb.field("current_a",        b.current_a);
    jb.field("battery_remaining", (int)b.battery_remaining);
    jb.field("consumed_mah",     b.consumed_mah);
    jb.field("time_remaining_s", b.time_remaining_s);
    jb.field("temperature_c",    (int)b.temperature_c);
    jb.field("function",         (int)b.function);
    jb.field("type",             (int)b.type);
    jb.field("charge_state",     (int)b.charge_state);
    jb.end_object();
    return jb.str();
}

inline std::string highres_imu_json(const mg::core::mavlink::HighresImuInfo& imu) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("instance",     (int)imu.instance);
    jb.field("xacc",         imu.xacc);
    jb.field("yacc",         imu.yacc);
    jb.field("zacc",         imu.zacc);
    jb.field("xgyro",        imu.xgyro);
    jb.field("ygyro",        imu.ygyro);
    jb.field("zgyro",        imu.zgyro);
    jb.field("xmag",         imu.xmag);
    jb.field("ymag",         imu.ymag);
    jb.field("zmag",         imu.zmag);
    jb.field("abs_pressure", imu.abs_pressure);
    jb.field("diff_pressure", imu.diff_pressure);
    jb.field("pressure_alt", imu.pressure_alt);
    jb.field("temperature",  imu.temperature);
    jb.field("fields_updated", (int64_t)imu.fields_updated);
    jb.end_object();
    return jb.str();
}

inline std::string extended_sys_state_json(const mg::core::mavlink::ExtendedSysStateInfo& es) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("vtol_state",   (int)es.vtol_state);
    jb.field("landed_state", (int)es.landed_state);
    jb.end_object();
    return jb.str();
}

inline std::string prearm_check_json(const mg::core::mavlink::PrearmCheckInfo& pc) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("system_id", (int)pc.system_id);
    jb.field("check_mask", (int64_t)pc.check_mask);
    jb.field("baro", (bool)(pc.check_mask & (1 << 0)));
    jb.field("compass", (bool)(pc.check_mask & (1 << 1)));
    jb.field("gps", (bool)(pc.check_mask & (1 << 2)));
    jb.field("ins", (bool)(pc.check_mask & (1 << 3)));
    jb.field("rc", (bool)(pc.check_mask & (1 << 4)));
    jb.field("voltage", (bool)(pc.check_mask & (1 << 5)));
    jb.end_object();
    return jb.str();
}

inline std::string vibration_json(const mg::core::mavlink::VibrationInfo& vib) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("system_id", (int)vib.system_id);
    jb.field("vibration_x", vib.vibration_x);
    jb.field("vibration_y", vib.vibration_y);
    jb.field("vibration_z", vib.vibration_z);
    float mag = std::sqrt(vib.vibration_x * vib.vibration_x +
                          vib.vibration_y * vib.vibration_y +
                          vib.vibration_z * vib.vibration_z);
    jb.field("magnitude", mag);
    jb.field("healthy", mag < 30.0f);
    jb.field("warning", mag >= 30.0f && mag < 60.0f);
    jb.field("critical", mag >= 60.0f);
    jb.field("clipping_0", (int64_t)vib.clipping_0);
    jb.field("clipping_1", (int64_t)vib.clipping_1);
    jb.field("clipping_2", (int64_t)vib.clipping_2);
    jb.end_object();
    return jb.str();
}

inline std::string esc_status_json(const mg::core::mavlink::EscStatusInfo& esc) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("system_id", (int)esc.system_id);
    jb.field("index", (int)esc.index);
    jb.field("count", (int)esc.count);
    jb.field("error_count", (int64_t)esc.error_count);
    jb.field("rpm", esc.rpm);
    jb.field("voltage", esc.voltage);
    jb.field("current", esc.current);
    jb.field("temperature", esc.temperature);
    jb.end_object();
    return jb.str();
}

inline std::string wind_estimate_json(const mg::core::mavlink::WindEstimateInfo& wind) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("system_id", (int)wind.system_id);
    jb.field("wind_x", wind.wind_x);
    jb.field("wind_y", wind.wind_y);
    jb.field("wind_z", wind.wind_z);
    float speed = std::sqrt(wind.wind_x * wind.wind_x + wind.wind_y * wind.wind_y);
    jb.field("speed", speed);
    jb.field("var_horiz", wind.var_horiz);
    jb.field("var_vert", wind.var_vert);
    jb.end_object();
    return jb.str();
}
} // namespace mg::bridge




#endif // MGCORE_JSON_SERIALIZERS_H
