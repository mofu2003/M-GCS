/**
 * mavlink_adapter_internal.h - MavlinkAdapter 内部共享状态
 *
 * This header defines the Impl struct which holds all mutable state.
 * Included by both mavlink_adapter.cc and mavlink_senders.cc.
 *
 * NOT part of the public API - never included outside core/src/mavlink/.
 *
 * Thread safety note:
 *   Each MavlinkAdapter::Impl owns its own SendCallback.  The send path
 *   always goes through Impl::send_msg() which uses Impl::send_cb directly.
 *   For the fallback comm_send_ch() path (used by MAVLink C library when
 *   MAVLINK_USE_CONVENIENCE_FUNCTIONS is defined), a thread_local pointer
 *   routes each call to the correct adapter without global state.
 */

#ifndef MGCORE_MAVLINK_ADAPTER_INTERNAL_H
#define MGCORE_MAVLINK_ADAPTER_INTERNAL_H

#include "core/mavlink/mavlink_adapter.h"
#include "core/mission/mission_types.h"
#include "core/log/logging.h"
#include "ardupilotmega/mavlink.h"
#include "standard/mavlink_msg_autopilot_version.h"
#include "core/firmware/firmware_adapter.h"
#include "core/ftp/mavlink_ftp.h"
#include "core/platform/platform_mutex.h"
#include "command_queue.h"
#include "param_cache.h"
#include "mission_transfer.h"
#include <array>
#include <cstring>
#include <chrono>
#include <deque>
#include <unordered_map>
#include <atomic>
#include <cassert>
#include <algorithm>

namespace mg::core::mavlink {

// ================================================================
// 通过 thread_local 实现每个适配器的发送路由
//
// comm_send_ch() 由 MAVLink C 库的 _mavlink_send_uart() 调用。
// 由于它不接收上下文参数，我们使用 thread_local 指针将字节路由到正确的适配器。
//
// 每次 send_msg() 在 MAVLink pack 调用前设置 s_active_send_cb，调用后清除。
// 因为 MAVLink pack 在调用线程上同步执行，所以不存在竞态条件。
// ================================================================

// 设置当前活跃的发送回调（thread_local）
static thread_local SendCallback* s_active_send_cb{nullptr};

// 设置活跃发送回调
inline void set_active_send_cb(SendCallback* cb) {
    s_active_send_cb = cb;
}

// MAVLink C 库的 comm_send_ch 回调，通过 thread_local 指针路由
extern "C" inline void comm_send_ch(mavlink_channel_t /*chan*/, uint8_t ch) {
    if (s_active_send_cb && *s_active_send_cb) {
        (*s_active_send_cb)(&ch, 1);
    }
}

// ================================================================
// Impl — 所有可变状态，受 platform::Mutex 保护
// ================================================================


// ================================================================
// 按职责拆分的子结构体
// ================================================================

// 遥测数据最新值和回调集合
struct TelemetryLatest {
    std::optional<HeartbeatInfo>        heartbeat;
    std::optional<AttitudeInfo>         attitude;
    std::optional<VfrHudInfo>           vfr_hud;
    std::optional<ImuInfo>              imu;
    std::optional<PressureInfo>         pressure;
    std::optional<FirmwareVersionInfo>  firmware_version;
    std::optional<SysStatusInfo>        sys_status;
    std::optional<GpsInfo>              gps;
    std::optional<RcChannelsInfo>       rc_channels;
    std::optional<ServoOutputInfo>      servo_output;
    std::optional<GlobalPositionInfo>   global_position;
    std::optional<HomePositionInfo>     home_position;
    std::optional<BatteryInfo>          battery;
    std::optional<HighresImuInfo>       highres_imu;
    std::optional<ExtendedSysStateInfo> extended_sys_state;
    std::optional<PrearmCheckInfo>      prearm_check;
    std::optional<VibrationInfo>        vibration;
    std::optional<EscStatusInfo>        esc_status;
    std::optional<WindEstimateInfo>     wind_estimate;
    std::optional<CommandAckInfo>       command_ack;
    std::vector<VideoStreamInfo>        video_streams;  // 已发现的视频流
    HeartbeatCallback       heartbeat_cb;
    SysStatusCallback       sys_status_cb;
    GpsCallback             gps_cb;
    ImuCallback             imu_cb;
    PressureCallback        pressure_cb;
    AttitudeCallback        attitude_cb;
    VfrHudCallback          vfr_hud_cb;
    FirmwareVersionCallback firmware_version_cb;
    ParamCallback           param_cb;
    CommandAckCallback      command_ack_cb;
    CommandLongCallback     command_long_cb;
    MagCalibrationProgressCallback mag_cal_progress_cb;
    MagCalibrationReportCallback mag_cal_report_cb;
    RcChannelsCallback      rc_channels_cb;
    ServoOutputCallback     servo_output_cb;
    GlobalPositionCallback  global_position_cb;
    HomePositionCallback    home_position_cb;
    BatteryCallback         battery_cb;
    HighresImuCallback      highres_imu_cb;
    ExtendedSysStateCallback extended_sys_state_cb;
    PrearmCheckCallback     prearm_check_cb;
    VibrationCallback       vibration_cb;
    EscStatusCallback       esc_status_cb;
    WindEstimateCallback    wind_estimate_cb;
    DistanceSensorCallback  distance_sensor_cb;
    AdsbVehicleCallback     adsb_vehicle_cb;
    VideoStreamCallback     video_stream_cb;
    MissionReachedCallback  mission_reached_cb;
    std::string last_statustext;
    uint8_t last_statustext_severity = 0;
    std::function<void(const std::string&, uint8_t)> statustext_cb;
    uint64_t last_heartbeat_ms = 0;
    bool     heartbeat_lost    = false;
};

// ================================================================
// ParamCache 和 MissionTransferState 已提取到独立头文件
// - param_cache.h: 参数缓存状态机
// - mission_transfer.h: 任务传输状态机 + select_transfer/find_uploading
// ================================================================

// MissionTransferState 已提取到 mission_transfer.h

// 命令 ACK 等待器（用于 wait_for_ack 的条件变量机制，保留向后兼容）
struct CommandAckWaiter {
    std::optional<CommandAckInfo> pending;
    bool ack_received = false;
    mg::core::platform::ConditionVariable cv;
};

struct LinkQualitySample {
    std::chrono::steady_clock::time_point received_at;
    uint8_t lost_before = 0;
};

struct LinkQualityWindow {
    static constexpr auto kWindow = std::chrono::seconds(3);

