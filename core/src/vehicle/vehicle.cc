#include "core/vehicle/vehicle.h"
#include "telemetry_bindings.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/param_json_serializers.h"
#include "core/firmware/firmware_adapter.h"
#include "core/log/logging.h"
#include <chrono>

namespace mg::core::vehicle {

namespace {
/// 单调时钟毫秒，波形缓冲只关心相对时间。
uint64_t steady_now_ms() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

constexpr double kRadToDeg = 57.29577951308232;
constexpr double kMradPerSecToDegPerSec = kRadToDeg / 1000.0;
/// SCALED_IMU 的加速度是毫重力，磁力计是毫高斯。
constexpr double kMilliGToMetersPerSec2 = 9.80665 / 1000.0;
constexpr double kMilliGaussToGauss = 1.0 / 1000.0;

void configure_vehicle_telemetry_streams(Vehicle& vehicle) {
    auto* mav = vehicle.mavlink();
    if (!mav || vehicle.system_id() == 0) return;

    const uint8_t sys = vehicle.system_id();
    const uint8_t comp = vehicle.info().component_id;

    // 速率单位：微秒间隔。前端按 20Hz 轮询遥测缓存，所以位置、姿态、VFR_HUD
    // 都请求 20Hz——位置流原来是 2Hz，飞机在地图上会一格一格跳。
    mav->request_message_interval(sys, comp, 1, 1000000);   // SYS_STATUS 1Hz
    mav->request_message_interval(sys, comp, 24, 200000);   // GPS_RAW_INT 5Hz
    mav->request_message_interval(sys, comp, 26, 500000);   // SCALED_IMU 2Hz
    mav->request_message_interval(sys, comp, 29, 500000);   // SCALED_PRESSURE 2Hz
    mav->request_message_interval(sys, comp, 30, 50000);    // ATTITUDE 20Hz
    mav->request_message_interval(sys, comp, 33, 50000);    // GLOBAL_POSITION_INT 20Hz
    mav->request_message_interval(sys, comp, 36, 50000);    // SERVO_OUTPUT_RAW 20Hz
    mav->request_message_interval(sys, comp, 65, 50000);    // RC_CHANNELS 20Hz
    mav->request_message_interval(sys, comp, 74, 50000);    // VFR_HUD 20Hz
    mav->request_message_interval(sys, comp, 147, 500000);  // BATTERY_STATUS 2Hz

    MGC_LOG_INFO("[Vehicle %d] Telemetry streams configured", vehicle.vehicle_id());
}
}

Vehicle::Vehicle(int vehicle_id)
    : id_(vehicle_id)
{
    info_.vehicle_id = vehicle_id;
    auto connection = std::make_shared<mg::core::transport::ConnectionManager>();
    connections_[vehicle_id] = connection;
    primary_route_id_ = vehicle_id;
    MGC_LOG_INFO("[Vehicle %d] Created", id_);
}

Vehicle::~Vehicle() {
    // 销毁前先让外部回调停止继续访问当前对象。
    if (alive_flag_) *alive_flag_ = false;
    disconnect();
    MGC_LOG_INFO("[Vehicle %d] Destroyed", id_);
}

void Vehicle::clear_waveforms() {
    // 断连/重连后旧数据没有意义，所有实例一起清空，
    // 活跃时间也归零，前端会退回"只有 IMU1"的初始状态。
    for (size_t instance = 0; instance < kMaxImuInstances; instance++) {
        waveforms_[instance].clear();
        instance_activity_[instance].imu_last_ms.store(0);
        instance_activity_[instance].mag_last_ms.store(0);
    }
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
    clear_waveforms();

    auto connection = connection_for_route(id_);
    if (!connection) {
        mg::core::platform::LockGuard lk(connections_mtx_);
        connection = std::make_shared<mg::core::transport::ConnectionManager>();
        connections_[id_] = connection;
        primary_route_id_ = id_;
    }
    bool ok = false;
    if (scheme == "tcp") {
        ok = connection->connect_tcp(addr);
    } else if (scheme == "udp") {
        ok = connection->connect_udp(addr);
    } else {
        ok = connection->connect(addr, baud);
    }

    if (!ok) {
        state_.store(VehicleState::Disconnected);
        return false;
    }
    link_manager_.add_link(id_);

    finish_connect();
    MGC_LOG_INFO("[Vehicle %d] Connected to %s", id_, info_.endpoint.c_str());
    return true;
}

bool Vehicle::attach_shared_session(
    const std::shared_ptr<mg::core::transport::LinkSession>& session,
    const std::string& endpoint,
    uint8_t system_id,
    uint8_t component_id) {
    return attach_shared_primary_route(
        session, id_, endpoint, system_id, component_id);
}

bool Vehicle::attach_shared_primary_route(
    const std::shared_ptr<mg::core::transport::LinkSession>& session,
    int route_id,
    const std::string& endpoint,
    uint8_t system_id,
    uint8_t component_id) {
    if (!session || route_id <= 0) return false;

    const auto scheme_pos = endpoint.find("://");
    const std::string scheme = scheme_pos != std::string::npos
        ? endpoint.substr(0, scheme_pos)
        : "serial";
    std::string addr = scheme_pos != std::string::npos
        ? endpoint.substr(scheme_pos + 3)
        : endpoint;

    uint32_t baud = 115200;
    const auto qmark = addr.find('?');
    if (qmark != std::string::npos) {
        const std::string params = addr.substr(qmark + 1);
        addr = addr.substr(0, qmark);
        const auto baud_pos = params.find("baud=");
        if (baud_pos != std::string::npos) {
            try {
                baud = static_cast<uint32_t>(std::stoi(params.substr(baud_pos + 5)));
            } catch (...) {
                // Keep the default when the URI contains a malformed baud.
            }
        }
    }

    info_.endpoint = scheme_pos != std::string::npos
        ? endpoint
        : "serial://" + endpoint;
    info_.transport_type = scheme;
    info_.port_name = addr;
    info_.baud_rate = baud;
    if (scheme == "serial" || scheme == "com" || scheme == "usb") {
        info_.serial_port = addr;
        info_.serial_baud = baud;
    }

    auto connection = connection_for_route(route_id);
    if (!connection) {
        connection = std::make_shared<mg::core::transport::ConnectionManager>();
    }
    state_.store(VehicleState::Connecting);
    clear_waveforms();
    if (!connection->attach_shared(session, route_id, system_id, component_id)) {
        state_.store(VehicleState::Disconnected);
        return false;
    }
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        connections_[route_id] = connection;
        primary_route_id_ = route_id;
    }
    link_manager_.add_link(route_id);
    install_connection_state_callback(route_id, connection);
    finish_connect();
    MGC_LOG_INFO("[Vehicle %d] Attached primary shared route %d on %s",
                 id_, route_id, info_.endpoint.c_str());
    return true;
}
bool Vehicle::attach_shared_route(
    const std::shared_ptr<mg::core::transport::LinkSession>& session,
    int route_id,
    uint8_t system_id,
    uint8_t component_id) {
    if (!session || route_id <= 0) return false;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        if (route_id == primary_route_id_ || connections_.count(route_id) != 0) {
            return false;
        }
    }

    auto connection = std::make_shared<mg::core::transport::ConnectionManager>();
    if (!connection->attach_shared(session, route_id, system_id, component_id)) {
        return false;
    }
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        if (connections_.count(route_id) != 0) return false;
        connections_[route_id] = connection;
    }
    link_manager_.add_link(route_id);
    install_connection_state_callback(route_id, connection);
    wire_telemetry_callbacks_for(connection.get(), route_id);
    MGC_LOG_INFO("[Vehicle %d] Attached secondary shared route %d",
                 id_, route_id);
    return true;
}

