#pragma once

#include "core/calibration/calibration_service.h"
#include "core/flight/flight_mode_manager.h"
#include "core/telemetry/flight_message_log.h"
#include "core/platform/platform_mutex.h"
#include "core/sensor/sensor_id_decoder.h"
#include "core/settings/flight_logging_service.h"
#include "core/settings/mavlink_log_writer.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/telemetry/waveform_buffer.h"
#include "core/transport/connection_manager.h"
#include "core/transport/link_config.h"
#include "core/vehicle/vehicle_link_manager.h"
#include "core/telemetry/telemetry_cache.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace mg::core::vehicle {

enum class VehicleState {
    Disconnected,
    Connecting,
    WaitingHeartbeat,
    Connected,
    HeartbeatLost,
    Disconnecting,
    Initializing,
};

struct VehicleInfo {
    int vehicle_id = 0;
    uint8_t system_id = 0;
    uint8_t component_id = 0;
    std::string name;
    std::string firmware_type;
    std::string vehicle_type;
    std::string endpoint;
    std::string transport_type;
    std::string port_name;
    uint32_t baud_rate = 0;
    VehicleState state = VehicleState::Disconnected;

    std::string serial_port;
    uint32_t serial_baud = 0;
};

class Vehicle : public std::enable_shared_from_this<Vehicle> {
public:
    explicit Vehicle(int vehicle_id);
    ~Vehicle();

    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    bool connect(const std::string& endpoint);

    /// Attach this Vehicle to a shared LinkSession. system_id/component_id may
    /// be zero for the first placeholder Vehicle before heartbeat discovery.
    bool attach_shared_session(
        const std::shared_ptr<mg::core::transport::LinkSession>& session,
        const std::string& endpoint,
        uint8_t system_id = 0,
        uint8_t component_id = 0);

    /// Attach an additional shared route to this Vehicle. Telemetry from the
    /// route is merged into the same Vehicle and participates in primary-link
    /// selection.
    bool attach_shared_route(
        const std::shared_ptr<mg::core::transport::LinkSession>& session,
        int route_id,
        uint8_t system_id,
        uint8_t component_id);

    /// Attach a shared route as the first/primary route with an explicit ID.
    bool attach_shared_primary_route(
        const std::shared_ptr<mg::core::transport::LinkSession>& session,
        int route_id,
        const std::string& endpoint,
        uint8_t system_id,
        uint8_t component_id);

    /// Remove one route and reselect primary if needed.
    bool remove_shared_route(int route_id);

    /// Synchronize the command/calibration source with VehicleLinkManager.
    void refresh_primary_connection();
    /// Physical lifecycle, including waiting routes (not only the primary).
    bool all_routes_disconnected() const;
    std::shared_ptr<mg::core::transport::ConnectionManager> command_connection() const;
    /// Retain transaction ownership across primary-link changes.
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> operation_mavlink(const std::string& id);
    bool cancel_operation(const std::string& id);

    void disconnect();
    bool is_connected() const;

    void wire_telemetry_callbacks();
    void wire_telemetry_callbacks_for(
        const std::shared_ptr<mg::core::transport::ConnectionManager>& source,
        int route_id);
    void set_identity(uint8_t sysid, uint8_t compid, const std::string& fw_type, const std::string& v_type);

    uint8_t system_id() const { return info_.system_id; }
    int vehicle_id() const { return info_.vehicle_id; }
    VehicleState state() const { return state_.load(); }
    const VehicleInfo& info() const { return info_; }

    using EventCallback = std::function<void(int, const std::string&)>;
    void set_event_callback(EventCallback cb) {
        mg::core::platform::LockGuard lk(event_cb_mtx_);
        event_cb_ = std::move(cb);
    }

    /// Active connection. Never null.
    ///
    /// disconnect() swaps every route out of `connections_` while the Vehicle can
    /// still be reachable through the registry (and the fallback vehicle is
    /// always reachable); the primary connection is then null and the old
    /// `return *primary_connection()` dereferenced it — EXC_BAD_ACCESS at
    /// ConnectionManager::state_ (offset 0x74), reproducible with
    /// `mg_connection_close()` followed by a connection state read on a
    /// core that has no active vehicle. When no route is attached this returns
    /// `detached_conn_`: a never-wired manager that reports the disconnected
    /// baseline (state Disconnected, no link, no adapter, no session), so readers
    /// stay safe without a lifetime check.
    mg::core::transport::ConnectionManager& conn() {
        if (const auto connection = primary_connection()) return *connection;
        return detached_conn_;
    }
    mg::core::vehicle::VehicleLinkManager& link_manager() { return link_manager_; }
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> shared_mavlink() const {
        const auto connection = primary_connection();
        return connection ? connection->shared_mavlink() : nullptr;
    }
    bool set_signing_key(const uint8_t* key, size_t key_size,
                         bool sign_outgoing, uint64_t persisted_timestamp,
                         const std::string& key_name = {},
                         bool auto_detected = false);
    bool set_signing_candidates(
        const std::vector<mg::core::mavlink::SigningCandidate>& candidates);
    void clear_signing_candidates();
    void clear_signing_key();
    bool signing_enabled() const;
    int signing_status() const;
    std::string signing_key_name() const;
    bool signing_auto_detected() const;
    size_t signing_candidate_count() const;
    uint64_t signing_timestamp() const;
    bool send_setup_signing(uint8_t target_system,
                            uint8_t target_component,
                            const uint8_t* key,
                            size_t key_size,
                            uint64_t initial_timestamp = 0);
    int primary_route_id() const { return primary_route_id_; }
    size_t connection_count() const;
    std::shared_ptr<mg::core::transport::ConnectionManager>
    connection_for_route(int route_id) const;
    mg::core::telemetry::TelemetryCache& cache() { return cache_; }
    void set_usb_board_name(const std::string& name) { cache_.set_usb_board_name(name); }