    bool target_known = false;
    uint8_t target_system = 0;
    uint8_t target_component = 0;
    bool sequence_known = false;
    uint8_t last_sequence = 0;
    std::deque<LinkQualitySample> samples;

    void observe(const mavlink_message_t& message) {
        if (!target_known) {
            if (message.msgid != MAVLINK_MSG_ID_HEARTBEAT) return;
            target_known = true;
            target_system = message.sysid;
            target_component = message.compid;
        }
        if (message.sysid != target_system || message.compid != target_component) {
            return;
        }

        uint8_t lost_before = 0;
        if (sequence_known) {
            const uint8_t difference = static_cast<uint8_t>(message.seq - last_sequence);
            if (difference == 0 || difference > 127) {
                return;
            }
            lost_before = static_cast<uint8_t>(difference - 1);
        }
        sequence_known = true;
        last_sequence = message.seq;
        samples.push_back({std::chrono::steady_clock::now(), lost_before});
        trim();
    }

    LinkQualityInfo snapshot() const {
        LinkQualityInfo info;
        const auto cutoff = std::chrono::steady_clock::now() - kWindow;
        for (const auto& sample : samples) {
            if (sample.received_at < cutoff) continue;
            info.received++;
            info.lost += sample.lost_before;
        }
        info.measuring = info.received < 2;
        if (info.received + info.lost > 0) {
            info.quality_pct = static_cast<uint8_t>(
                (info.received * 100) / (info.received + info.lost));
        }
        return info;
    }

private:
    void trim() {
        const auto cutoff = std::chrono::steady_clock::now() - kWindow;
        while (!samples.empty() && samples.front().received_at < cutoff) {
            samples.pop_front();
        }
    }
};

// 命令队列定义已提取到 command_queue.h
// Impl 中使用 CommandQueue 类型（通过 #include "command_queue.h"）

// 日志下载状态
struct LogState {
    std::vector<MavlinkAdapter::LogEntry> entries;
    
    // Download state
    bool     downloading = false;
    uint16_t download_id = 0;
    uint32_t download_ofs = 0;
    uint32_t download_size = 0;
    std::vector<uint8_t> download_data;
    std::vector<uint8_t> download_received;
    uint32_t download_received_bytes = 0;
    std::chrono::steady_clock::time_point download_last_data_at{};
    std::string download_filepath;
    uint8_t download_target_sys = 0;
    uint8_t download_target_comp = 0;
    std::function<void(uint32_t received, uint32_t total)> download_progress_cb;
    std::function<void(bool success, const std::string& path)> download_complete_cb;
    bool download_completed = false;
    bool download_cancelled = false;
    std::string download_error;
};

// MavlinkAdapter 的 PIMPL 实现，包含所有可变状态和消息处理逻辑
struct MavlinkAdapter::Impl {
    Impl()
        : ftp([this](const uint8_t* data, size_t size) {
            return send_cb ? send_cb(data, size) : false;
        }) {}

    mavlink_message_t msg{};
    mavlink_status_t  status{};
    uint64_t          msg_count = 0;
    LinkQualityWindow  link_quality;


    // === 子结构体组合 ===
    TelemetryLatest      tel;
    ParamCache           pc;
    MissionTransferState mission;
    MissionTransferState rally;
    MissionTransferState fence;
    LogState             log_state;
    CommandAckWaiter     ack_waiter;
    CommandQueue         cmd_queue;         // 命令队列（ACK 超时重发）
    SendCallback         send_cb;
    mg::core::ftp::MavlinkFtpClient ftp;
    std::vector<std::function<void()>> deferred_cbs;
    std::chrono::steady_clock::time_point last_timeout_check_;  // 超时检查节流
    std::array<uint8_t, MAVLINK_MAX_PACKET_LEN> mag_progress_frame{};
    size_t mag_progress_frame_len = 0;
    size_t mag_progress_frame_expected = 0;

    // The common dialect does not list ArduPilot message 191 in its CRC
    // table. Keep a tiny compatibility parser so a valid MAG_CAL_PROGRESS
    // frame still reaches the normal message dispatch path.
    void feed_mag_progress_compat(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            const uint8_t byte = data[i];
            if (mag_progress_frame_len == 0) {
                if (byte != MAVLINK_STX) continue;
                mag_progress_frame[mag_progress_frame_len++] = byte;
                continue;
            }
            if (mag_progress_frame_len >= mag_progress_frame.size()) {
                mag_progress_frame_len = 0;
                mag_progress_frame_expected = 0;
                continue;
            }
            mag_progress_frame[mag_progress_frame_len++] = byte;
            if (mag_progress_frame_len == 3) {
                const uint8_t payload_len = mag_progress_frame[1];
                const uint8_t incompat_flags = mag_progress_frame[2];
                mag_progress_frame_expected = 10u + payload_len + 2u +
                    ((incompat_flags & 0x01u) ? 13u : 0u);
            }
            if (mag_progress_frame_expected == 0 ||
                mag_progress_frame_len < mag_progress_frame_expected) continue;

            const uint32_t msgid = static_cast<uint32_t>(mag_progress_frame[7]) |
                (static_cast<uint32_t>(mag_progress_frame[8]) << 8) |
                (static_cast<uint32_t>(mag_progress_frame[9]) << 16);
            if (msgid == MAVLINK_MSG_ID_MAG_CAL_PROGRESS) {
                const uint8_t payload_len = mag_progress_frame[1];
                const uint16_t crc_offset = static_cast<uint16_t>(10u + payload_len);
                uint16_t checksum = crc_calculate(&mag_progress_frame[1],
                                                  static_cast<uint16_t>(9u + payload_len));
                crc_accumulate(MAVLINK_MSG_ID_MAG_CAL_PROGRESS_CRC, &checksum);
                const uint16_t received = static_cast<uint16_t>(mag_progress_frame[crc_offset]) |
                    (static_cast<uint16_t>(mag_progress_frame[crc_offset + 1]) << 8);
                if (checksum == received && payload_len >= MAVLINK_MSG_ID_MAG_CAL_PROGRESS_MIN_LEN) {
                    mavlink_message_t message{};
                    message.len = payload_len;
                    message.incompat_flags = mag_progress_frame[2];
                    message.compat_flags = mag_progress_frame[3];
                    message.seq = mag_progress_frame[4];
                    message.sysid = mag_progress_frame[5];
                    message.compid = mag_progress_frame[6];
                    message.msgid = msgid;
                    memcpy(_MAV_PAYLOAD_NON_CONST(&message), &mag_progress_frame[10], payload_len);
                    process(message);
                }
            }
            mag_progress_frame_len = 0;
            mag_progress_frame_expected = 0;
        }
    }

