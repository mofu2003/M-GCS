#include "core/vehicle/vehicle.h"
#include "core/event/event_codes.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/param_json_serializers.h"
#include "core/firmware/firmware_adapter.h"
#include "core/log/logging.h"
#include <chrono>
#include <algorithm>

namespace mg::core::vehicle {
namespace {

/// 传输层状态 → 车辆状态（KD-15）。
///
/// 两个枚举一一对应，只有 `VehicleState::Initializing` 是车辆层独有（子系统初始化
/// 期间），没有传输层对应值，返回 nullopt 表示"不要改写车辆状态"。
std::optional<VehicleState> vehicle_state_for(
    mg::core::transport::ConnectionState state) {
    switch (state) {
        case mg::core::transport::ConnectionState::Disconnected:
            return VehicleState::Disconnected;
        case mg::core::transport::ConnectionState::Connecting:
            return VehicleState::Connecting;
        case mg::core::transport::ConnectionState::WaitingHeartbeat:
            return VehicleState::WaitingHeartbeat;
        case mg::core::transport::ConnectionState::Connected:
            return VehicleState::Connected;
        case mg::core::transport::ConnectionState::HeartbeatLost:
            return VehicleState::HeartbeatLost;
        case mg::core::transport::ConnectionState::Disconnecting:
            return VehicleState::Disconnecting;
    }
    return std::nullopt;
}

}  // namespace


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
    const auto mav = vehicle.shared_mavlink();
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
    mav->request_message_interval(sys, comp, 42, 1000000);  // MISSION_CURRENT 1Hz
    mav->request_message_interval(sys, comp, 65, 50000);    // RC_CHANNELS 20Hz
    mav->request_message_interval(sys, comp, 74, 50000);    // VFR_HUD 20Hz
    mav->request_message_interval(sys, comp, 147, 500000);  // BATTERY_STATUS 2Hz

    // EXTENDED_SYS_STATE 1Hz。ArduPilot 不主动发这条，必须请求：
    // QGC 在 APMFirmwarePlugin.cc:441-443 同位置请求，注释写明
    // "ArduPilot doesn't send MAVLINK_MSG_ID_EXTENDED_SYS_STATE messages
    // unless requested"。它是 vtol_state（MAV_VTOL_STATE）与 landed_state
    // 的唯一来源；不请求则 vtol_state 恒为 0，VTOL 顶栏只能退化成
    // "Guided · VTOL" 占位，四态（多旋翼/固定翼/转固定翼/转多旋翼）永不出现。
    // 频率对齐 QGC 取 1Hz：转态是秒级过程，无需更高。
    mav->request_message_interval(sys, comp, 245, 1000000);  // EXTENDED_SYS_STATE 1Hz

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
        mg::core::platform::LockGuard policy_lock(signing_policy_mtx_);
        if (auto mav = connection->shared_mavlink()) inherit_signing(*mav);
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
        // Serialize registration with policy changes; no transport I/O here.
        mg::core::platform::LockGuard policy_lock(signing_policy_mtx_);
        if (auto mav = connection->shared_mavlink()) inherit_signing(*mav);
        mg::core::platform::LockGuard lk(connections_mtx_);
        if (connections_.count(route_id) != 0) return false;
        connections_[route_id] = connection;
    }
    link_manager_.add_link(route_id);
    install_connection_state_callback(route_id, connection);
    wire_telemetry_callbacks_for(connection, route_id);
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
        // KD-15：车辆状态与传输层状态同源，断链/心跳丢失后不会再漂移。
        if (const auto mirrored = vehicle_state_for(state)) {
            state_.store(*mirrored);
        }
        flight_log_.on_connection_state(
            mg::core::transport::ConnectionManager::state_string(state),
            reason ? reason : "");
        switch (state) {
            case mg::core::transport::ConnectionState::Connected:
                fire_event(mg::core::event::kConnection, R"({"connected":true})");
                break;
            case mg::core::transport::ConnectionState::HeartbeatLost:
            case mg::core::transport::ConnectionState::Disconnected: {
                mg::core::telemetry::JsonBuilder jb;
                jb.begin_object();
                jb.field("connected", is_connected());
                jb.field("reason", reason ? reason : "");
                jb.end_object();
                fire_event(mg::core::event::kConnection, jb.str());
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
    for (const auto* id : {"params", "mission", "fence", "rally", "log"})
        (void)operation_mavlink(id);
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        primary_route_id_ = route_id;
    }
    calibration_.set_connection(connection.get());
}