std::shared_ptr<mg::core::transport::ConnectionManager>
Vehicle::primary_connection() const {
    mg::core::platform::LockGuard lk(connections_mtx_);
    const auto it = connections_.find(primary_route_id_);
    return it == connections_.end() ? nullptr : it->second;
}

std::shared_ptr<mg::core::transport::ConnectionManager>
Vehicle::connection_for_route(int route_id) const {
    mg::core::platform::LockGuard lk(connections_mtx_);
    const auto it = connections_.find(route_id);
    return it == connections_.end() ? nullptr : it->second;
}

void Vehicle::install_connection_state_callback(
    int route_id,
    const std::shared_ptr<mg::core::transport::ConnectionManager>& connection) {
    (void)route_id;
    if (!connection) return;
    connection->set_state_callback([this](mg::core::transport::ConnectionState state,
                                          const char* reason) {
        flight_log_.on_connection_state(
            mg::core::transport::ConnectionManager::state_string(state),
            reason ? reason : "");
        switch (state) {
            case mg::core::transport::ConnectionState::Connected:
                fire_event(MG_EVENT_CONNECTION, R"({"connected":true})");
                break;
            case mg::core::transport::ConnectionState::HeartbeatLost:
            case mg::core::transport::ConnectionState::Disconnected: {
                mg::core::telemetry::JsonBuilder jb;
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
}

void Vehicle::refresh_primary_connection() {
    const int route_id = link_manager_.primary_route_id();
    auto connection = connection_for_route(route_id);
    if (!connection) {
        // Keep the current primary when no health result is available yet.
        return;
    }
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        primary_route_id_ = route_id;
    }
    calibration_.set_connection(connection.get());
    if (auto* mav = connection->get_mavlink()) {
        flight_modes_.set_mavlink(mav, info_.system_id, info_.component_id);
    }
}

bool Vehicle::remove_shared_route(int route_id) {
    std::shared_ptr<mg::core::transport::ConnectionManager> connection;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        const auto it = connections_.find(route_id);
        if (it == connections_.end()) return false;
        connection = it->second;
        connections_.erase(it);
        if (primary_route_id_ == route_id) primary_route_id_ = -1;
    }
    if (connection) connection->disconnect();
    link_manager_.remove_link(route_id);
    refresh_primary_connection();
    if (!is_connected()) state_.store(VehicleState::Disconnected);
    return true;
}

void Vehicle::finish_connect() {
    if (state_.load() != VehicleState::Connecting) return;

    init_subsystems();

    wire_telemetry_callbacks();
    install_connection_state_callback(primary_route_id_, primary_connection());
    state_.store(VehicleState::WaitingHeartbeat);
}

void Vehicle::disconnect() {
    state_.store(VehicleState::Disconnecting);
    std::map<int, std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        connections.swap(connections_);
        primary_route_id_ = -1;
    }
    for (const auto& [route_id, connection] : connections) {
        if (connection) connection->disconnect();
    }
    link_manager_.clear();
    clear_waveforms();
    info_.system_id = 0;
    info_.component_id = 0;
    info_.firmware_type.clear();
    info_.vehicle_type.clear();
    state_.store(VehicleState::Disconnected);
}

size_t Vehicle::connection_count() const {
    mg::core::platform::LockGuard lk(connections_mtx_);
    return connections_.size();
}

bool Vehicle::set_signing_key(const uint8_t* key, size_t key_size,
                              bool sign_outgoing,
                              uint64_t persisted_timestamp) {
    if (key == nullptr || key_size < 32) return false;
    std::vector<std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        for (const auto& [route_id, connection] : connections_) {
            (void)route_id;
            if (connection) connections.push_back(connection);
        }
    }
    bool any = false;
    bool all = true;
    for (const auto& connection : connections) {
        auto* mav = connection ? connection->get_mavlink() : nullptr;
        if (!mav) continue;
        any = true;
        all = mav->set_signing_key(key, key_size, sign_outgoing,
                                   persisted_timestamp) && all;
    }
    return any && all;
}