    mutable mg::core::platform::Mutex mtx;  // 加锁保护所有可变状态

    // 通过注册的回调发送当前 msg 缓冲区。
    // 设置 thread_local s_active_send_cb 以便 comm_send_ch() 路由到本适配器。
    bool send_msg() {
        if (!send_cb) return false;
        set_active_send_cb(&send_cb);
        uint8_t buf[300];
        uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
        set_active_send_cb(nullptr);
        return send_cb(buf, len);
    }

    bool send_command_queue_entry(const CommandQueueEntry& e) {
        mavlink_msg_command_long_pack(
            255, 0, &msg,
            e.target_sys, e.target_comp,
            e.command, e.confirmation,
            e.params[0], e.params[1], e.params[2], e.params[3],
            e.params[4], e.params[5], e.params[6]);
        return send_msg();
    }

    // ================================================================
    // 消息处理器（在 process() 中持锁调用）
    // ================================================================

    // 处理心跳消息
    void handle_heartbeat(const mavlink_message_t& m) {
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
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        tel.heartbeat_lost = false;
        if (tel.heartbeat_cb) deferred_cbs.push_back([cb = tel.heartbeat_cb, info]() { cb(info); });
    }

    // 处理系统状态消息
    void handle_sys_status(const mavlink_message_t& m) {
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
        if (tel.sys_status_cb) deferred_cbs.push_back([cb = tel.sys_status_cb, info]() { cb(info); });
    }

    // 处理 GPS 原始数据
    void handle_gps(const mavlink_message_t& m) {
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
        if (tel.gps_cb) deferred_cbs.push_back([cb = tel.gps_cb, info]() { cb(info); });
    }

    // 处理缩放 IMU 数据。SCALED_IMU/2/3（26/116/129）三个消息的字段顺序、
    // 类型和长度完全相同，只有消息 ID 不同，所以共用同一个解码函数，
    // 用 instance 参数区分是第几个 IMU。
    void handle_scaled_imu(const mavlink_message_t& m, uint8_t instance) {
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
        if (instance == 0) tel.imu = info;
        if (tel.imu_cb) deferred_cbs.push_back([cb = tel.imu_cb, info]() { cb(info); });
    }

    // 处理气压计数据
    void handle_scaled_pressure(const mavlink_message_t& m) {
        mavlink_scaled_pressure_t sp;
        mavlink_msg_scaled_pressure_decode(&m, &sp);
        PressureInfo info{};
        info.system_id = m.sysid;
        info.press_abs = sp.press_abs;
        info.press_diff = sp.press_diff;
        info.temperature = sp.temperature;
        tel.pressure = info;
        if (tel.pressure_cb) deferred_cbs.push_back([cb = tel.pressure_cb, info]() { cb(info); });
    }

    // 处理姿态数据
    void handle_attitude(const mavlink_message_t& m) {
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
        if (tel.attitude_cb) deferred_cbs.push_back([cb = tel.attitude_cb, info]() { cb(info); });
    }

    // 处理 VFR HUD 飞行仪表数据
    void handle_vfr_hud(const mavlink_message_t& m) {
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
        if (tel.vfr_hud_cb) deferred_cbs.push_back([cb = tel.vfr_hud_cb, info]() { cb(info); });
    }

    // 处理飞控版本信息
    void handle_autopilot_version(const mavlink_message_t& m) {
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
        if (tel.firmware_version_cb) deferred_cbs.push_back([cb = tel.firmware_version_cb, info]() { cb(info); });
    }

    // 处理参数值响应
    // 处理参数值（下载接收 + 写入确认）
    void handle_param_value(const mavlink_message_t& m) {
        mavlink_param_value_t pv;
        mavlink_msg_param_value_decode(&m, &pv);
        ParamInfo info;
        info.name = std::string(pv.param_id, strnlen(pv.param_id, 16));
        info.value = pv.param_value;
        info.type = pv.param_type;
        info.index = pv.param_index;
        info.count = pv.param_count;

        // 检查是否是写入确认（参数名在 pending_writes 中）
        auto write_it = pc.pending_writes.find(info.name);
        if (write_it != pc.pending_writes.end()) {
            bool accepted = (std::abs(write_it->second.value - info.value) < 1e-6f);
            if (write_it->second.result_cb) {
                auto cb = write_it->second.result_cb;
                deferred_cbs.push_back([cb, accepted]() { cb(accepted); });
            }
            pc.pending_writes.erase(write_it);
        }

        // 存储参数
        pc.param_map[info.name] = info;
        pc.expected_param_count = pv.param_count;

        // 更新下载状态
        pc.on_param_received(pv.param_index);

        // 首次收到 count 后构建缺失列表
        if (pc.expected_param_count > 0 && pc.download_state == ParamCache::DownloadState::Receiving) {
            // 检查是否全部收到
            if (pc.param_map.size() >= pc.expected_param_count) {
                pc.download_state = ParamCache::DownloadState::Complete;
            }
        }

        if (tel.param_cb) deferred_cbs.push_back([cb = tel.param_cb, info]() { cb(info); });
    }