bool Vehicle::remove_shared_route(int route_id) {
    for (const auto* id : {"params", "mission", "fence", "rally", "log"})
        (void)operation_mavlink(id);
    std::shared_ptr<mg::core::transport::ConnectionManager> connection;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        const auto it = connections_.find(route_id);
        if (it == connections_.end()) return false;
        connection = it->second;
        connections_.erase(it);
        if (primary_route_id_ == route_id) primary_route_id_ = -1;
    }
    if (connection) {
        if (const auto mav = connection->shared_mavlink()) {
            mav->cancel_param_download(); mav->mission_cancel();
            mav->fence_cancel(); mav->rally_cancel(); mav->log_download_cancel();
        }
        connection->disconnect();
    }
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

void Vehicle::inherit_signing(mg::core::mavlink::MavlinkAdapter& mav) {
    // Called under signing_policy_mtx_, before a route is made visible.
    mav.set_signing_candidates(signing_candidates_);
    if (signing_policy_) {
        const auto& policy = *signing_policy_;
        mav.set_signing_key(policy.key.data(), policy.key.size(), signing_policy_outgoing_,
                            policy.persisted_timestamp, policy.name, signing_policy_detected_);
    }
}

std::shared_ptr<mg::core::transport::ConnectionManager> Vehicle::command_connection() const {
    return primary_connection();
}

bool Vehicle::all_routes_disconnected() const {
    mg::core::platform::LockGuard lk(connections_mtx_);
    for (const auto& [id, connection] : connections_) {
        (void)id;
        if (connection && connection->state() !=
                mg::core::transport::ConnectionState::Disconnected) return false;
    }
    return true;
}

namespace {
bool operation_active(mg::core::mavlink::MavlinkAdapter& mav, const std::string& id) {
    if (id == "params") return mav.param_download_active();
    if (id == "log") return mav.log_download_status().active;
    const auto progress = id == "mission" ? mav.mission_get_progress()
        : id == "fence" ? mav.fence_get_progress() : mav.rally_get_progress();
    return progress.state != mg::core::mission::MissionState::Idle;
}
bool cancel_adapter_operation(mg::core::mavlink::MavlinkAdapter& mav, const std::string& id) {
    if (id == "params") return mav.cancel_param_download();
    if (id == "mission") return mav.mission_cancel();
    if (id == "fence") return mav.fence_cancel();
    if (id == "rally") return mav.rally_cancel();
    if (id == "log") return mav.log_download_cancel();
    return false;
}
}

std::shared_ptr<mg::core::mavlink::MavlinkAdapter> Vehicle::operation_mavlink(const std::string& id) {
    std::vector<std::shared_ptr<mg::core::transport::ConnectionManager>> routes;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        for (const auto& [route_id, connection] : connections_) {
            (void)route_id;
            if (connection) routes.push_back(connection);
        }
    }
    for (const auto& connection : routes) {
        auto mav = connection->shared_mavlink();
        if (mav && operation_active(*mav, id)) {
            mg::core::platform::LockGuard lk(operation_mtx_);
            operation_adapters_[id] = mav;
            return mav;
        }
    }
    mg::core::platform::LockGuard lk(operation_mtx_);
    const auto it = operation_adapters_.find(id);
    if (it != operation_adapters_.end()) return it->second;
    const auto primary = primary_connection();
    return primary ? primary->shared_mavlink() : nullptr;
}