    mg::core::calibration::CalibrationService& calibration() { return calibration_; }
    mg::core::flight::FlightModeManager& flight_modes() { return flight_modes_; }
    // GeofenceManager已删除（独立模块，前端未接入）
    mg::core::settings::MavlinkLogWriter& mavlink_log() { return mavlink_log_; }
    mg::core::settings::FlightLoggingService& flight_log() { return flight_log_; }
    /// 飞控消息历史（消息框用）。缓存只留最新一条，历史单独放这里。
    mg::core::telemetry::FlightMessageLog& message_log() { return message_log_; }
    mg::core::sensor::SensorIdDecoder& sensor_decoder() { return sensor_decoder_; }
    mg::core::telemetry::TelemetryRingBuffer& ring_buffer() { return ring_buffer_; }

    /// 支持的 IMU 实例数上限：对应 IMU1/IMU2/IMU3。
    static constexpr size_t kMaxImuInstances = 3;

    /// 某个 IMU 实例的最近活跃时间。前端据此决定显示几个实例，
    /// 所以只记录"最近确实收到过数据"，不猜测固件配置。
    struct SensorInstanceActivity {
        /// 最近一帧 IMU（加速度计/陀螺仪）的单调时钟毫秒，0 表示从未收到。
        std::atomic<uint64_t> imu_last_ms{0};
        /// 最近一次有效磁力计读数的单调时钟毫秒，0 表示从未收到。
        /// SCALED_IMU 在没有对应罗盘实例时磁力计三轴固定是 0，
        /// 所以用"是否出现过非零读数"判断该实例有没有磁力计。
        std::atomic<uint64_t> mag_last_ms{0};
    };

    /// 取某个 IMU 实例的波形缓冲，instance 超范围返回 nullptr。
    mg::core::telemetry::WaveformBuffer* waveform(size_t instance) {
        return instance < kMaxImuInstances ? &waveforms_[instance] : nullptr;
    }

    /// 某个 IMU 实例的最近活跃时间，instance 超范围返回 0。
    uint64_t imu_last_seen_ms(size_t instance) const {
        return instance < kMaxImuInstances ? instance_activity_[instance].imu_last_ms.load()
                                           : 0;
    }

    /// 某个 IMU 实例的最近磁力计活跃时间，instance 超范围返回 0。
    uint64_t mag_last_seen_ms(size_t instance) const {
        return instance < kMaxImuInstances ? instance_activity_[instance].mag_last_ms.load()
                                           : 0;
    }

    void init_subsystems();
    void load_sensor_decoder(const std::string& json_path);

private:
    void finish_connect();
    void fire_event(int event_type, const std::string& json);
    std::shared_ptr<mg::core::transport::ConnectionManager>
    primary_connection() const;
    void install_connection_state_callback(
        int route_id,
        const std::shared_ptr<mg::core::transport::ConnectionManager>& connection);
    void update_and_fire(
        mg::core::transport::ConnectionManager* source,
        mg::core::telemetry::TelemetryType type,
        int event_id);
    void inherit_signing(mg::core::mavlink::MavlinkAdapter& mav);
    void apply_detected_signing_candidate(
        const mg::core::mavlink::SigningCandidate& candidate);
    /// 清空所有 IMU 实例的波形缓冲与活跃时间。
    void clear_waveforms();

    int id_;
    VehicleInfo info_;
    std::atomic<VehicleState> state_{VehicleState::Disconnected};

    mg::core::platform::Mutex event_cb_mtx_;
    EventCallback event_cb_;
    mutable mg::core::platform::Mutex connections_mtx_;
    std::map<int, std::shared_ptr<mg::core::transport::ConnectionManager>>
        connections_;
    /// Placeholder returned by conn() when no route is attached. Permanently
    /// unwired, so its accessors report the disconnected baseline; it exists so
    /// conn() can stay total (see conn()) without sharing state across vehicles.
    mg::core::transport::ConnectionManager detached_conn_;
    int primary_route_id_ = -1;
    mutable mg::core::platform::Mutex signing_policy_mtx_;
    std::optional<mg::core::mavlink::SigningCandidate> signing_policy_;
    bool signing_policy_outgoing_ = true;
    bool signing_policy_detected_ = false;
    std::vector<mg::core::mavlink::SigningCandidate> signing_candidates_;
    mg::core::platform::Mutex operation_mtx_;
    std::map<std::string, std::shared_ptr<mg::core::mavlink::MavlinkAdapter>> operation_adapters_;
    mg::core::vehicle::VehicleLinkManager link_manager_;
    mg::core::telemetry::TelemetryCache cache_;

    mg::core::calibration::CalibrationService calibration_;
    mg::core::flight::FlightModeManager flight_modes_;
    // mg::core::geofence::GeofenceManager geofence_; // 已删除
    mg::core::settings::MavlinkLogWriter mavlink_log_;
    mg::core::settings::FlightLoggingService flight_log_;
    mg::core::telemetry::FlightMessageLog message_log_;
    mg::core::sensor::SensorIdDecoder sensor_decoder_;
    mg::core::telemetry::TelemetryRingBuffer ring_buffer_;
    /// 每个 IMU 实例一份独立缓冲，互不共用容量。
    std::array<mg::core::telemetry::WaveformBuffer, kMaxImuInstances> waveforms_;
    std::array<SensorInstanceActivity, kMaxImuInstances> instance_activity_;
    std::atomic<bool> needs_reidentify_{false};
    std::atomic<bool> requested_firmware_version_{false};
    std::atomic<uint64_t> last_home_request_ms_{0};
};

} // namespace mg::core::vehicle