void Vehicle::clear_signing_key() {
    std::vector<std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        for (const auto& [route_id, connection] : connections_) {
            (void)route_id;
            if (connection) connections.push_back(connection);
        }
    }
    for (const auto& connection : connections) {
        if (auto* mav = connection ? connection->get_mavlink() : nullptr) {
            mav->clear_signing_key();
        }
    }
}

bool Vehicle::signing_enabled() const {
    const auto* mav = mavlink();
    return mav != nullptr && mav->signing_enabled();
}

int Vehicle::signing_status() const {
    const auto* mav = mavlink();
    return mav ? static_cast<int>(mav->signing_status()) : 0;
}

bool Vehicle::send_setup_signing(uint8_t target_system,
                                 uint8_t target_component,
                                 const uint8_t* key,
                                 size_t key_size,
                                 uint64_t initial_timestamp) {
    std::vector<std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        for (const auto& [route_id, connection] : connections_) {
            (void)route_id;
            if (connection) connections.push_back(connection);
        }
    }
    bool any = false;
    for (const auto& connection : connections) {
        auto* mav = connection ? connection->get_mavlink() : nullptr;
        if (!mav) continue;
        any = mav->send_setup_signing(target_system, target_component, key,
                                      key_size, initial_timestamp) || any;
    }
    return any;
}