bool Vehicle::cancel_operation(const std::string& id) {
    const auto owner = operation_mavlink(id);
    bool cancelled = owner && cancel_adapter_operation(*owner, id);
    std::vector<std::shared_ptr<mg::core::transport::ConnectionManager>> routes;
    {
        mg::core::platform::LockGuard lk(connections_mtx_);
        for (const auto& [route_id, connection] : connections_) {
            (void)route_id;
            if (connection) routes.push_back(connection);
        }
    }
    // Defensive cleanup if legacy callers started transactions on two routes.
    for (const auto& route : routes) {
        const auto mav = route->shared_mavlink();
        if (mav && mav != owner) cancelled = cancel_adapter_operation(*mav, id) || cancelled;
    }
    return cancelled;
}

bool Vehicle::set_signing_key(const uint8_t* key, size_t key_size,
                              bool sign_outgoing,
                              uint64_t persisted_timestamp,
                              const std::string& key_name,
                              bool auto_detected) {
    if (key == nullptr || key_size < 32) return false;
    mg::core::platform::LockGuard policy_lock(signing_policy_mtx_);
    mg::core::mavlink::SigningCandidate policy;
    std::copy_n(key, policy.key.size(), policy.key.begin());
    policy.name = key_name;
    policy.persisted_timestamp = persisted_timestamp;
    signing_policy_ = policy;
    signing_policy_outgoing_ = sign_outgoing;
    signing_policy_detected_ = auto_detected;
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
        const auto mav = connection ? connection->shared_mavlink() : nullptr;
        if (!mav) continue;
        any = true;
        if (auto_detected && mav->signing_enabled() &&
            mav->signing_auto_detected() && mav->signing_key_name() == key_name) continue;
        all = mav->set_signing_key(key, key_size, sign_outgoing,
                                   persisted_timestamp, key_name,
                                   auto_detected) && all;
    }
    return any && all;
}

bool Vehicle::set_signing_candidates(
    const std::vector<mg::core::mavlink::SigningCandidate>& candidates) {
    mg::core::platform::LockGuard policy_lock(signing_policy_mtx_);
    signing_candidates_ = candidates;
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
        const auto mav = connection ? connection->shared_mavlink() : nullptr;
        if (!mav) continue;
        any = true;
        all = mav->set_signing_candidates(candidates) && all;
    }
    return any && all;
}

void Vehicle::clear_signing_candidates() {
    mg::core::platform::LockGuard policy_lock(signing_policy_mtx_);
    signing_candidates_.clear();
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
        if (const auto mav = connection ? connection->shared_mavlink() : nullptr) {
            mav->clear_signing_candidates();
        }
    }
}

void Vehicle::clear_signing_key() {
    mg::core::platform::LockGuard policy_lock(signing_policy_mtx_);
    signing_policy_.reset();
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
        if (const auto mav = connection ? connection->shared_mavlink() : nullptr) {
            mav->clear_signing_key();
        }
    }
}

bool Vehicle::signing_enabled() const {
    const auto mav = shared_mavlink();
    return mav != nullptr && mav->signing_enabled();
}

int Vehicle::signing_status() const {
    const auto mav = shared_mavlink();
    return mav ? static_cast<int>(mav->signing_status()) : 0;
}

std::string Vehicle::signing_key_name() const {
    const auto mav = shared_mavlink();
    return mav ? mav->signing_key_name() : std::string();
}

bool Vehicle::signing_auto_detected() const {
    const auto mav = shared_mavlink();
    return mav != nullptr && mav->signing_auto_detected();
}

size_t Vehicle::signing_candidate_count() const {
    const auto mav = shared_mavlink();
    return mav ? mav->signing_candidate_count() : 0;
}

uint64_t Vehicle::signing_timestamp() const {
    const auto mav = shared_mavlink();
    return mav ? mav->signing_timestamp() : 0;
}

void Vehicle::apply_detected_signing_candidate(
    const mg::core::mavlink::SigningCandidate& candidate) {
    set_signing_key(candidate.key.data(), candidate.key.size(), true,
                    candidate.persisted_timestamp, candidate.name, true);
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
        const auto mav = connection ? connection->shared_mavlink() : nullptr;
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
    const auto mav = source->shared_mavlink();
    std::string json = cache_.update(type, mav.get());
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
        wire_telemetry_callbacks_for(connection, route_id);
    }
}

