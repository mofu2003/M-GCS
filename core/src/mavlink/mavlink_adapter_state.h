/**
 * mavlink_adapter_state.h - MavlinkAdapter internal state containers
 *
 * Kept separate from mavlink_adapter_internal.h so the adapter's mutable
 * state can evolve without growing the message-dispatch header.
 *
 * NOT part of the public API - never included outside core/src/mavlink/.
 */

#ifndef MGCORE_MAVLINK_ADAPTER_STATE_H
#define MGCORE_MAVLINK_ADAPTER_STATE_H

#include "core/mavlink/mavlink_adapter.h"
#include "core/platform/platform_mutex.h"
#include "ardupilotmega/mavlink.h"

#include <chrono>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace mg::core::mavlink {

// Latest telemetry values and registered callback sinks.
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
    std::vector<VideoStreamInfo>        video_streams;
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
    bool     heartbeat_lost = false;
};

// Command ACK waiter retained for the legacy wait_for_ack path.
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

// Log download state. Entries share the public LogEntry representation.
struct LogState {
    std::vector<MavlinkAdapter::LogEntry> entries;

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

} // namespace mg::core::mavlink

#endif /* MGCORE_MAVLINK_ADAPTER_STATE_H */