    // 处理命令确认（路由到命令队列 + 唤醒旧 waiter）
    void handle_command_ack(const mavlink_message_t& m) {
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
        if (tel.command_ack_cb) deferred_cbs.push_back([cb = tel.command_ack_cb, info]() { cb(info); });
    }

    void handle_command_long(const mavlink_message_t& m) {
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

    void handle_mag_cal_progress(const mavlink_message_t& m) {
        // Decode the fixed leading bytes directly so calibration progress
        // remains usable from the compact common wire representation.
        // The compact fields below occupy payload bytes 12..16.  Reject
        // truncated frames before touching them; malformed MAVLink frames can
        // otherwise turn a protocol error into an out-of-bounds read.
        constexpr uint8_t kCompactProgressMinLen = 17;
        if (m.len < kCompactProgressMinLen) return;
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

    void handle_mag_cal_report(const mavlink_message_t& m) {
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

    // 处理遥控通道数据
    void handle_rc_channels(const mavlink_message_t& m) {
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
        if (tel.rc_channels_cb) deferred_cbs.push_back([cb = tel.rc_channels_cb, info]() { cb(info); });
    }

    // 处理舵机原始输出
    void handle_servo_output_raw(const mavlink_message_t& m) {
        mavlink_servo_output_raw_t so;
        mavlink_msg_servo_output_raw_decode(&m, &so);
        // The 1-16 page is backed by port 0.  ArduPilot uses port 1 for
        // channels 17-32; never let that second packet overwrite channels 1-16.
        if (so.port != 0) return;
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
        if (tel.servo_output_cb) deferred_cbs.push_back([cb = tel.servo_output_cb, info]() { cb(info); });
    }

    // 处理飞控状态文本
    void handle_statustext(const mavlink_message_t& m) {
        mavlink_statustext_t st;
        mavlink_msg_statustext_decode(&m, &st);
        tel.last_statustext = std::string(st.text, strnlen(st.text, sizeof(st.text)));
        tel.last_statustext_severity = st.severity;
        if (tel.statustext_cb) deferred_cbs.push_back([cb = tel.statustext_cb, text = tel.last_statustext, severity = tel.last_statustext_severity]() { cb(text, severity); });
    }

    // 处理全局位置数据
    void handle_global_position(const mavlink_message_t& m) {
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
        if (tel.global_position_cb) deferred_cbs.push_back([cb = tel.global_position_cb, info]() { cb(info); });
    }

    // 处理返航点位置
    void handle_home_position(const mavlink_message_t& m) {
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
        if (tel.home_position_cb) deferred_cbs.push_back([cb = tel.home_position_cb, info]() { cb(info); });
    }

    // 处理电池状态
    void handle_battery_status(const mavlink_message_t& m) {
        mavlink_battery_status_t bs;
        mavlink_msg_battery_status_decode(&m, &bs);
        BatteryInfo info{};
        info.system_id = m.sysid;
        info.voltage_v = 0.0f;
        for (int vi = 0; vi < 10 && bs.voltages[vi] != UINT16_MAX; vi++) info.voltage_v += bs.voltages[vi] / 1000.0f;
        info.current_a = (bs.current_battery != -1) ? bs.current_battery / 100.0f : 0.0f;
        info.battery_remaining = bs.battery_remaining;
        info.consumed_mah = bs.current_consumed;
        tel.battery = info;
        if (tel.battery_cb) deferred_cbs.push_back([cb = tel.battery_cb, info]() { cb(info); });
        // Battery failsafe event: fire when battery is critically low
        if (info.battery_remaining >= 0 && info.battery_remaining <= 10) {
            if (tel.battery_cb) {
                // The MG_EVENT_BATTERY_FAILSAFE will be handled in the FFI layer
                // by checking battery_remaining in the telemetry cache
            }
        }
    }

    // 处理高分辨率 IMU 数据
    void handle_highres_imu(const mavlink_message_t& m) {
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
        info.abs_pressure = hi.abs_pressure / 100.0f;  // Pa -> hPa
        info.diff_pressure = hi.diff_pressure / 100.0f;  // Pa -> hPa
        info.pressure_alt = hi.pressure_alt;
        info.temperature = hi.temperature;
        info.fields_updated = hi.fields_updated;
        // 同 SCALED_IMU：latest_highres_imu() 只代表主 IMU。
        if (info.instance == 0) tel.highres_imu = info;
        if (tel.highres_imu_cb) deferred_cbs.push_back([cb = tel.highres_imu_cb, info]() { cb(info); });
    }

    // 处理扩展系统状态
    void handle_extended_sys_state(const mavlink_message_t& m) {
        mavlink_extended_sys_state_t es;
        mavlink_msg_extended_sys_state_decode(&m, &es);
        ExtendedSysStateInfo info{};
        info.system_id = m.sysid;
        info.vtol_state = es.vtol_state;
        info.landed_state = es.landed_state;
        tel.extended_sys_state = info;
        if (tel.extended_sys_state_cb) deferred_cbs.push_back([cb = tel.extended_sys_state_cb, info]() { cb(info); });
    }


    // 处理振动数据
    void handle_vibration(const mavlink_message_t& m) {
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
        if (tel.vibration_cb) deferred_cbs.push_back([cb = tel.vibration_cb, info]() { cb(info); });
    }

    // 处理电调状态数据
    void handle_esc_status(const mavlink_message_t& m) {
        mavlink_esc_status_t esc;
        mavlink_msg_esc_status_decode(&m, &esc);
        EscStatusInfo info{};
        info.system_id = m.sysid;
        info.index = esc.index;
        info.count = 4;  // ESC_STATUS always covers 4 ESCs per message
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
        if (tel.esc_status_cb) deferred_cbs.push_back([cb = tel.esc_status_cb, info]() { cb(info); });
    }

    // 处理距离传感器数据
    void handle_distance_sensor(const mavlink_message_t& m) {
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
        if (tel.distance_sensor_cb) deferred_cbs.push_back([cb = tel.distance_sensor_cb, info]() { cb(info); });
    }

    // 处理 ADS-B 飞行器数据
    void handle_adsb_vehicle(const mavlink_message_t& m) {
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
        if (tel.adsb_vehicle_cb) deferred_cbs.push_back([cb = tel.adsb_vehicle_cb, info]() { cb(info); });
    }

    // 处理视频流信息（VIDEO_STREAM_INFORMATION, msg 269）
    void handle_video_stream_information(const mavlink_message_t& m) {
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
            if (s.stream_id == info.stream_id) { s = info; found = true; break; }
        }
        if (!found) tel.video_streams.push_back(info);

        if (tel.video_stream_cb) deferred_cbs.push_back([cb = tel.video_stream_cb, info]() { cb(info); });
    }

    // ================================================================
    // 相机/云台消息处理器
    // ================================================================

    // 相机信息（msg 261）— 转发给 CameraManager 解析
    void handle_camera_information(const mavlink_message_t& m) {
        mavlink_camera_information_t ci;
        mavlink_msg_camera_information_decode(&m, &ci);
        // CameraManager 在 Vehicle 层，此处只记录日志
        // 实际解析由 wire_post_connect_callbacks 中的相机回调处理
        MGC_LOG_INFO("[MavlinkAdapter] CAMERA_INFORMATION: model=%s vendor=%s",
                     ci.model_name, ci.vendor_name);
    }

    // 相机拍照状态（msg 254）
    void handle_camera_capture_status(const mavlink_message_t& m) {
        mavlink_camera_capture_status_t cs;
        mavlink_msg_camera_capture_status_decode(&m, &cs);
        MGC_LOG_INFO("[MavlinkAdapter] CAMERA_CAPTURE_STATUS: image_status=%d video_status=%d count=%d",
                     cs.image_status, cs.video_status, cs.image_count);
    }

    // 云台管理器信息（msg 280）
    void handle_gimbal_manager_info(const mavlink_message_t& m) {
        MGC_LOG_INFO("[MavlinkAdapter] GIMBAL_MANAGER_INFORMATION from sys=%d comp=%d", m.sysid, m.compid);
    }

    // 云台管理器状态（msg 281）
    void handle_gimbal_manager_status(const mavlink_message_t& m) {
        MGC_LOG_INFO("[MavlinkAdapter] GIMBAL_MANAGER_STATUS from sys=%d comp=%d", m.sysid, m.compid);
    }

    // ================================================================
    // 任务协议处理器
    // ================================================================

    // 按 mission_type 选择对应的传输状态（委托 mission_transfer.h 自由函数）
    MissionTransferState& select_transfer(uint8_t mission_type) {
        return mg::core::mavlink::select_transfer(mission, rally, fence, mission_type);
    }

    // 查找当前正在上传的传输状态（委托 mission_transfer.h 自由函数）
    MissionTransferState* find_uploading() {
        return mg::core::mavlink::find_uploading(mission, rally, fence);
    }

    // 处理任务计数消息（开始下载）
    void handle_mission_count(const mavlink_message_t& m) {
        mavlink_mission_count_t mc;
        mavlink_msg_mission_count_decode(&m, &mc);

        auto& state = select_transfer(mc.mission_type);
        state.record_recv();
        state.download_total = mc.count;
        state.download_seq = 0;
        state.plan.items.clear();
        ++state.plan_revision;
        state.plan.items.reserve(mc.count);
        state.progress = mg::core::mission::MissionTransferProgress{};
        state.progress.state = mg::core::mission::MissionState::Downloading;
        state.progress.total = mc.count;
        state.progress.received = 0;

        if (mc.count == 0) {
            state.progress.state = mg::core::mission::MissionState::Idle;
            state.plan.valid = true;
            state.last_completed_download_revision = state.plan_revision;
            ++state.completed_download_count;
        } else {
            send_mission_request_int(m.sysid, m.compid, 0);
            state.record_send(m.sysid, m.compid, [this, &state, sys = m.sysid, comp = m.compid]() {
                send_mission_request_int(sys, comp, state.download_seq);
            });
        }
    }

    // 处理任务项（INT 格式，用于下载接收）
    void handle_mission_item_int(const mavlink_message_t& m) {
        mavlink_mission_item_int_t mi;
        mavlink_msg_mission_item_int_decode(&m, &mi);

        mg::core::mission::MissionItem item{};
        item.seq = mi.seq;
        item.frame = mi.frame;
        item.command = mi.command;
        item.current = mi.current;
        item.autocontinue = mi.autocontinue;
        item.param1 = mi.param1;
        item.param2 = mi.param2;
        item.param3 = mi.param3;
        item.param4 = mi.param4;
        item.x = mi.x;
        item.y = mi.y;
        item.z = mi.z;
        item.mission_type = mi.mission_type;

        auto& state = select_transfer(mi.mission_type);
        if (mi.seq >= state.plan.items.size()) {
            state.plan.items.resize(mi.seq + 1);
        }
        state.plan.items[mi.seq] = item;
        ++state.plan_revision;
        state.download_seq = mi.seq + 1;
        state.progress.received = state.download_seq;
        state.record_recv();

        if (state.download_seq < state.download_total) {
            send_mission_request_int(m.sysid, m.compid, state.download_seq);
            state.record_send(m.sysid, m.compid, [this, &state, sys = m.sysid, comp = m.compid]() {
                send_mission_request_int(sys, comp, state.download_seq);
            });
        } else {
            state.plan.valid = true;
            state.last_completed_download_revision = state.plan_revision;
            ++state.completed_download_count;
            send_mission_ack(m.sysid, m.compid, MAV_MISSION_ACCEPTED);
            state.progress.state = mg::core::mission::MissionState::Idle;
            state.resend_fn = nullptr;
        }
    }

    // 处理旧版任务项（浮点格式，用于兼容下载接收）
    void handle_mission_item(const mavlink_message_t& m) {
        mavlink_mission_item_t mi;
        mavlink_msg_mission_item_decode(&m, &mi);

        mg::core::mission::MissionItem item{};
        item.seq = mi.seq;
        item.frame = mi.frame;
        item.command = mi.command;
        item.current = mi.current;
        item.autocontinue = mi.autocontinue;
        item.param1 = mi.param1;
        item.param2 = mi.param2;
        item.param3 = mi.param3;
        item.param4 = mi.param4;
        if (mi.frame == MAV_FRAME_GLOBAL ||
            mi.frame == MAV_FRAME_GLOBAL_RELATIVE_ALT ||
            mi.frame == MAV_FRAME_GLOBAL_TERRAIN_ALT) {
            item.x = static_cast<int32_t>(mi.x * 1e7f);
            item.y = static_cast<int32_t>(mi.y * 1e7f);
        } else {
            item.x = static_cast<int32_t>(mi.x);
            item.y = static_cast<int32_t>(mi.y);
        }
        item.z = mi.z;
        item.mission_type = mi.mission_type;

        auto& state = select_transfer(mi.mission_type);
        if (mi.seq >= state.plan.items.size()) {
            state.plan.items.resize(mi.seq + 1);
        }
        state.plan.items[mi.seq] = item;
        ++state.plan_revision;
        state.download_seq = mi.seq + 1;
        state.progress.received = state.download_seq;
        state.record_recv();

        if (state.download_seq < state.download_total) {
            send_mission_request_int(m.sysid, m.compid, state.download_seq);
            state.record_send(m.sysid, m.compid, [this, &state, sys = m.sysid, comp = m.compid]() {
                send_mission_request_int(sys, comp, state.download_seq);
            });
        } else {
            state.plan.valid = true;
            state.last_completed_download_revision = state.plan_revision;
            ++state.completed_download_count;
            send_mission_ack(m.sysid, m.compid, MAV_MISSION_ACCEPTED);
            state.progress.state = mg::core::mission::MissionState::Idle;
            state.resend_fn = nullptr;
        }
    }

    void mark_mission_item_sent(MissionTransferState& state, uint16_t seq) {
        state.upload_seq = seq + 1;
        state.progress.sent = state.upload_seq;
    }

    // 处理任务请求（INT 格式，用于上传响应）
    void handle_mission_request_int(const mavlink_message_t& m) {
        mavlink_mission_request_int_t mr;
        mavlink_msg_mission_request_int_decode(&m, &mr);

        auto* state = find_uploading();
        if (!state || mr.seq >= state->upload_total) return;

        state->record_recv();
        auto item = state->plan.items[mr.seq];
        send_mission_item_int(m.sysid, m.compid, item);
        mark_mission_item_sent(*state, mr.seq);
        state->record_send(m.sysid, m.compid, [this, sys = m.sysid, comp = m.compid, item]() {
            send_mission_item_int(sys, comp, item);
        });
    }

    // 处理旧版任务请求（非 INT 格式，用旧版 MISSION_ITEM 回复）
    void handle_mission_request(const mavlink_message_t& m) {
        mavlink_mission_request_t mr;
        mavlink_msg_mission_request_decode(&m, &mr);

        auto* state = find_uploading();
        if (!state || mr.seq >= state->upload_total) return;

        state->record_recv();
        auto item = state->plan.items[mr.seq];
        send_mission_item(m.sysid, m.compid, item);
        mark_mission_item_sent(*state, mr.seq);
        state->record_send(m.sysid, m.compid, [this, sys = m.sysid, comp = m.compid, item]() {
            send_mission_item(sys, comp, item);
        });
    }

    // 处理任务确认消息
    void handle_mission_ack(const mavlink_message_t& m) {
        mavlink_mission_ack_t ack;
        mavlink_msg_mission_ack_decode(&m, &ack);

        auto& state = select_transfer(ack.mission_type);
        state.record_recv();
        state.resend_fn = nullptr;
        state.progress.result = ack.type;
        state.progress.state = mg::core::mission::MissionState::Idle;

        if (ack.type == MAV_MISSION_ACCEPTED) {
            if (state.progress.received > 0 || state.progress.sent >= state.progress.total) {
                state.plan.valid = true;
            }
        } else {
            const char* label = (ack.mission_type == MAV_MISSION_TYPE_RALLY) ? "Rally" :
                                (ack.mission_type == MAV_MISSION_TYPE_FENCE) ? "Fence" : "Mission";
            state.progress.error = std::string(label) + " rejected: result=" + std::to_string(ack.type);
        }
    }
    
    // 处理日志条目列表
    void handle_log_entry(const mavlink_message_t& m) {
        mavlink_log_entry_t le;
        mavlink_msg_log_entry_decode(&m, &le);
        LogEntry entry;
        entry.id = le.id;
        entry.time_utc = le.time_utc;
        entry.size_bytes = le.size;
        log_state.entries.push_back(entry);
    }

    // 处理日志数据块（完成时写入文件）
    void handle_log_data(const mavlink_message_t& m) {
        if (!log_state.downloading) return;
        mavlink_log_data_t ld;
        mavlink_msg_log_data_decode(&m, &ld);
        if (ld.id != log_state.download_id) return;

        const uint32_t end = ld.ofs + ld.count;
        // 日志列表提供了已知大小时，忽略飞控偶发发送到文件尾之外的字节，
        // 避免额外字节被计入完成条件或写入本地文件。
        const uint32_t write_end = log_state.download_size > 0
            ? std::min(end, log_state.download_size)
            : end;
        if (write_end > log_state.download_data.size()) {
            log_state.download_data.resize(write_end);
        }
        if (write_end > log_state.download_received.size()) {
            log_state.download_received.resize(write_end, 0);
        }
        for (uint32_t i = 0; i < ld.count; ++i) {
            const auto index = ld.ofs + i;
            if (log_state.download_size > 0 && index >= log_state.download_size) {
                break;
            }
            log_state.download_data[index] = ld.data[i];
            if (!log_state.download_received[index]) {
                log_state.download_received[index] = 1;
                ++log_state.download_received_bytes;
            }
        }
        log_state.download_ofs = write_end;
        log_state.download_last_data_at = std::chrono::steady_clock::now();
        if (log_state.download_size == 0 && ld.count < 90) {
            log_state.download_size = end;
        }

        const bool complete =
            (log_state.download_size > 0 &&
             log_state.download_received_bytes >= log_state.download_size) ||
            (ld.count == 0 && log_state.download_size == 0);
        if (complete) {
            // 延迟文件写入到锁外执行，避免在 Impl::mtx 锁内做阻塞 I/O
            auto filepath = log_state.download_filepath;
            auto data = std::move(log_state.download_data);
            if (log_state.download_size > 0 && data.size() > log_state.download_size) {
                data.resize(log_state.download_size);
            }
            deferred_cbs.push_back([filepath, data = std::move(data)]() {
                FILE* f = fopen(filepath.c_str(), "wb");
                if (f) {
                    size_t written = fwrite(data.data(), 1, data.size(), f);
                    fclose(f);
                    if (written != data.size()) {
                        MGC_LOG_WARN("[MavlinkAdapter] Log file write incomplete: %zu / %zu bytes", written, data.size());
                    }
                } else {
                    MGC_LOG_ERROR("[MavlinkAdapter] Failed to open log file: %s", filepath.c_str());
                }
            });
            log_state.downloading = false;
            log_state.download_completed = true;
        } else {
            mavlink_msg_log_request_data_pack(
                255, 0, &msg,
                log_state.download_target_sys, log_state.download_target_comp,
                log_state.download_id, log_state.download_ofs, 90);
            send_msg();
        }
    }

    // 处理当前任务航点
    void handle_mission_current(const mavlink_message_t& m) {
        mavlink_mission_current_t mc;
        mavlink_msg_mission_current_decode(&m, &mc);
        mission.plan.current_index = mc.seq;
    }
    
    // 处理任务航点到达通知
    void handle_mission_item_reached(const mavlink_message_t& m) {
        mavlink_mission_item_reached_t mr;
        mavlink_msg_mission_item_reached_decode(&m, &mr);
        if (tel.mission_reached_cb) deferred_cbs.push_back([cb = tel.mission_reached_cb, seq = mr.seq]() { cb(seq); });
    }
    
    // 任务发送辅助函数
    // 请求下载下一个任务项
    bool send_mission_request_int(uint8_t sysid, uint8_t compid, uint16_t seq) {
        mavlink_msg_mission_request_int_pack(
            255, 0, &msg,
            sysid, compid, seq, 0);
        return send_msg();
    }
    
    // 发送任务确认
    bool send_mission_ack(uint8_t sysid, uint8_t compid, uint8_t type) {
        mavlink_msg_mission_ack_pack(
            255, 0, &msg,
            sysid, compid, type, 0, 0);
        return send_msg();
    }
    
    // 发送任务项（INT 格式）
    bool send_mission_item_int(uint8_t sysid, uint8_t compid, const mg::core::mission::MissionItem& item) {
        mavlink_msg_mission_item_int_pack(
            255, 0, &msg,
            sysid, compid,
            item.seq, item.frame, item.command,
            item.current, item.autocontinue,
            item.param1, item.param2, item.param3, item.param4,
            item.x, item.y, item.z,
            item.mission_type);
        return send_msg();
    }

    // 发送任务项（旧版浮点格式），用于兼容只请求 MISSION_ITEM 的飞控。
    bool send_mission_item(uint8_t sysid, uint8_t compid, const mg::core::mission::MissionItem& item) {
        float x = static_cast<float>(item.x);
        float y = static_cast<float>(item.y);
        if (item.frame == MAV_FRAME_GLOBAL ||
            item.frame == MAV_FRAME_GLOBAL_RELATIVE_ALT ||
            item.frame == MAV_FRAME_GLOBAL_TERRAIN_ALT) {
            x = static_cast<float>(static_cast<double>(item.x) / 1e7);
            y = static_cast<float>(static_cast<double>(item.y) / 1e7);
        }
        mavlink_msg_mission_item_pack(
            255, 0, &msg,
            sysid, compid,
            item.seq, item.frame, item.command,
            item.current, item.autocontinue,
            item.param1, item.param2, item.param3, item.param4,
            x, y, item.z,
            item.mission_type);
        return send_msg();
    }

    // 发送 PARAM_REQUEST_READ（按索引请求单个参数）
    bool send_param_request_read_by_index(uint8_t sysid, uint8_t compid, int16_t param_index) {
        char empty_name[16] = {};
        mavlink_msg_param_request_read_pack(
            255, 0, &msg,
            sysid, compid, empty_name, param_index);
        return send_msg();
    }

    // 处理参数超时（在 process() 中调用）
    void process_param_timeouts() {
        int action = pc.check_timeouts(deferred_cbs);
        switch (action) {
            case 1: {
                // 重试 PARAM_REQUEST_LIST
                mavlink_msg_param_request_list_pack(
                    255, 0, &msg, pc.target_sys, pc.target_comp);
                send_msg();
                break;
            }
            case 2: {
                // 请求缺失参数批次
                for (uint16_t idx : pc.batch_queue) {
                    send_param_request_read_by_index(pc.target_sys, pc.target_comp, idx);
                }
                break;
            }
            default: break;
        }
    }

    // 处理完整解码的 MAVLink 消息（获取锁，处理后在锁外执行回调）
    void process(const mavlink_message_t& m) {
        std::vector<std::function<void()>> callbacks;
        {
            mg::core::platform::LockGuard lk(mtx);
            msg_count++;
            link_quality.observe(m);

            switch (m.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT:           handle_heartbeat(m); break;
            case MAVLINK_MSG_ID_SYS_STATUS:          handle_sys_status(m); break;
            case MAVLINK_MSG_ID_GPS_RAW_INT:         handle_gps(m); break;
            case MAVLINK_MSG_ID_SCALED_IMU:          handle_scaled_imu(m, 0); break;
            case MAVLINK_MSG_ID_SCALED_IMU2:         handle_scaled_imu(m, 1); break;
            case MAVLINK_MSG_ID_SCALED_IMU3:         handle_scaled_imu(m, 2); break;
            case MAVLINK_MSG_ID_SCALED_PRESSURE:     handle_scaled_pressure(m); break;
            case MAVLINK_MSG_ID_ATTITUDE:            handle_attitude(m); break;
            case MAVLINK_MSG_ID_VFR_HUD:             handle_vfr_hud(m); break;
            case MAVLINK_MSG_ID_AUTOPILOT_VERSION:   handle_autopilot_version(m); break;
            case MAVLINK_MSG_ID_PARAM_VALUE:         handle_param_value(m); break;
            case MAVLINK_MSG_ID_COMMAND_ACK:         handle_command_ack(m); break;
            case MAVLINK_MSG_ID_COMMAND_LONG:        handle_command_long(m); break;
            case 191:                                 handle_mag_cal_progress(m); break;
            case MAVLINK_MSG_ID_MAG_CAL_REPORT:       handle_mag_cal_report(m); break;
            case MAVLINK_MSG_ID_RC_CHANNELS:        handle_rc_channels(m); break;
            case MAVLINK_MSG_ID_SERVO_OUTPUT_RAW:   handle_servo_output_raw(m); break;
            case MAVLINK_MSG_ID_STATUSTEXT:        handle_statustext(m); break;
            case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: handle_global_position(m); break;
            case MAVLINK_MSG_ID_HOME_POSITION:        handle_home_position(m); break;
            case MAVLINK_MSG_ID_MISSION_COUNT:       handle_mission_count(m); break;
            case MAVLINK_MSG_ID_MISSION_ITEM:        handle_mission_item(m); break;
            case MAVLINK_MSG_ID_MISSION_ITEM_INT:    handle_mission_item_int(m); break;
            case MAVLINK_MSG_ID_MISSION_REQUEST_INT: handle_mission_request_int(m); break;
            case MAVLINK_MSG_ID_MISSION_REQUEST:     handle_mission_request(m); break;
            case MAVLINK_MSG_ID_MISSION_ACK:         handle_mission_ack(m); break;
            case MAVLINK_MSG_ID_MISSION_CURRENT:     handle_mission_current(m); break;
            case MAVLINK_MSG_ID_MISSION_ITEM_REACHED: handle_mission_item_reached(m); break;
            case MAVLINK_MSG_ID_BATTERY_STATUS:      handle_battery_status(m); break;
            case MAVLINK_MSG_ID_HIGHRES_IMU:         handle_highres_imu(m); break;
            case MAVLINK_MSG_ID_EXTENDED_SYS_STATE:  handle_extended_sys_state(m); break;
            // PREARM_CHECK_STATUS not in standard MAVLink v2.0 headers (ArduPilot extension)
            case MAVLINK_MSG_ID_VIBRATION:            handle_vibration(m); break;
            case MAVLINK_MSG_ID_ESC_STATUS:            handle_esc_status(m); break;
            // WIND_ESTIMATE not in standard MAVLink v2.0 common headers
            case MAVLINK_MSG_ID_LOG_ENTRY:               handle_log_entry(m); break;
            case MAVLINK_MSG_ID_LOG_DATA:                handle_log_data(m); break;
            case MAVLINK_MSG_ID_DISTANCE_SENSOR:        handle_distance_sensor(m); break;
            case MAVLINK_MSG_ID_ADSB_VEHICLE:           handle_adsb_vehicle(m); break;
            case 110: { // FILE_TRANSFER_PROTOCOL
                mavlink_file_transfer_protocol_t ftp_message{};
                mavlink_msg_file_transfer_protocol_decode(&m, &ftp_message);
                ftp.handle_ftp_message(ftp_message);
                break;
            }
            // 相机/云台消息
            case 261:   /* CAMERA_INFORMATION */         handle_camera_information(m); break;
            case 254:   /* CAMERA_CAPTURE_STATUS */      handle_camera_capture_status(m); break;
            case 269:   /* VIDEO_STREAM_INFORMATION */  handle_video_stream_information(m); break;
            case 280:   /* GIMBAL_MANAGER_INFORMATION */  handle_gimbal_manager_info(m); break;
            case 281:   /* GIMBAL_MANAGER_STATUS */       handle_gimbal_manager_status(m); break;
            default: break;
            }

            // 超时检查节流：每 100ms 检查一次，避免每条消息都扫描队列
            auto now = std::chrono::steady_clock::now();
            auto since_check = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_timeout_check_).count();
            if (since_check >= 100) {
                last_timeout_check_ = now;

                // 命令队列超时检查
                auto timed_out = cmd_queue.check_timeouts(
                    [this](const CommandQueueEntry& e) -> bool {
                        return send_command_queue_entry(e);
                    });
                for (auto& entry : timed_out) {
                    if (entry.result_cb) {
                        CommandAckInfo fail_info{};
                        fail_info.command = entry.command;
                        fail_info.result = 4;  // MAV_RESULT_FAILED
                        fail_info.result_text = "command ack timeout - no response";
                        deferred_cbs.push_back([cb = entry.result_cb, fail_info = std::move(fail_info)]() { cb(fail_info); });
                    }
                }
                cmd_queue.purge();

                // 任务传输超时检查
                mission.check_timeout();
                rally.check_timeout();
                fence.check_timeout();

                // 参数下载/写入超时
                process_param_timeouts();
            }

            // 将延迟回调移出，以便在锁释放后执行
            callbacks = std::move(deferred_cbs);
            deferred_cbs.clear();
        } // LockGuard 在此释放（RAII）

        // 在锁外执行回调（防止 POSIX 死锁）
        for (auto& cb : callbacks) { cb(); }
    }
};

} // namespace mg::core::mavlink

#endif /* MGCORE_MAVLINK_ADAPTER_INTERNAL_H */