bool Vehicle::is_connected() const {
    std::map<int, std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        connections = connections_;
    }
    for (const auto& [route_id, connection] : connections) {
        if (connection && connection->is_connected()) return true;
    }
    return false;
}

void Vehicle::set_identity(uint8_t sysid, uint8_t compid, const std::string& fw_type, const std::string& v_type) {
    info_.system_id = sysid;
    info_.component_id = compid;
    info_.firmware_type = fw_type;
    info_.vehicle_type = v_type;
    // 首次心跳识别，或重连后的身份刷新。
    auto st = state_.load();
    bool any_connected = false;
    std::map<int, std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        connections = connections_;
    }
    for (const auto& [route_id, connection] : connections) {
        any_connected = any_connected ||
            (connection && connection->state() ==
                mg::core::transport::ConnectionState::Connected);
    }
    if (st == VehicleState::WaitingHeartbeat ||
        st == VehicleState::Connected || any_connected) {
        state_.store(VehicleState::Connected);
        configure_vehicle_telemetry_streams(*this);
        MGC_LOG_INFO("[Vehicle %d] Identified: sysid=%d fw=%s type=%s", id_, sysid, fw_type.c_str(), v_type.c_str());
    }
}

void Vehicle::init_subsystems() {
    // 各子系统共享同一条连接对象，当前先把校准服务接上。
    const auto primary = primary_connection();
    calibration_.set_connection(primary ? primary.get() : nullptr);
    MGC_LOG_INFO("[Vehicle %d] Subsystems initialized", id_);
}