void Vehicle::wire_telemetry_callbacks_for(
    const std::shared_ptr<mg::core::transport::ConnectionManager>& source,
    int route_id) {
    if (!source) return;
    const auto mav = source->shared_mavlink();
    if (!mav) return;

    // 把链路适配器绑到本车自己的 .tlog 写入器（ADR-0006）。绑定是幂等的，
    // 写入器只在 mg_tlog_start 后真正落盘。
    mav->set_log_sink(&mavlink_log_);

    std::weak_ptr<Vehicle> weak = shared_from_this();
    // 回调只持弱引用：共享链路下 remove_shared_route() 会释放本 route 的
    // ConnectionManager，而适配器仍可能被租约持有并继续派发回调。
    std::weak_ptr<mg::core::transport::ConnectionManager> weak_source = source;
    mav->set_signing_detected_callback(
        [weak](const mg::core::mavlink::SigningCandidate& candidate) {
            if (auto self = weak.lock()) {
                self->apply_detected_signing_candidate(candidate);
            }
        });

    mav->on_heartbeat([weak, weak_source, route_id](const mg::core::mavlink::HeartbeatInfo& info) {
        auto self = weak.lock();
        if (!self) return;
        auto src = weak_source.lock();
        if (!src) return;
        self->flight_modes_.update_from_heartbeat(
            info.autopilot_type,
            info.vehicle_type,
            info.base_mode,
            info.custom_mode,
            info.armed,
            info.system_status);
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kHeartbeat, mg::core::event::kHeartbeat);

        const uint64_t age = src->heartbeat_age_ms();
        if (self->info_.system_id > 0 && age > 5000) {
            self->needs_reidentify_.store(true);
        }

        src->record_heartbeat();
        self->link_manager_.record_heartbeat(route_id);
        self->refresh_primary_connection();

        bool expected = false;
        if (self->requested_firmware_version_.compare_exchange_strong(expected, true)) {
            const auto mav = src->shared_mavlink();
            if (mav) {
                mav->request_autopilot_version(info.system_id, info.component_id);
            }
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
            if (const auto mav = src->shared_mavlink()) {
                self->last_home_request_ms_.store(steady_now_ms());
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
        // Like QGC's ArduPilot plugin, recover missing Home streaming per vehicle.
        const auto now = steady_now_ms();
        auto last_request = self->last_home_request_ms_.load();
        if (self->cache_.age_ms(mg::core::telemetry::TelemetryType::kHomePosition) > 10000 &&
            now - last_request >= 10000 &&
            self->last_home_request_ms_.compare_exchange_strong(last_request, now)) {
            if (const auto mav = src->shared_mavlink()) {
                mav->request_home_position(info.system_id, info.component_id);
                mav->request_message_interval(info.system_id, info.component_id,
                                              MAVLINK_MSG_ID_HOME_POSITION, 1000000);
            }
        }
    });

    // 这一组回调负责把 MAVLink 快照同步进缓存，并按需转成事件抛给上层。
    mav->on_sys_status([weak, weak_source](const mg::core::mavlink::SysStatusInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kSysStatus, mg::core::event::kSysStatus);
    });
    mav->on_gps([weak, weak_source](const mg::core::mavlink::GpsInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kGps, mg::core::event::kGps);
    });
    mav->on_attitude([weak, weak_source](const mg::core::mavlink::AttitudeInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kAttitude, mg::core::event::kAttitude);
    });
    mav->on_vfr_hud([weak, weak_source](const mg::core::mavlink::VfrHudInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kVfrHud, mg::core::event::kTelemetry);
    });
    mav->on_firmware_version([weak, weak_source](const mg::core::mavlink::FirmwareVersionInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kFirmwareVersion, mg::core::event::kFirmware);
    });
    mav->on_global_position([weak, weak_source](const mg::core::mavlink::GlobalPositionInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kGlobalPosition, mg::core::event::kGlobalPosition);
    });
    mav->on_battery([weak, weak_source](const mg::core::mavlink::BatteryInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kBattery, mg::core::event::kBattery);
    });
    mav->on_extended_sys_state([weak, weak_source](const mg::core::mavlink::ExtendedSysStateInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kExtendedSysState, mg::core::event::kExtendedSysState);
    });
    mav->on_prearm_check([weak, weak_source](const mg::core::mavlink::PrearmCheckInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kPrearmCheck, mg::core::event::kPrearmCheck);
    });
    mav->on_vibration([weak, weak_source](const mg::core::mavlink::VibrationInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kVibration, mg::core::event::kVibration);
    });
    mav->on_esc_status([weak, weak_source](const mg::core::mavlink::EscStatusInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kEscStatus, mg::core::event::kEscStatus);
    });
    mav->on_wind_estimate([weak, weak_source](const mg::core::mavlink::WindEstimateInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        self->update_and_fire(src.get(), mg::core::telemetry::TelemetryType::kWindEstimate, mg::core::event::kWindEstimate);
    });
    mav->on_param([weak](const mg::core::mavlink::ParamInfo& param) {
        auto self = weak.lock(); if (!self) return;
        self->fire_event(mg::core::event::kParameters, mg::core::telemetry::param_json(param));
    });
    mav->on_statustext([weak](const std::string& text, uint8_t severity) {
        auto self = weak.lock(); if (!self) return;
        self->cache_.update_statustext(text, severity);
        // 消息框要历史，缓存只留最新一条。
        self->message_log_.add(text, severity);
        // 同理：飞行日志声明采集飞控 STATUSTEXT，但之前没有接线。
        self->flight_log_.on_statustext(severity, text);
        auto snap = self->cache_.snapshot_statustext();
        self->fire_event(mg::core::event::kStatustext, mg::core::telemetry::statustext_json(snap.data));
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

    // mg::core::event::kWaypointReached 之前只在公开枚举里声明，没有任何地方发出，
    // 订阅该事件的客户端会一直收不到消息。这里把适配器的 reached 回调接上。
    mav->on_mission_reached([weak](uint16_t seq) {
        auto self = weak.lock(); if (!self) return;
        mg::core::telemetry::JsonBuilder jb;
        jb.begin_object();
        jb.field("seq", static_cast<int>(seq));
        jb.end_object();
        self->fire_event(mg::core::event::kWaypointReached, jb.str());
    });

    mav->on_imu([weak, weak_source](const mg::core::mavlink::ImuInfo& info) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
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
            const auto mav = src->shared_mavlink();
            self->cache_.update(mg::core::telemetry::TelemetryType::kImu, mav.get());
        }
    });
    mav->on_pressure([weak, weak_source](const mg::core::mavlink::PressureInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        const auto mav = src->shared_mavlink();
        self->cache_.update(mg::core::telemetry::TelemetryType::kPressure, mav.get());
    });
    mav->on_rc_channels([weak, weak_source](const mg::core::mavlink::RcChannelsInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        const auto mav = src->shared_mavlink();
        self->cache_.update(mg::core::telemetry::TelemetryType::kRcChannels, mav.get());
    });
    mav->on_servo_output([weak, weak_source](const mg::core::mavlink::ServoOutputInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        const auto mav = src->shared_mavlink();
        self->cache_.update(mg::core::telemetry::TelemetryType::kServoOutput, mav.get());
    });
    mav->on_home_position([weak, weak_source](const mg::core::mavlink::HomePositionInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        const auto mav = src->shared_mavlink();
        self->cache_.update(mg::core::telemetry::TelemetryType::kHomePosition, mav.get());
    });
    mav->on_highres_imu([weak, weak_source](const mg::core::mavlink::HighresImuInfo&) {
        auto self = weak.lock(); if (!self) return;
        auto src = weak_source.lock(); if (!src) return;
        const auto mav = src->shared_mavlink();
        self->cache_.update(mg::core::telemetry::TelemetryType::kHighresImu, mav.get());
    });

    MGC_LOG_INFO("[Vehicle %d] Telemetry callbacks wired", id_);
}

} // namespace mg::core::vehicle
