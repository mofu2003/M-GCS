#include "core/vehicle/vehicle.h"
#include "telemetry_bindings.h"
#include "json_serializers.h"
#include "param_json_serializers.h"
#include "core/firmware/firmware_adapter.h"
#include "core/log/logging.h"
#include <chrono>

namespace mg::core::vehicle {

namespace {
void configure_vehicle_telemetry_streams(Vehicle& vehicle) {
    auto* mav = vehicle.mavlink();
    if (!mav || vehicle.system_id() == 0) return;

    const uint8_t sys = vehicle.system_id();
    const uint8_t comp = vehicle.info().component_id;

    mav->request_message_interval(sys, comp, 1, 1000000);
    mav->request_message_interval(sys, comp, 24, 500000);
    mav->request_message_interval(sys, comp, 26, 500000);
    mav->request_message_interval(sys, comp, 29, 500000);
    mav->request_message_interval(sys, comp, 30, 100000);
    mav->request_message_interval(sys, comp, 33, 500000);
    mav->request_message_interval(sys, comp, 36, 200000);
    mav->request_message_interval(sys, comp, 65, 200000);
    mav->request_message_interval(sys, comp, 74, 100000);
    mav->request_message_interval(sys, comp, 147, 500000);

    MGC_LOG_INFO("[Vehicle %d] Telemetry streams configured", vehicle.vehicle_id());
}
}

Vehicle::Vehicle(int vehicle_id)
    : id_(vehicle_id)
{
    info_.vehicle_id = vehicle_id;
    MGC_LOG_INFO("[Vehicle %d] Created", id_);
}

Vehicle::~Vehicle() {
    // 销毁前先让外部回调停止继续访问当前对象。
    if (alive_flag_) *alive_flag_ = false;
    disconnect();
    MGC_LOG_INFO("[Vehicle %d] Destroyed", id_);
}

bool Vehicle::connect(const std::string& endpoint) {
    auto pos = endpoint.find("://");
    std::string scheme = (pos != std::string::npos) ? endpoint.substr(0, pos) : "serial";
    std::string addr = (pos != std::string::npos) ? endpoint.substr(pos + 3) : endpoint;

    uint32_t baud = 115200;
    auto qmark = addr.find('?');
    if (qmark != std::string::npos) {
        std::string params = addr.substr(qmark + 1);
        addr = addr.substr(0, qmark);
        auto baud_pos = params.find("baud=");
        if (baud_pos != std::string::npos) {
            try { baud = static_cast<uint32_t>(std::stoi(params.substr(baud_pos + 5))); } catch (...) { /* keep default 115200 */ }
        }
    }

    info_.endpoint = (pos != std::string::npos) ? endpoint : "serial://" + endpoint;
    info_.transport_type = scheme;
    info_.port_name = addr;
    info_.baud_rate = baud;
    if (scheme == "serial" || scheme == "com" || scheme == "usb") {
        info_.serial_port = addr;
        info_.serial_baud = baud;
    }
    state_.store(VehicleState::Connecting);

    bool ok = false;
    if (scheme == "tcp") {
        ok = conn_.connect_tcp(addr);
    } else if (scheme == "udp") {
        ok = conn_.connect_udp(addr);
    } else {
        ok = conn_.connect(addr, baud);
    }

    if (!ok) {
        state_.store(VehicleState::Disconnected);
        return false;
    }

    init_subsystems();

    wire_telemetry_callbacks();
    conn_.set_state_callback([this](mg::core::transport::ConnectionState state, const char* reason) {
        switch (state) {
            case mg::core::transport::ConnectionState::Connected:
                fire_event(MG_EVENT_CONNECTION, R"({"connected":true})");
                break;
            case mg::core::transport::ConnectionState::HeartbeatLost:
            case mg::core::transport::ConnectionState::Disconnected: {
                mg::bridge::JsonBuilder jb;
                jb.begin_object();
                jb.field("connected", false);
                jb.field("reason", reason ? reason : "");
                jb.end_object();
                fire_event(MG_EVENT_CONNECTION, jb.str());
                break;
            }
            default: break;
        }
    });
    state_.store(VehicleState::WaitingHeartbeat);

    MGC_LOG_INFO("[Vehicle %d] Connected to %s", id_, info_.endpoint.c_str());
    return true;
}

void Vehicle::disconnect() {
    state_.store(VehicleState::Disconnecting);
    conn_.disconnect();
    info_.system_id = 0;
    info_.component_id = 0;
    info_.firmware_type.clear();
    info_.vehicle_type.clear();
    state_.store(VehicleState::Disconnected);
}

bool Vehicle::is_connected() const {
    return conn_.is_connected();
}

void Vehicle::set_identity(uint8_t sysid, uint8_t compid, const std::string& fw_type, const std::string& v_type) {
    info_.system_id = sysid;
    info_.component_id = compid;
    info_.firmware_type = fw_type;
    info_.vehicle_type = v_type;
    // 首次心跳识别，或重连后的身份刷新。
    auto st = state_.load();
    auto conn_st = conn_.state();
    if (st == VehicleState::WaitingHeartbeat ||
        st == VehicleState::Connected ||
        conn_st == mg::core::transport::ConnectionState::Connected) {
        state_.store(VehicleState::Connected);
        configure_vehicle_telemetry_streams(*this);
        MGC_LOG_INFO("[Vehicle %d] Identified: sysid=%d fw=%s type=%s", id_, sysid, fw_type.c_str(), v_type.c_str());
    }
}

void Vehicle::init_subsystems() {
    // 各子系统共享同一条连接对象，当前先把校准服务接上。
    calibration_.set_connection(&conn_);
    MGC_LOG_INFO("[Vehicle %d] Subsystems initialized", id_);
}

void Vehicle::load_sensor_decoder(const std::string& json_path) {
    if (!json_path.empty()) {
        sensor_decoder_ = mg::core::sensor::SensorIdDecoder(json_path);
        MGC_LOG_INFO("[Vehicle %d] Sensor decoder loaded from %s", id_, json_path.c_str());
    }
}

void Vehicle::fire_event(int event_type, const std::string& json) {
    EventCallback cb;
    {
        mg::core::platform::LockGuard lk(event_cb_mtx_);
        cb = event_cb_;  // 在锁内复制
    }
    if (cb) cb(event_type, json);  // 在锁外触发
}

void Vehicle::update_and_fire(mg::bridge::TelemetryType type, int event_id) {
    std::string json = cache_.update(type, conn_.get_mavlink());
    if (!json.empty()) fire_event(event_id, json);
}

void Vehicle::wire_telemetry_callbacks() {
    auto* mav = conn_.get_mavlink();
    if (!mav) return;

    std::weak_ptr<Vehicle> weak = shared_from_this();

    mav->on_heartbeat([weak](const mg::core::mavlink::HeartbeatInfo& info) {
        auto self = weak.lock();
        if (!self) return;
        self->flight_modes_.set_mavlink(self->conn_.get_mavlink(), info.system_id, info.component_id);
        self->flight_modes_.update_from_heartbeat(
            info.autopilot_type,
            info.vehicle_type,
            info.base_mode,
            info.custom_mode,
            info.armed,
            info.system_status);
        self->update_and_fire(mg::bridge::TelemetryType::kHeartbeat, MG_EVENT_HEARTBEAT);

        const uint64_t age = self->conn_.heartbeat_age_ms();
        if (self->info_.system_id > 0 && age > 5000) {
            self->needs_reidentify_.store(true);
        }

        self->conn_.record_heartbeat();

        bool expected = false;
        if (self->requested_firmware_version_.compare_exchange_strong(expected, true)) {
            auto* mav = self->conn_.get_mavlink();
            if (mav) {
                mav->request_autopilot_version(info.system_id, info.component_id);
            }
        }

        if (info.armed) {
            int64_t now = static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            if (self->last_armed_timestamp_ms_ > 0) {
                uint32_t elapsed = static_cast<uint32_t>(now - self->last_armed_timestamp_ms_);
                self->flight_time_s_.fetch_add(elapsed);
            }
            self->last_armed_timestamp_ms_ = now;
        } else {
            self->last_armed_timestamp_ms_ = 0;
        }

        if (self->info_.system_id == 0 || self->needs_reidentify_.exchange(false)) {
            mg::core::firmware::FirmwareAdapter adapter;
            adapter.detect(info.autopilot_type, info.vehicle_type);
            self->set_identity(
                info.system_id,
                info.component_id,
                adapter.firmware_name(),
                adapter.normalized_vehicle_name());
        }
    });

    // 这一组回调负责把 MAVLink 快照同步进缓存，并按需转成事件抛给上层。
    mav->on_sys_status([weak](const mg::core::mavlink::SysStatusInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kSysStatus, MG_EVENT_SYS_STATUS);
    });
    mav->on_gps([weak](const mg::core::mavlink::GpsInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kGps, MG_EVENT_GPS);
    });
    mav->on_attitude([weak](const mg::core::mavlink::AttitudeInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kAttitude, MG_EVENT_ATTITUDE);
    });
    mav->on_vfr_hud([weak](const mg::core::mavlink::VfrHudInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kVfrHud, MG_EVENT_TELEMETRY);
    });
    mav->on_firmware_version([weak](const mg::core::mavlink::FirmwareVersionInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kFirmwareVersion, MG_EVENT_FIRMWARE);
    });
    mav->on_global_position([weak](const mg::core::mavlink::GlobalPositionInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kGlobalPosition, MG_EVENT_GLOBAL_POSITION);
    });
    mav->on_battery([weak](const mg::core::mavlink::BatteryInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kBattery, MG_EVENT_BATTERY);
    });
    mav->on_extended_sys_state([weak](const mg::core::mavlink::ExtendedSysStateInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kExtendedSysState, MG_EVENT_EXTENDED_SYS_STATE);
    });
    mav->on_prearm_check([weak](const mg::core::mavlink::PrearmCheckInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kPrearmCheck, MG_EVENT_PREARM_CHECK);
    });
    mav->on_vibration([weak](const mg::core::mavlink::VibrationInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kVibration, MG_EVENT_VIBRATION);
    });
    mav->on_esc_status([weak](const mg::core::mavlink::EscStatusInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kEscStatus, MG_EVENT_ESC_STATUS);
    });
    mav->on_wind_estimate([weak](const mg::core::mavlink::WindEstimateInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(mg::bridge::TelemetryType::kWindEstimate, MG_EVENT_WIND_ESTIMATE);
    });
    mav->on_param([weak](const mg::core::mavlink::ParamInfo& param) {
        auto self = weak.lock(); if (!self) return;
        self->fire_event(MG_EVENT_PARAMETERS, mg::bridge::param_json(param));
    });
    mav->on_statustext([weak](const std::string& text, uint8_t severity) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update_statustext(text, severity);
        auto snap = self->cache_.snapshot_statustext();
        self->fire_event(MG_EVENT_STATUSTEXT, mg::bridge::statustext_json(snap.data));
        self->calibration_.process_statustext(text, severity);
    });
    mav->on_command_ack([weak](const mg::core::mavlink::CommandAckInfo& ack) {
        auto self = weak.lock(); if (!self) return;
        self->calibration_.process_command_ack(ack.command, ack.result, ack.progress);
    });

    mav->on_imu([weak](const mg::core::mavlink::ImuInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::bridge::TelemetryType::kImu, self->conn_.get_mavlink());
    });
    mav->on_pressure([weak](const mg::core::mavlink::PressureInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::bridge::TelemetryType::kPressure, self->conn_.get_mavlink());
    });
    mav->on_rc_channels([weak](const mg::core::mavlink::RcChannelsInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::bridge::TelemetryType::kRcChannels, self->conn_.get_mavlink());
    });
    mav->on_servo_output([weak](const mg::core::mavlink::ServoOutputInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::bridge::TelemetryType::kServoOutput, self->conn_.get_mavlink());
    });
    mav->on_home_position([weak](const mg::core::mavlink::HomePositionInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::bridge::TelemetryType::kHomePosition, self->conn_.get_mavlink());
    });
    mav->on_highres_imu([weak](const mg::core::mavlink::HighresImuInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::bridge::TelemetryType::kHighresImu, self->conn_.get_mavlink());
    });

    // 把视频流元数据继续转发给视频接收器。
    mav->on_video_stream_info([weak](const mg::core::mavlink::VideoStreamInfo& info) {
        auto self = weak.lock(); if (!self) return;
        mavlink_video_stream_information_t vsi{};
        vsi.stream_id = info.stream_id;
        vsi.type = info.type;
        vsi.resolution_h = info.width;
        vsi.resolution_v = info.height;
        vsi.framerate = info.framerate;
        vsi.bitrate = info.bitrate;
        vsi.rotation = info.rotation;
        vsi.hfov = info.hfov;
        vsi.flags = info.flags;
        memcpy(vsi.name, info.name, sizeof(info.name));
        memcpy(vsi.uri, info.uri, sizeof(info.uri));
        self->video().process_stream_info(0, 0, &vsi);
    });

    MGC_LOG_INFO("[Vehicle %d] Telemetry callbacks wired", id_);
}

} // namespace mg::core::vehicle
