#pragma once

#include "core/calibration/calibration_service.h"
#include "core/camera/camera_manager.h"
#include "core/flight/flight_mode_manager.h"
#include "core/geofence/geofence_manager.h"
#include "core/platform/platform_mutex.h"
#include "core/sensor/sensor_id_decoder.h"
#include "core/settings/flight_log_writer.h"
#include "core/settings/flight_logging_service.h"
#include "core/settings/mavlink_log_writer.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/telemetry/waveform_buffer.h"
#include "core/transport/connection_manager.h"
#include "core/transport/link_config.h"
#include "core/video/video_receiver.h"
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

    std::string imu_sensor;
    std::string mag_sensor;
    std::string baro_sensor;
    std::string gyro_sensor;
    std::string airspeed_sensor;
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
    void disconnect();
    bool is_connected() const;

    void wire_telemetry_callbacks();
    void set_identity(uint8_t sysid, uint8_t compid, const std::string& fw_type, const std::string& v_type);

    uint8_t system_id() const { return info_.system_id; }
    int vehicle_id() const { return info_.vehicle_id; }
    VehicleState state() const { return state_.load(); }
    const VehicleInfo& info() const { return info_; }
    uint32_t flight_time_s() const { return flight_time_s_.load(); }
    std::shared_ptr<std::atomic<bool>> alive_flag() { return alive_flag_; }

    using EventCallback = std::function<void(int, const std::string&)>;
    void set_event_callback(EventCallback cb) {
        mg::core::platform::LockGuard lk(event_cb_mtx_);
        event_cb_ = std::move(cb);
    }

    mg::core::transport::ConnectionManager& conn() { return conn_; }
    mg::core::mavlink::MavlinkAdapter* mavlink() { return conn_.get_mavlink(); }
    mg::core::telemetry::TelemetryCache& cache() { return cache_; }
    void set_usb_board_name(const std::string& name) { cache_.set_usb_board_name(name); }

    mg::core::calibration::CalibrationService& calibration() { return calibration_; }
    mg::core::flight::FlightModeManager& flight_modes() { return flight_modes_; }
    mg::core::geofence::GeofenceManager& geofence() { return geofence_; }
    mg::core::camera::CameraManager& camera() { return camera_; }
    mg::core::video::VideoReceiver& video() { return video_; }
    mg::core::settings::MavlinkLogWriter& mavlink_log() { return mavlink_log_; }
    mg::core::settings::FlightLoggingService& flight_log() { return flight_log_; }
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
    void fire_event(int event_type, const std::string& json);
    void update_and_fire(mg::core::telemetry::TelemetryType type, int event_id);
    /// 清空所有 IMU 实例的波形缓冲与活跃时间。
    void clear_waveforms();

    int id_;
    VehicleInfo info_;
    std::atomic<VehicleState> state_{VehicleState::Disconnected};

    mg::core::platform::Mutex event_cb_mtx_;
    EventCallback event_cb_;
    mg::core::transport::ConnectionManager conn_;
    mg::core::telemetry::TelemetryCache cache_;

    mg::core::calibration::CalibrationService calibration_;
    mg::core::flight::FlightModeManager flight_modes_;
    mg::core::geofence::GeofenceManager geofence_;
    mg::core::camera::CameraManager camera_;
    mg::core::video::VideoReceiver video_;
    mg::core::settings::MavlinkLogWriter mavlink_log_;
    mg::core::settings::FlightLoggingService flight_log_;
    mg::core::sensor::SensorIdDecoder sensor_decoder_;
    std::shared_ptr<std::atomic<bool>> alive_flag_{std::make_shared<std::atomic<bool>>(true)};
    mg::core::telemetry::TelemetryRingBuffer ring_buffer_;
    /// 每个 IMU 实例一份独立缓冲，互不共用容量。
    std::array<mg::core::telemetry::WaveformBuffer, kMaxImuInstances> waveforms_;
    std::array<SensorInstanceActivity, kMaxImuInstances> instance_activity_;
    std::atomic<uint32_t> flight_time_s_{0};
    std::atomic<int64_t> last_armed_timestamp_ms_{0};
    std::atomic<bool> needs_reidentify_{false};
    std::atomic<bool> requested_firmware_version_{false};
};

} // namespace mg::core::vehicle
