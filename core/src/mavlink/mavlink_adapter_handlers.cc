/**
 * mavlink_adapter_handlers.cc - MavlinkAdapter message handlers
 *
 * Definitions are out of line so the internal header only declares the
 * handler surface. Dispatch, locking, timeout checks, and callback
 * execution remain in MavlinkAdapter::Impl::process().
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

namespace {

const char* param_error_text(uint8_t error) {
    switch (error) {
    case MAV_PARAM_ERROR_DOES_NOT_EXIST:
        return "parameter does not exist";
    case MAV_PARAM_ERROR_VALUE_OUT_OF_RANGE:
        return "parameter value is out of range";
    case MAV_PARAM_ERROR_PERMISSION_DENIED:
        return "parameter write permission denied";
    case MAV_PARAM_ERROR_COMPONENT_NOT_FOUND:
        return "parameter component not found";
    case MAV_PARAM_ERROR_READ_ONLY:
        return "parameter is read-only";
    case MAV_PARAM_ERROR_TYPE_UNSUPPORTED:
        return "parameter type is unsupported";
    case MAV_PARAM_ERROR_TYPE_MISMATCH:
        return "parameter type mismatch";
    case MAV_PARAM_ERROR_READ_FAIL:
        return "parameter read failed";
    default:
        return "parameter write rejected";
    }
}

} // namespace

void MavlinkAdapter::Impl::handle_heartbeat(const mavlink_message_t& m)
{
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&m, &hb);
    HeartbeatInfo info{};
    info.system_id = m.sysid;
    info.component_id = m.compid;
    info.autopilot_type = hb.autopilot;
    info.vehicle_type = hb.type;
    info.base_mode = hb.base_mode;
    info.custom_mode = hb.custom_mode;
    info.system_status = hb.system_status;
    info.armed = (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
    pc.fw_adapter.detect(info.autopilot_type, info.vehicle_type);
    auto mode_info = pc.fw_adapter.decode_flight_mode(info.custom_mode);
    if (mode_info.valid) {
        strncpy(info.flight_mode, mode_info.mode_name.c_str(), sizeof(info.flight_mode) - 1);
    }
    tel.heartbeat = info;
    tel.last_heartbeat_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
    tel.heartbeat_lost = false;
    if (tel.heartbeat_cb)
        deferred_cbs.push_back([cb = tel.heartbeat_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_sys_status(const mavlink_message_t& m)
{
    mavlink_sys_status_t ss;
    mavlink_msg_sys_status_decode(&m, &ss);
    SysStatusInfo info{};
    info.system_id = m.sysid;
    info.onboard_control_sensors_present = ss.onboard_control_sensors_present;
    info.onboard_control_sensors_enabled = ss.onboard_control_sensors_enabled;
    info.onboard_control_sensors_health = ss.onboard_control_sensors_health;
    info.battery_voltage = ss.voltage_battery;
    info.battery_current = ss.current_battery;
    info.battery_remaining = ss.battery_remaining;
    tel.sys_status = info;
    if (tel.sys_status_cb)
        deferred_cbs.push_back([cb = tel.sys_status_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_gps(const mavlink_message_t& m)
{
    mavlink_gps_raw_int_t gps;
    mavlink_msg_gps_raw_int_decode(&m, &gps);
    GpsInfo info{};
    info.system_id = m.sysid;
    info.lat = gps.lat;
    info.lon = gps.lon;
    info.alt = gps.alt;
    info.fix_type = gps.fix_type;
    info.satellites_visible = gps.satellites_visible;
    tel.gps = info;
    if (tel.gps_cb)
        deferred_cbs.push_back([cb = tel.gps_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_scaled_imu(const mavlink_message_t& m, uint8_t instance)
{
    mavlink_scaled_imu_t imu;
    mavlink_msg_scaled_imu_decode(&m, &imu);
    ImuInfo info{};
    info.system_id = m.sysid;
    info.instance = instance;
    info.xacc = imu.xacc;
    info.yacc = imu.yacc;
    info.zacc = imu.zacc;
    info.xgyro = imu.xgyro;
    info.ygyro = imu.ygyro;
    info.zgyro = imu.zgyro;
    info.xmag = imu.xmag;
    info.ymag = imu.ymag;
    info.zmag = imu.zmag;
    info.temperature = imu.temperature;
    // latest_imu() 语义是"主 IMU"，只让 IMU1 覆盖缓存；
    // 其余实例只通过回调下发，由调用方按 instance 分派。
    if (instance == 0)
        tel.imu = info;
    if (tel.imu_cb)
        deferred_cbs.push_back([cb = tel.imu_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_scaled_pressure(const mavlink_message_t& m)
{
    mavlink_scaled_pressure_t sp;
    mavlink_msg_scaled_pressure_decode(&m, &sp);
    PressureInfo info{};
    info.system_id = m.sysid;
    info.press_abs = sp.press_abs;
    info.press_diff = sp.press_diff;
    info.temperature = sp.temperature;
    tel.pressure = info;
    if (tel.pressure_cb)
        deferred_cbs.push_back([cb = tel.pressure_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_attitude(const mavlink_message_t& m)
{
    mavlink_attitude_t att;
    mavlink_msg_attitude_decode(&m, &att);
    AttitudeInfo info{};
    info.system_id = m.sysid;
    info.roll = att.roll;
    info.pitch = att.pitch;
    info.yaw = att.yaw;
    info.rollspeed = att.rollspeed;
    info.pitchspeed = att.pitchspeed;
    info.yawspeed = att.yawspeed;
    tel.attitude = info;
    if (tel.attitude_cb)
        deferred_cbs.push_back([cb = tel.attitude_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_vfr_hud(const mavlink_message_t& m)
{
    mavlink_vfr_hud_t vfr;
    mavlink_msg_vfr_hud_decode(&m, &vfr);
    VfrHudInfo info{};
    info.system_id = m.sysid;
    info.airspeed = vfr.airspeed;
    info.groundspeed = vfr.groundspeed;
    info.alt = vfr.alt;
    info.climb = vfr.climb;
    info.heading = vfr.heading;
    info.throttle = vfr.throttle;
    tel.vfr_hud = info;
    if (tel.vfr_hud_cb)
        deferred_cbs.push_back([cb = tel.vfr_hud_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_autopilot_version(const mavlink_message_t& m)
{
    mavlink_autopilot_version_t ap;
    mavlink_msg_autopilot_version_decode(&m, &ap);
    FirmwareVersionInfo info{};
    info.system_id = m.sysid;
    info.component_id = m.compid;
    info.firmware_major = (ap.flight_sw_version >> 24) & 0xFF;
    info.firmware_minor = (ap.flight_sw_version >> 16) & 0xFF;
    info.firmware_patch = (ap.flight_sw_version >> 8) & 0xFF;
    info.firmware_type = ap.flight_sw_version & 0xFF;
    info.capabilities = ap.capabilities;
    info.vendor_id = ap.vendor_id;
    info.product_id = ap.product_id;
    info.uid = ap.uid;
    memcpy(info.git_hash, ap.flight_custom_version, 8);
    info.git_hash[8] = '\0';
    info.received = true;
    tel.firmware_version = info;
    if (tel.firmware_version_cb)
        deferred_cbs.push_back([cb = tel.firmware_version_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_param_value(const mavlink_message_t& m)
{
    mavlink_param_value_t pv;
    mavlink_msg_param_value_decode(&m, &pv);
    ParamInfo info;
    info.name = std::string(pv.param_id, strnlen(pv.param_id, 16));
    info.value = pv.param_value;
    info.type = pv.param_type;
    info.index = pv.param_index;
    info.count = pv.param_count;

    const auto result = pc.handle_param_value(info, m.sysid, m.compid, deferred_cbs);
    if (result.cache_updated && tel.param_cb)
        deferred_cbs.push_back([cb = tel.param_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_param_error(const mavlink_message_t& m)
{
    mavlink_param_error_t error{};
    mavlink_msg_param_error_decode(&m, &error);
    const std::string name(error.param_id, strnlen(error.param_id, sizeof(error.param_id)));
    pc.fail_refresh(name, m.sysid, m.compid);
    const auto pending = pc.pending_writes.find(name);
    if (pending == pc.pending_writes.end()) {
        return;
    }
    if ((pending->second.target_sys != 0 && pending->second.target_sys != m.sysid) ||
        (pending->second.target_comp != 0 && pending->second.target_comp != m.compid)) {
        return;
    }

    pc.reject_write(
        name,
        std::string("parameter write rejected: ") + param_error_text(error.error),
        deferred_cbs);
}

void MavlinkAdapter::Impl::handle_command_ack(const mavlink_message_t& m)
{
    mavlink_command_ack_t ack;
    mavlink_msg_command_ack_decode(&m, &ack);
    CommandAckInfo info{};
    info.result = ack.result;
    info.command = ack.command;
    info.progress = ack.progress;
    tel.command_ack = info;

    // 路由到命令队列
    uint64_t matched_id = cmd_queue.handle_ack(m.compid, ack.command, ack.result, ack.progress);
    if (matched_id != 0) {
        int idx = cmd_queue.find_by_id(matched_id);
        if (idx >= 0) {
            auto& entry = cmd_queue.entries[idx];
            if (entry.result_cb) {
                deferred_cbs.push_back([cb = entry.result_cb, info]() { cb(info); });
            }
        }
    }

    // 旧 waiter 路径（向后兼容 wait_for_ack）
    ack_waiter.pending = info;
    ack_waiter.ack_received = true;
    ack_waiter.cv.notify_one();
    if (tel.command_ack_cb)
        deferred_cbs.push_back([cb = tel.command_ack_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_command_long(const mavlink_message_t& m)
{
    mavlink_command_long_t command;
    mavlink_msg_command_long_decode(&m, &command);
    CommandLongInfo info{};
    info.system_id = m.sysid;
    info.component_id = m.compid;
    info.target_system = command.target_system;
    info.target_component = command.target_component;
    info.command = command.command;
    info.param1 = command.param1;
    if (tel.command_long_cb) {
        deferred_cbs.push_back([cb = tel.command_long_cb, info]() { cb(info); });
    }
}

void MavlinkAdapter::Impl::handle_mag_cal_progress(const mavlink_message_t& m)
{
    // Decode the fixed leading bytes directly so calibration progress
    // remains usable from the compact common wire representation.
    // The compact fields below occupy payload bytes 12..16.  Reject
    // truncated frames before touching them; malformed MAVLink frames can
    // otherwise turn a protocol error into an out-of-bounds read.
    constexpr uint8_t kCompactProgressMinLen = 17;
    if (m.len < kCompactProgressMinLen)
        return;
    const auto* payload = reinterpret_cast<const uint8_t*>(m.payload64);
    MagCalibrationProgressInfo info{};
    info.system_id = m.sysid;
    // MAVLink packets use wire ordering: direction vectors occupy bytes
    // 0..11, then the compact state fields begin at byte 12.
    info.compass_id = payload[12];
    info.cal_mask = payload[13];
    info.cal_status = payload[14];
    info.attempt = payload[15];
    info.completion_pct = payload[16];
    if (tel.mag_cal_progress_cb) {
        deferred_cbs.push_back([cb = tel.mag_cal_progress_cb, info]() { cb(info); });
    }
}

void MavlinkAdapter::Impl::handle_mag_cal_report(const mavlink_message_t& m)
{
    mavlink_mag_cal_report_t report{};
    mavlink_msg_mag_cal_report_decode(&m, &report);
    MagCalibrationReportInfo info{};
    info.system_id = m.sysid;
    info.compass_id = report.compass_id;
    info.cal_mask = report.cal_mask;
    info.cal_status = report.cal_status;
    info.autosaved = report.autosaved != 0;
    info.fitness = report.fitness;
    info.offset_x = report.ofs_x;
    info.offset_y = report.ofs_y;
    info.offset_z = report.ofs_z;
    info.diagonal_x = report.diag_x;
    info.diagonal_y = report.diag_y;
    info.diagonal_z = report.diag_z;
    info.offdiag_x = report.offdiag_x;
    info.offdiag_y = report.offdiag_y;
    info.offdiag_z = report.offdiag_z;
    if (tel.mag_cal_report_cb) {
        deferred_cbs.push_back([cb = tel.mag_cal_report_cb, info]() { cb(info); });
    }
}

void MavlinkAdapter::Impl::handle_rc_channels(const mavlink_message_t& m)
{
    mavlink_rc_channels_t rc;
    mavlink_msg_rc_channels_decode(&m, &rc);
    RcChannelsInfo info{};
    info.system_id = m.sysid;
    info.chancount = rc.chancount;
    info.rssi = rc.rssi;
    info.chan[0] = rc.chan1_raw;
    info.chan[1] = rc.chan2_raw;
    info.chan[2] = rc.chan3_raw;
    info.chan[3] = rc.chan4_raw;
    info.chan[4] = rc.chan5_raw;
    info.chan[5] = rc.chan6_raw;
    info.chan[6] = rc.chan7_raw;
    info.chan[7] = rc.chan8_raw;
    info.chan[8] = rc.chan9_raw;
    info.chan[9] = rc.chan10_raw;
    info.chan[10] = rc.chan11_raw;
    info.chan[11] = rc.chan12_raw;
    info.chan[12] = rc.chan13_raw;
    info.chan[13] = rc.chan14_raw;
    info.chan[14] = rc.chan15_raw;
    info.chan[15] = rc.chan16_raw;
    info.chan[16] = rc.chan17_raw;
    info.chan[17] = rc.chan18_raw;
    tel.rc_channels = info;
    if (tel.rc_channels_cb)
        deferred_cbs.push_back([cb = tel.rc_channels_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_servo_output_raw(const mavlink_message_t& m)
{
    mavlink_servo_output_raw_t so;
    mavlink_msg_servo_output_raw_decode(&m, &so);
    // The 1-16 page is backed by port 0.  ArduPilot uses port 1 for
    // channels 17-32; never let that second packet overwrite channels 1-16.
    if (so.port != 0)
        return;
    ServoOutputInfo info{};
    info.system_id = m.sysid;
    info.time_usec = so.time_usec;
    info.port = so.port;
    info.servo[0] = so.servo1_raw;
    info.servo[1] = so.servo2_raw;
    info.servo[2] = so.servo3_raw;
    info.servo[3] = so.servo4_raw;
    info.servo[4] = so.servo5_raw;
    info.servo[5] = so.servo6_raw;
    info.servo[6] = so.servo7_raw;
    info.servo[7] = so.servo8_raw;
    info.servo[8] = so.servo9_raw;
    info.servo[9] = so.servo10_raw;
    info.servo[10] = so.servo11_raw;
    info.servo[11] = so.servo12_raw;
    info.servo[12] = so.servo13_raw;
    info.servo[13] = so.servo14_raw;
    info.servo[14] = so.servo15_raw;
    info.servo[15] = so.servo16_raw;
    tel.servo_output = info;
    if (tel.servo_output_cb)
        deferred_cbs.push_back([cb = tel.servo_output_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_statustext(const mavlink_message_t& m)
{
    mavlink_statustext_t st;
    mavlink_msg_statustext_decode(&m, &st);
    tel.last_statustext = std::string(st.text, strnlen(st.text, sizeof(st.text)));
    tel.last_statustext_severity = st.severity;
    if (tel.statustext_cb)
        deferred_cbs.push_back([cb = tel.statustext_cb, text = tel.last_statustext,
                                severity = tel.last_statustext_severity]() { cb(text, severity); });
}

void MavlinkAdapter::Impl::handle_global_position(const mavlink_message_t& m)
{
    mavlink_global_position_int_t gp;
    mavlink_msg_global_position_int_decode(&m, &gp);
    GlobalPositionInfo info{};
    info.system_id = m.sysid;
    info.lat = gp.lat;
    info.lon = gp.lon;
    info.alt = gp.alt;
    info.relative_alt = gp.relative_alt;
    info.vx = gp.vx;
    info.vy = gp.vy;
    info.vz = gp.vz;
    info.hdg = gp.hdg;
    tel.global_position = info;
    if (tel.global_position_cb)
        deferred_cbs.push_back([cb = tel.global_position_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_home_position(const mavlink_message_t& m)
{
    mavlink_home_position_t hp;
    mavlink_msg_home_position_decode(&m, &hp);
    HomePositionInfo info{};
    info.system_id = m.sysid;
    info.latitude = hp.latitude;
    info.longitude = hp.longitude;
    info.altitude = hp.altitude;
    info.x = hp.x;
    info.y = hp.y;
    info.z = hp.z;
    info.approach_x = hp.approach_x;
    info.approach_y = hp.approach_y;
    info.approach_z = hp.approach_z;
    tel.home_position = info;
    if (tel.home_position_cb)
        deferred_cbs.push_back([cb = tel.home_position_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_battery_status(const mavlink_message_t& m)
{
    mavlink_battery_status_t bs;
    mavlink_msg_battery_status_decode(&m, &bs);
    BatteryInfo info{};
    info.system_id = m.sysid;
    info.voltage_v = 0.0f;
    for (int vi = 0; vi < 10 && bs.voltages[vi] != UINT16_MAX; vi++)
        info.voltage_v += bs.voltages[vi] / 1000.0f;
    info.current_a = (bs.current_battery != -1) ? bs.current_battery / 100.0f : 0.0f;
    info.battery_remaining = bs.battery_remaining;
    info.consumed_mah = bs.current_consumed;
    tel.battery = info;
    if (tel.battery_cb)
        deferred_cbs.push_back([cb = tel.battery_cb, info]() { cb(info); });
    // Battery failsafe event: fire when battery is critically low
    if (info.battery_remaining >= 0 && info.battery_remaining <= 10) {
        if (tel.battery_cb) {
            // The MG_EVENT_BATTERY_FAILSAFE will be handled in the FFI layer
            // by checking battery_remaining in the telemetry cache
        }
    }
}

void MavlinkAdapter::Impl::handle_highres_imu(const mavlink_message_t& m)
{
    mavlink_highres_imu_t hi;
    mavlink_msg_highres_imu_decode(&m, &hi);
    HighresImuInfo info{};
    info.system_id = m.sysid;
    info.instance = hi.id;
    info.xacc = hi.xacc;
    info.yacc = hi.yacc;
    info.zacc = hi.zacc;
    info.xgyro = hi.xgyro;
    info.ygyro = hi.ygyro;
    info.zgyro = hi.zgyro;
    info.xmag = hi.xmag;
    info.ymag = hi.ymag;
    info.zmag = hi.zmag;
    info.abs_pressure = hi.abs_pressure / 100.0f;   // Pa -> hPa
    info.diff_pressure = hi.diff_pressure / 100.0f; // Pa -> hPa
    info.pressure_alt = hi.pressure_alt;
    info.temperature = hi.temperature;
    info.fields_updated = hi.fields_updated;
    // 同 SCALED_IMU：latest_highres_imu() 只代表主 IMU。
    if (info.instance == 0)
        tel.highres_imu = info;
    if (tel.highres_imu_cb)
        deferred_cbs.push_back([cb = tel.highres_imu_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_extended_sys_state(const mavlink_message_t& m)
{
    mavlink_extended_sys_state_t es;
    mavlink_msg_extended_sys_state_decode(&m, &es);
    ExtendedSysStateInfo info{};
    info.system_id = m.sysid;
    info.vtol_state = es.vtol_state;
    info.landed_state = es.landed_state;
    tel.extended_sys_state = info;
    if (tel.extended_sys_state_cb)
        deferred_cbs.push_back([cb = tel.extended_sys_state_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_vibration(const mavlink_message_t& m)
{
    mavlink_vibration_t vib;
    mavlink_msg_vibration_decode(&m, &vib);
    VibrationInfo info{};
    info.system_id = m.sysid;
    info.vibration_x = vib.vibration_x;
    info.vibration_y = vib.vibration_y;
    info.vibration_z = vib.vibration_z;
    info.clipping_0 = vib.clipping_0;
    info.clipping_1 = vib.clipping_1;
    info.clipping_2 = vib.clipping_2;
    tel.vibration = info;
    if (tel.vibration_cb)
        deferred_cbs.push_back([cb = tel.vibration_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_esc_status(const mavlink_message_t& m)
{
    mavlink_esc_status_t esc;
    mavlink_msg_esc_status_decode(&m, &esc);
    EscStatusInfo info{};
    info.system_id = m.sysid;
    info.index = esc.index;
    info.count = 4; // ESC_STATUS always covers 4 ESCs per message
    info.error_count = 0;
    // Average RPM/voltage/current across the 4 ESC channels
    info.rpm = 0.0f;
    info.voltage = 0.0f;
    info.current = 0.0f;
    for (int i = 0; i < 4; i++) {
        info.rpm += static_cast<float>(esc.rpm[i]);
        info.voltage += esc.voltage[i];
        info.current += esc.current[i];
    }
    info.rpm *= 0.25f;
    info.voltage *= 0.25f;
    info.current *= 0.25f;
    info.temperature = 0;
    tel.esc_status = info;
    if (tel.esc_status_cb)
        deferred_cbs.push_back([cb = tel.esc_status_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_distance_sensor(const mavlink_message_t& m)
{
    mavlink_distance_sensor_t ds;
    mavlink_msg_distance_sensor_decode(&m, &ds);
    DistanceSensorInfo info{};
    info.system_id = m.sysid;
    info.id = ds.id;
    info.orientation = ds.orientation;
    info.current_distance = ds.current_distance;
    info.min_distance = ds.min_distance;
    info.max_distance = ds.max_distance;
    info.type = ds.type;
    info.covariance = ds.covariance;
    if (tel.distance_sensor_cb)
        deferred_cbs.push_back([cb = tel.distance_sensor_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_adsb_vehicle(const mavlink_message_t& m)
{
    mavlink_adsb_vehicle_t av;
    mavlink_msg_adsb_vehicle_decode(&m, &av);
    AdsbVehicleInfo info{};
    info.icao_address = av.ICAO_address;
    info.lat = av.lat;
    info.lon = av.lon;
    info.altitude = av.altitude;
    info.heading = av.heading;
    info.hor_velocity = av.hor_velocity;
    info.ver_velocity = av.ver_velocity;
    memcpy(info.callsign, av.callsign, sizeof(info.callsign));
    info.emitter_type = av.emitter_type;
    info.tslc = av.tslc;
    info.flags = av.flags;
    info.squawk = av.squawk;
    if (tel.adsb_vehicle_cb)
        deferred_cbs.push_back([cb = tel.adsb_vehicle_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_video_stream_information(const mavlink_message_t& m)
{
    mavlink_video_stream_information_t vsi;
    mavlink_msg_video_stream_information_decode(&m, &vsi);
    VideoStreamInfo info{};
    info.stream_id = vsi.stream_id;
    info.type = vsi.type;
    info.width = vsi.resolution_h;
    info.height = vsi.resolution_v;
    info.framerate = vsi.framerate;
    info.bitrate = vsi.bitrate;
    info.rotation = vsi.rotation;
    info.hfov = vsi.hfov;
    info.flags = vsi.flags;
    memcpy(info.name, vsi.name, sizeof(info.name));
    memcpy(info.uri, vsi.uri, sizeof(info.uri));

    // 更新缓存：按 stream_id 查找或新建
    bool found = false;
    for (auto& s : tel.video_streams) {
        if (s.stream_id == info.stream_id) {
            s = info;
            found = true;
            break;
        }
    }
    if (!found)
        tel.video_streams.push_back(info);

    if (tel.video_stream_cb)
        deferred_cbs.push_back([cb = tel.video_stream_cb, info]() { cb(info); });
}

void MavlinkAdapter::Impl::handle_camera_information(const mavlink_message_t& m)
{
    mavlink_camera_information_t ci;
    mavlink_msg_camera_information_decode(&m, &ci);
    // CameraManager 在 Vehicle 层，此处只记录日志
    // 实际解析由 wire_post_connect_callbacks 中的相机回调处理
    MGC_LOG_INFO("[MavlinkAdapter] CAMERA_INFORMATION: model=%s vendor=%s", ci.model_name, ci.vendor_name);
}

void MavlinkAdapter::Impl::handle_camera_capture_status(const mavlink_message_t& m)
{
    mavlink_camera_capture_status_t cs;
    mavlink_msg_camera_capture_status_decode(&m, &cs);
    MGC_LOG_INFO("[MavlinkAdapter] CAMERA_CAPTURE_STATUS: image_status=%d video_status=%d count=%d", cs.image_status,
                 cs.video_status, cs.image_count);
}

void MavlinkAdapter::Impl::handle_gimbal_manager_info(const mavlink_message_t& m)
{
    MGC_LOG_INFO("[MavlinkAdapter] GIMBAL_MANAGER_INFORMATION from sys=%d comp=%d", m.sysid, m.compid);
}

void MavlinkAdapter::Impl::handle_gimbal_manager_status(const mavlink_message_t& m)
{
    MGC_LOG_INFO("[MavlinkAdapter] GIMBAL_MANAGER_STATUS from sys=%d comp=%d", m.sysid, m.compid);
}

} // namespace mg::core::mavlink