void Vehicle::load_sensor_decoder(const std::string& json_path) {
    if (!json_path.empty()) {
        sensor_decoder_ = mg::core::sensor::SensorIdDecoder(json_path);
        if (sensor_decoder_.is_loaded()) {
            MGC_LOG_INFO("[Vehicle %d] Sensor decoder loaded from %s", id_, json_path.c_str());
        } else {
            MGC_LOG_WARN("[Vehicle %d] Sensor decoder failed to load %s", id_, json_path.c_str());
        }
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

void Vehicle::update_and_fire(
    mg::core::transport::ConnectionManager* source,
    mg::core::telemetry::TelemetryType type,
    int event_id) {
    if (!source) return;
    std::string json = cache_.update(type, source->get_mavlink());
    if (!json.empty()) fire_event(event_id, json);
}

void Vehicle::wire_telemetry_callbacks() {
    std::map<int, std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        connections = connections_;
    }
    for (const auto& [route_id, connection] : connections) {
        wire_telemetry_callbacks_for(connection.get(), route_id);
    }
}

void Vehicle::wire_telemetry_callbacks_for(
    mg::core::transport::ConnectionManager* source,
    int route_id) {
    auto* mav = source ? source->get_mavlink() : nullptr;
    if (!mav) return;

    std::weak_ptr<Vehicle> weak = shared_from_this();

    mav->on_heartbeat([weak, source, route_id](const mg::core::mavlink::HeartbeatInfo& info) {
        auto self = weak.lock();
        if (!self) return;
        self->flight_modes_.set_mavlink(source->get_mavlink(), info.system_id, info.component_id);
        self->flight_modes_.update_from_heartbeat(
            info.autopilot_type,
            info.vehicle_type,
            info.base_mode,
            info.custom_mode,
            info.armed,
            info.system_status);
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kHeartbeat, MG_EVENT_HEARTBEAT);

        const uint64_t age = source->heartbeat_age_ms();
        if (self->info_.system_id > 0 && age > 5000) {
            self->needs_reidentify_.store(true);
        }

        source->record_heartbeat();
        self->link_manager_.record_heartbeat(route_id);
        self->refresh_primary_connection();

        bool expected = false;
        if (self->requested_firmware_version_.compare_exchange_strong(expected, true)) {
            auto* mav = source->get_mavlink();
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
            // ArduPilot 只在首次上电和每次解锁时发一次 HOME_POSITION，GCS 连晚了
            // 就收不到，地图上的 home 会一直空着（QGC 的 APMFirmwarePlugin 有同样
            // 的注释与做法）。识别出系统号之后再要，避免猜 target。
            if (auto* mav = source->get_mavlink()) {
                mav->request_home_position(info.system_id, info.component_id);
                mav->request_message_interval(
                    info.system_id, info.component_id,
                    MAVLINK_MSG_ID_HOME_POSITION,
                    1000000 /* 1 Hz */);
                // ArduPilot 同样不主动发 EXTENDED_SYS_STATE（QGC 的注释：
                // “ArduPilot doesn't send MAVLINK_MSG_ID_EXTENDED_SYS_STATE messages
                // unless requested”）。引导操作要靠它的 landed_state 判断是否在飞行，
                // 所以要主动请一下。
                mav->request_message_interval(
                    info.system_id, info.component_id,
                    MAVLINK_MSG_ID_EXTENDED_SYS_STATE,
                    1000000 /* 1 Hz */);
            }
        }
    });

    // 这一组回调负责把 MAVLink 快照同步进缓存，并按需转成事件抛给上层。
    mav->on_sys_status([weak, source](const mg::core::mavlink::SysStatusInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kSysStatus, MG_EVENT_SYS_STATUS);
    });
    mav->on_gps([weak, source](const mg::core::mavlink::GpsInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kGps, MG_EVENT_GPS);
    });
    mav->on_attitude([weak, source](const mg::core::mavlink::AttitudeInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kAttitude, MG_EVENT_ATTITUDE);
    });
    mav->on_vfr_hud([weak, source](const mg::core::mavlink::VfrHudInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kVfrHud, MG_EVENT_TELEMETRY);
    });
    mav->on_firmware_version([weak, source](const mg::core::mavlink::FirmwareVersionInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kFirmwareVersion, MG_EVENT_FIRMWARE);
    });
    mav->on_global_position([weak, source](const mg::core::mavlink::GlobalPositionInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kGlobalPosition, MG_EVENT_GLOBAL_POSITION);
    });
    mav->on_battery([weak, source](const mg::core::mavlink::BatteryInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kBattery, MG_EVENT_BATTERY);
    });
    mav->on_extended_sys_state([weak, source](const mg::core::mavlink::ExtendedSysStateInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kExtendedSysState, MG_EVENT_EXTENDED_SYS_STATE);
    });
    mav->on_prearm_check([weak, source](const mg::core::mavlink::PrearmCheckInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kPrearmCheck, MG_EVENT_PREARM_CHECK);
    });
    mav->on_vibration([weak, source](const mg::core::mavlink::VibrationInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kVibration, MG_EVENT_VIBRATION);
    });
    mav->on_esc_status([weak, source](const mg::core::mavlink::EscStatusInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kEscStatus, MG_EVENT_ESC_STATUS);
    });
    mav->on_wind_estimate([weak, source](const mg::core::mavlink::WindEstimateInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->update_and_fire(source, mg::core::telemetry::TelemetryType::kWindEstimate, MG_EVENT_WIND_ESTIMATE);
    });
    mav->on_param([weak](const mg::core::mavlink::ParamInfo& param) {
        auto self = weak.lock(); if (!self) return;
        self->fire_event(MG_EVENT_PARAMETERS, mg::core::telemetry::param_json(param));
    });
    mav->on_statustext([weak](const std::string& text, uint8_t severity) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update_statustext(text, severity);
        // 消息框要历史，缓存只留最新一条。
        self->message_log_.add(text, severity);
        // 同理：飞行日志声明采集飞控 STATUSTEXT，但之前没有接线。
        self->flight_log_.on_statustext(severity, text);
        auto snap = self->cache_.snapshot_statustext();
        self->fire_event(MG_EVENT_STATUSTEXT, mg::core::telemetry::statustext_json(snap.data));
        self->calibration_.process_statustext(text, severity);
    });
    mav->on_command_ack([weak](const mg::core::mavlink::CommandAckInfo& ack) {
        auto self = weak.lock(); if (!self) return;
        self->calibration_.process_command_ack(ack.command, ack.result, ack.progress);
    });
    mav->on_mag_cal_progress([weak](const mg::core::mavlink::MagCalibrationProgressInfo& progress) {
        auto self = weak.lock(); if (!self) return;
        self->calibration_.process_mag_cal_progress(progress);
    });
    mav->on_mag_cal_report([weak](const mg::core::mavlink::MagCalibrationReportInfo& report) {
        auto self = weak.lock(); if (!self) return;
        self->calibration_.process_mag_cal_report(report);
    });
    mav->on_command_long([weak](const mg::core::mavlink::CommandLongInfo& command) {
        auto self = weak.lock(); if (!self) return;
        if (command.command == 42429) {  // MAV_CMD_ACCELCAL_VEHICLE_POS
            self->calibration_.process_accelerometer_position_request(
                static_cast<uint32_t>(command.param1));
        }
    });

    // MG_EVENT_WAYPOINT_REACHED 之前只在公开枚举里声明，没有任何地方发出，
    // 订阅该事件的客户端会一直收不到消息。这里把适配器的 reached 回调接上。
    mav->on_mission_reached([weak](uint16_t seq) {
        auto self = weak.lock(); if (!self) return;
        mg::core::telemetry::JsonBuilder jb;
        jb.begin_object();
        jb.field("seq", static_cast<int>(seq));
        jb.end_object();
        self->fire_event(MG_EVENT_WAYPOINT_REACHED, jb.str());
    });

    mav->on_imu([weak, source](const mg::core::mavlink::ImuInfo& info) {
        auto self = weak.lock(); if (!self) return;
        // SCALED_IMU/2/3 一次带回加速度、角速度和磁力，刚好凑成同一帧；
        // 每个 IMU 实例写进自己的缓冲，前端按实例号取数。
        auto* buffer = self->waveform(info.instance);
        if (!buffer) return;
        const uint64_t now_ms = steady_now_ms();
        buffer->set_accel(static_cast<float>(info.xacc * kMilliGToMetersPerSec2),
                          static_cast<float>(info.yacc * kMilliGToMetersPerSec2),
                          static_cast<float>(info.zacc * kMilliGToMetersPerSec2));
        buffer->set_gyro(static_cast<float>(info.xgyro * kMradPerSecToDegPerSec),
                         static_cast<float>(info.ygyro * kMradPerSecToDegPerSec),
                         static_cast<float>(info.zgyro * kMradPerSecToDegPerSec));
        buffer->set_mag(static_cast<float>(info.xmag * kMilliGaussToGauss),
                        static_cast<float>(info.ymag * kMilliGaussToGauss),
                        static_cast<float>(info.zmag * kMilliGaussToGauss));
        buffer->commit(now_ms);
        self->instance_activity_[info.instance].imu_last_ms.store(now_ms);
        // 没有对应罗盘实例时这组值是 0，用非零读数区分"有磁力计"。
        if (info.xmag != 0 || info.ymag != 0 || info.zmag != 0) {
            self->instance_activity_[info.instance].mag_last_ms.store(now_ms);
        }
        // 主 IMU 才刷新遥测缓存，其余实例只走波形缓冲。
        if (info.instance == 0) {
            self->cache_.update(mg::core::telemetry::TelemetryType::kImu, source->get_mavlink());
        }
    });
    mav->on_pressure([weak, source](const mg::core::mavlink::PressureInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::core::telemetry::TelemetryType::kPressure, source->get_mavlink());
    });
    mav->on_rc_channels([weak, source](const mg::core::mavlink::RcChannelsInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::core::telemetry::TelemetryType::kRcChannels, source->get_mavlink());
    });
    mav->on_servo_output([weak, source](const mg::core::mavlink::ServoOutputInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::core::telemetry::TelemetryType::kServoOutput, source->get_mavlink());
    });
    mav->on_home_position([weak, source](const mg::core::mavlink::HomePositionInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::core::telemetry::TelemetryType::kHomePosition, source->get_mavlink());
    });
    mav->on_highres_imu([weak, source](const mg::core::mavlink::HighresImuInfo&) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update(mg::core::telemetry::TelemetryType::kHighresImu, source->get_mavlink());
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
