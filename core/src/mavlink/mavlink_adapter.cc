/**
 * mavlink_adapter.cc - MAVLink 协议适配器实现（核心生命周期 + 接收 + 数据获取）
 *
 * Split from monolithic mavlink_adapter.cc for maintainability.
 * See mavlink_adapter_internal.h for Impl struct and message handlers.
 * See mavlink_senders.cc for all outgoing message functions.
 *
 * References:
 *   MAVLink: third_party/mavlink/include/mavlink/v2.0/
 *   QGC:     src/Vehicle/FactGroups/VehicleFactGroup.cc
 *   MP:      ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs
 */

#include "mavlink_adapter_internal.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>

namespace mg::core::mavlink {

// 生命周期
// ================================================================

// 创建适配器实例，初始化 PIMPL
MavlinkAdapter::MavlinkAdapter()
    : impl_(std::make_unique<Impl>()) {}

MavlinkAdapter::~MavlinkAdapter() = default;

bool MavlinkAdapter::download_file(uint8_t target_system, uint8_t target_component,
                                    const std::string& remote_path,
                                    const std::string& local_path,
                                    std::string* error) {
    mg::core::ftp::FtpResult result;
    const bool success = impl_->ftp.download_file(
        target_system, target_component, remote_path, local_path, nullptr, &result);
    if (!success && error) {
        *error = result.error;
        if (result.error_code != 0) {
            *error += " (MAVLink FTP 错误码 " +
                std::to_string(static_cast<unsigned>(result.error_code)) + ")";
        }
    }
    return success;
}

bool MavlinkAdapter::upload_file(uint8_t target_system, uint8_t target_component,
                                 const std::string& local_path,
                                 const std::string& remote_path,
                                 std::string* error) {
    mg::core::ftp::FtpResult result;
    const bool success = impl_->ftp.upload_file(
        target_system, target_component, local_path, remote_path, nullptr, &result);
    if (!success && error) {
        *error = result.error;
        if (result.error_code != 0) {
            *error += " (MAVLink FTP 错误码 " +
                std::to_string(static_cast<unsigned>(result.error_code)) + ")";
        }
    }
    return success;
}

std::vector<mg::core::ftp::DirEntry> MavlinkAdapter::list_directory(
    uint8_t target_system, uint8_t target_component,
    const std::string& remote_path, std::string* error) {
    mg::core::ftp::FtpResult result;
    auto entries = impl_->ftp.list_directory(
        target_system, target_component, remote_path, &result);
    if (!result.success && error != nullptr) {
        *error = result.error;
        if (result.error_code != 0) {
            *error += " (MAVLink FTP 错误码 " +
                std::to_string(static_cast<unsigned>(result.error_code)) + ")";
        }
    }
    return entries;
}

void MavlinkAdapter::cancel_file_transfer() {
    impl_->ftp.cancel();
}

// ================================================================
// 接收路径
// ================================================================

// 喂入原始字节，逐字节解析 MAVLink 消息
// 使用独立的 parse_msg/parse_status，不与发送函数共享 impl_->msg
bool MavlinkAdapter::feed_raw(const uint8_t* data, size_t size) {
    // QGC MAVLinkProtocol and MP MAVLinkInterface isolate RX state per link.
    // Use official caller-owned buffers rather than global channel state.
    // Dispatch outside the RX lock because handlers may call external callbacks.
    const bool compatibility_bypass_disabled =
        impl_->signing_enabled.load(std::memory_order_acquire);
    if (!compatibility_bypass_disabled) {
        impl_->feed_mag_progress_compat(data, size);
    }
    std::vector<mavlink_message_t> frames;
    {
        mg::core::platform::LockGuard lk(impl_->receive_mtx);
        mavlink_message_t decoded{};
        mavlink_status_t status{};
        for (size_t i = 0; i < size; ++i) {
            const auto framing = mavlink_frame_char_buffer(
                &impl_->rx_message, &impl_->rx_status, data[i], &decoded, &status);
            if (framing == MAVLINK_FRAMING_OK) {
                frames.push_back(decoded);
            } else if (framing == MAVLINK_FRAMING_BAD_CRC ||
                       framing == MAVLINK_FRAMING_BAD_SIGNATURE) {
                // Same resynchronization as official mavlink_parse_char().
                impl_->rx_status.parse_state = MAVLINK_PARSE_STATE_IDLE;
                if (data[i] == MAVLINK_STX) {
                    impl_->rx_status.parse_state = MAVLINK_PARSE_STATE_GOT_STX;
                    impl_->rx_message.len = 0;
                    mavlink_start_checksum(&impl_->rx_message);
                }
            }
        }
    }
    bool accepted = false;
    for (const auto& frame : frames) {
        // Candidate selection is not authentication. Frames parsed before a
        // key was installed must pass official stream/timestamp checks before
        // delivery, including every other frame already collected in this chunk.
        if (!compatibility_bypass_disabled) {
            const bool signed_frame = (frame.incompat_flags & MAVLINK_IFLAG_SIGNED) != 0;
            if (signed_frame && !signing_enabled()) {
                if (signing_candidate_count() > 0 && !try_auto_detect_signing(frame)) continue;
            }
            if (signing_enabled()) {
                mg::core::platform::LockGuard lk(impl_->mtx);
                mg::core::platform::LockGuard send_lock(impl_->send_mtx);
                mg::core::platform::LockGuard receive_lock(impl_->receive_mtx);
                if (!signed_frame || !mavlink_signature_check(
                        &impl_->signing, &impl_->signing_streams, &frame)) continue;
            }
        }
        impl_->process(frame);
        accepted = true;
    }
    return accepted;
}

namespace {

uint64_t signing_now_ticks() {
    constexpr uint64_t kSigningEpochMicros = 1420070400000000ULL;
    const uint64_t now_micros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    return (now_micros - kSigningEpochMicros) / 10ULL;
}

uint8_t next_signing_link_id() {
    static std::atomic<uint32_t> next{1};
    const uint32_t value = next.fetch_add(1, std::memory_order_relaxed);
    return static_cast<uint8_t>((value % 255U) + 1U);
}

bool signing_key_matches(const std::array<uint8_t, 32>& key,
                         const mavlink_message_t& message) {
    if ((message.incompat_flags & MAVLINK_IFLAG_SIGNED) == 0) return false;
    mavlink_sha256_ctx ctx;
    mavlink_sha256_init(&ctx);
    mavlink_sha256_update(&ctx, key.data(), key.size());
    const auto* header = reinterpret_cast<const uint8_t*>(&message.magic);
    mavlink_sha256_update(&ctx, header, MAVLINK_NUM_HEADER_BYTES);
    mavlink_sha256_update(&ctx, _MAV_PAYLOAD(&message), message.len);
    const uint8_t checksum[2] = {
        static_cast<uint8_t>(message.checksum & 0xFF),
        static_cast<uint8_t>(message.checksum >> 8),
    };
    mavlink_sha256_update(&ctx, checksum, sizeof(checksum));
    mavlink_sha256_update(&ctx, message.signature, 7);
    uint8_t signature[6]{};
    mavlink_sha256_final_48(&ctx, signature);
    return std::memcmp(signature, message.signature + 7,
                       sizeof(signature)) == 0;
}

} // namespace

// 设置底层发送回调，加锁保护
void MavlinkAdapter::set_send_callback(SendCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->send_cb = std::move(cb);
}

bool MavlinkAdapter::set_signing_key(const uint8_t* key,
                                     size_t key_size,
                                     bool sign_outgoing,
                                     uint64_t persisted_timestamp,
                                     const std::string& key_name,
                                     bool auto_detected) {
    if (key == nullptr || key_size < sizeof(impl_->signing.secret_key)) {
        return false;
    }

    mg::core::platform::LockGuard lk(impl_->mtx);
    mg::core::platform::LockGuard send_lock(impl_->send_mtx);
    mg::core::platform::LockGuard receive_lock(impl_->receive_mtx);

    std::fill(std::begin(impl_->signing.secret_key),
              std::end(impl_->signing.secret_key), 0);
    impl_->signing_streams = {};
    impl_->signing.link_id = next_signing_link_id();
    impl_->signing.flags =
        sign_outgoing ? MAVLINK_SIGNING_FLAG_SIGN_OUTGOING : 0;
    impl_->signing.accept_unsigned_callback = nullptr;
    impl_->signing.last_status = MAVLINK_SIGNING_STATUS_NONE;
    std::copy_n(key, sizeof(impl_->signing.secret_key),
                impl_->signing.secret_key);
    impl_->signing.timestamp = std::max(
        signing_now_ticks(), persisted_timestamp + 1ULL);

    impl_->tx_status.signing = &impl_->signing;
    impl_->tx_status.signing_streams = &impl_->signing_streams;
    impl_->rx_status.signing = &impl_->signing;
    impl_->rx_status.signing_streams = &impl_->signing_streams;
    impl_->signing_key_name = key_name;
    impl_->signing_auto_detected = auto_detected;
    impl_->signing_enabled.store(true, std::memory_order_release);
    return true;
}

bool MavlinkAdapter::set_signing_candidates(
    const std::vector<SigningCandidate>& candidates) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->signing_candidates = candidates;
    impl_->signing_detect_cooldown = {};
    return true;
}

void MavlinkAdapter::clear_signing_candidates() {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->signing_candidates.clear();
    impl_->signing_detect_cooldown = {};
}

void MavlinkAdapter::set_signing_detected_callback(
    SigningDetectedCallback callback) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->signing_detected_cb = std::move(callback);
}

void MavlinkAdapter::clear_signing_key() {
    mg::core::platform::LockGuard lk(impl_->mtx);
    mg::core::platform::LockGuard send_lock(impl_->send_mtx);
    mg::core::platform::LockGuard receive_lock(impl_->receive_mtx);

    impl_->tx_status.signing = nullptr;
    impl_->tx_status.signing_streams = nullptr;
    impl_->rx_status.signing = nullptr;
    impl_->rx_status.signing_streams = nullptr;
    std::fill(std::begin(impl_->signing.secret_key),
              std::end(impl_->signing.secret_key), 0);
    impl_->signing = {};
    impl_->signing_streams = {};
    impl_->signing_key_name.clear();
    impl_->signing_auto_detected = false;
    impl_->signing_enabled.store(false, std::memory_order_release);
}

bool MavlinkAdapter::signing_enabled() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->signing_enabled.load(std::memory_order_acquire);
}

mavlink_signing_status_t MavlinkAdapter::signing_status() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->signing_enabled.load(std::memory_order_acquire)
            ? impl_->signing.last_status
                                  : MAVLINK_SIGNING_STATUS_NONE;
}

std::string MavlinkAdapter::signing_key_name() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->signing_key_name;
}

bool MavlinkAdapter::signing_auto_detected() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->signing_auto_detected;
}

size_t MavlinkAdapter::signing_candidate_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->signing_candidates.size();
}

uint64_t MavlinkAdapter::signing_timestamp() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->signing.timestamp;
}

bool MavlinkAdapter::try_auto_detect_signing(
    const mavlink_message_t& message) {
    if ((message.incompat_flags & MAVLINK_IFLAG_SIGNED) == 0) return false;

    std::vector<SigningCandidate> candidates;
    const auto now = std::chrono::steady_clock::now();
    {
        mg::core::platform::LockGuard lk(impl_->mtx);
        if (impl_->signing_enabled.load(std::memory_order_acquire) ||
            impl_->signing_candidates.empty() ||
            now < impl_->signing_detect_cooldown) {
            return false;
        }
        candidates = impl_->signing_candidates;
    }

    for (const auto& candidate : candidates) {
        if (!signing_key_matches(candidate.key, message)) continue;
        // Candidate matching chooses a key; freshness is enforced before
        // committing it so replayed first packets cannot authenticate a route.
        uint64_t packet_ticks = 0;
        for (int i = 0; i < 6; ++i) {
            packet_ticks |= static_cast<uint64_t>(message.signature[1 + i]) << (8 * i);
        }
        const uint64_t floor = std::max(signing_now_ticks(), candidate.persisted_timestamp);
        if (packet_ticks + 6000000ULL < floor) continue;
        if (set_signing_key(candidate.key.data(), candidate.key.size(), true,
                            candidate.persisted_timestamp, candidate.name,
                            true)) {
            SigningDetectedCallback callback;
            {
                mg::core::platform::LockGuard lk(impl_->mtx);
                callback = impl_->signing_detected_cb;
            }
            if (callback) callback(candidate);
            return true;
        }
    }

    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->signing_detect_cooldown = now + std::chrono::seconds(2);
    return false;
}

bool MavlinkAdapter::send_setup_signing(uint8_t target_system,
                                        uint8_t target_component,
                                        const uint8_t* key,
                                        size_t key_size,
                                        uint64_t initial_timestamp) {
    if (key == nullptr || key_size < sizeof(impl_->signing.secret_key)) {
        return false;
    }
    mg::core::platform::LockGuard lk(impl_->mtx);
    const uint64_t timestamp = initial_timestamp == 0
        ? signing_now_ticks()
        : initial_timestamp;
    mavlink_msg_setup_signing_pack(
        255, 0, &impl_->msg, target_system, target_component,
        key, timestamp);
    return impl_->send_message_with_mode(impl_->msg, false);
}

// ================================================================
// 回调注册方法（每个都加锁保护）
// ================================================================

// 注册心跳回调
void MavlinkAdapter::on_heartbeat(HeartbeatCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.heartbeat_cb = std::move(cb);
}

void MavlinkAdapter::on_sys_status(SysStatusCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.sys_status_cb = std::move(cb);
}

void MavlinkAdapter::on_gps(GpsCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.gps_cb = std::move(cb);
}

void MavlinkAdapter::on_imu(ImuCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.imu_cb = std::move(cb);
}

void MavlinkAdapter::on_pressure(PressureCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.pressure_cb = std::move(cb);
}

void MavlinkAdapter::on_attitude(AttitudeCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.attitude_cb = std::move(cb);
}

void MavlinkAdapter::on_vfr_hud(VfrHudCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.vfr_hud_cb = std::move(cb);
}

void MavlinkAdapter::on_firmware_version(FirmwareVersionCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.firmware_version_cb = std::move(cb);
}

void MavlinkAdapter::on_param(ParamCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.param_cb = std::move(cb);
}

void MavlinkAdapter::on_command_ack(CommandAckCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.command_ack_cb = std::move(cb);
}

void MavlinkAdapter::on_command_long(CommandLongCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.command_long_cb = std::move(cb);
}

void MavlinkAdapter::on_mag_cal_progress(MagCalibrationProgressCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.mag_cal_progress_cb = std::move(cb);
}

void MavlinkAdapter::on_mag_cal_report(MagCalibrationReportCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.mag_cal_report_cb = std::move(cb);
}

void MavlinkAdapter::on_rc_channels(RcChannelsCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.rc_channels_cb = std::move(cb);
}

void MavlinkAdapter::on_servo_output(ServoOutputCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.servo_output_cb = std::move(cb);
}

std::optional<RcChannelsInfo> MavlinkAdapter::latest_rc_channels() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.rc_channels;
}

std::optional<ServoOutputInfo> MavlinkAdapter::latest_servo_output() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.servo_output;
}


void MavlinkAdapter::on_statustext(StatustextCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.statustext_cb = std::move(cb);
}

std::string MavlinkAdapter::last_statustext() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.last_statustext;
}

uint8_t MavlinkAdapter::last_statustext_severity() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.last_statustext_severity;
}


void MavlinkAdapter::on_global_position(GlobalPositionCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.global_position_cb = std::move(cb);
}

void MavlinkAdapter::on_home_position(HomePositionCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.home_position_cb = std::move(cb);
}

void MavlinkAdapter::on_battery(BatteryCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.battery_cb = std::move(cb);
}

void MavlinkAdapter::on_highres_imu(HighresImuCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.highres_imu_cb = std::move(cb);
}

void MavlinkAdapter::on_prearm_check(PrearmCheckCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.prearm_check_cb = std::move(cb);
}

void MavlinkAdapter::on_vibration(VibrationCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.vibration_cb = std::move(cb);
}

void MavlinkAdapter::on_esc_status(EscStatusCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.esc_status_cb = std::move(cb);
}

void MavlinkAdapter::on_wind_estimate(WindEstimateCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.wind_estimate_cb = std::move(cb);
}

void MavlinkAdapter::on_distance_sensor(DistanceSensorCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.distance_sensor_cb = std::move(cb);
}

void MavlinkAdapter::on_adsb_vehicle(AdsbVehicleCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.adsb_vehicle_cb = std::move(cb);
}

void MavlinkAdapter::on_mission_reached(MissionReachedCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.mission_reached_cb = std::move(cb);
}

void MavlinkAdapter::on_video_stream_info(VideoStreamCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.video_stream_cb = std::move(cb);
}

void MavlinkAdapter::on_extended_sys_state(ExtendedSysStateCallback cb) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->tel.extended_sys_state_cb = std::move(cb);
}

std::optional<StatustextInfo> MavlinkAdapter::latest_statustext_info() const {
    // STATUSTEXT doesn't store a full StatustextInfo, reuse existing fields
    mg::core::platform::LockGuard lk(impl_->mtx);
    if (impl_->tel.last_statustext.empty()) return std::nullopt;
    StatustextInfo info{};
    info.severity = impl_->tel.last_statustext_severity;
    size_t n = impl_->tel.last_statustext.size();
    if (n > 50) n = 50;
    memcpy(info.text, impl_->tel.last_statustext.c_str(), n);
    info.text[n] = 0;
    return info;
}

std::optional<GlobalPositionInfo> MavlinkAdapter::latest_global_position() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.global_position;
}

std::optional<HomePositionInfo> MavlinkAdapter::latest_home_position() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.home_position;
}

std::optional<BatteryInfo> MavlinkAdapter::latest_battery() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.battery;
}

std::optional<HighresImuInfo> MavlinkAdapter::latest_highres_imu() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.highres_imu;
}

std::optional<PrearmCheckInfo> MavlinkAdapter::latest_prearm_check() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.prearm_check;
}

std::optional<VibrationInfo> MavlinkAdapter::latest_vibration() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.vibration;
}

std::optional<EscStatusInfo> MavlinkAdapter::latest_esc_status() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.esc_status;
}

std::optional<WindEstimateInfo> MavlinkAdapter::latest_wind_estimate() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.wind_estimate;
}

std::optional<ExtendedSysStateInfo> MavlinkAdapter::latest_extended_sys_state() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.extended_sys_state;
}
// ================================================================
// 数据获取器（线程安全快照）
// ================================================================

// 获取最新心跳数据
std::optional<HeartbeatInfo> MavlinkAdapter::latest_heartbeat() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.heartbeat;
}

std::optional<AttitudeInfo> MavlinkAdapter::latest_attitude() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.attitude;
}

std::optional<VfrHudInfo> MavlinkAdapter::latest_vfr_hud() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.vfr_hud;
}

std::optional<ImuInfo> MavlinkAdapter::latest_imu() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.imu;
}

std::optional<PressureInfo> MavlinkAdapter::latest_pressure() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.pressure;
}

std::optional<FirmwareVersionInfo> MavlinkAdapter::latest_firmware_version() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.firmware_version;
}


// 按名称从参数缓存获取参数
std::optional<ParamInfo> MavlinkAdapter::get_param_by_name(const char* param_id) const {
    if (!param_id) return std::nullopt;
    mg::core::platform::LockGuard lk(impl_->mtx);
    auto it = impl_->pc.param_map.find(std::string(param_id));
    if (it != impl_->pc.param_map.end()) return it->second;
    return std::nullopt;
}
// 获取所有参数的快照
std::vector<ParamInfo> MavlinkAdapter::all_params() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    std::vector<ParamInfo> result;
    result.reserve(impl_->pc.param_map.size());
    for (auto& [key, val] : impl_->pc.param_map) {
        result.push_back(val);
    }
    return result;
}

std::optional<CommandAckInfo> MavlinkAdapter::latest_command_ack() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.command_ack;
}

uint16_t MavlinkAdapter::param_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.expected_param_count;
}

size_t MavlinkAdapter::params_received() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.param_map.size();
}

size_t MavlinkAdapter::params_download_received() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.received_indices.size();
}

bool MavlinkAdapter::param_download_active() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    switch (impl_->pc.download_state) {
        case ParamCache::DownloadState::WaitingList:
        case ParamCache::DownloadState::Receiving:
        case ParamCache::DownloadState::Recovering:
            return true;
        case ParamCache::DownloadState::Idle:
        case ParamCache::DownloadState::Complete:
        case ParamCache::DownloadState::Failed:
            return false;
    }
    return false;
}

bool MavlinkAdapter::param_download_cancelled() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.download_cancelled;
}

bool MavlinkAdapter::param_download_failed() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.download_failed;
}

std::string MavlinkAdapter::param_download_error() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.download_error;
}

int MavlinkAdapter::param_download_retry_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    int retries = impl_->pc.list_retry_count;
    for (const auto& [index, count] : impl_->pc.missing_indices) {
        (void)index;
        retries = std::max(retries, count);
    }
    return retries;
}

int MavlinkAdapter::param_download_missing_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return static_cast<int>(impl_->pc.missing_indices.size());
}

int MavlinkAdapter::param_download_failed_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return static_cast<int>(impl_->pc.failed_indices.size());
}

bool MavlinkAdapter::cancel_param_download() {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->pc.cancel_download();
}

std::vector<MavlinkAdapter::ParamChangeRecord> MavlinkAdapter::param_change_history() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    std::vector<ParamChangeRecord> records;
    records.reserve(impl_->pc.write_history.size());
    for (const auto& record : impl_->pc.write_history) {
        ParamChangeRecord output;
        output.id = record.id;
        output.name = record.name;
        output.requested_value = record.requested_value;
        output.actual_value = record.actual_value;
        output.type = record.type;
        switch (record.state) {
            case ParamCache::WriteState::Pending:
                output.state = "pending";
                break;
            case ParamCache::WriteState::Confirmed:
                output.state = "confirmed";
                break;
            case ParamCache::WriteState::Failed:
                output.state = "failed";
                break;
        }
        output.error = record.error;
        output.requested_at_ms = record.requested_at_ms;
        output.completed_at_ms = record.completed_at_ms;
        records.push_back(std::move(output));
    }
    return records;
}

int MavlinkAdapter::start_param_refresh(uint8_t target_system,
                                        uint8_t target_component,
                                        const std::vector<std::string>& names) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->pc.begin_refresh(names, target_system, target_component);
    int sent = 0;
    for (const auto& name : names) {
        mavlink_msg_param_request_read_pack(
            255, 0, &impl_->msg,
            target_system, target_component, name.c_str(), -1);
        if (impl_->send_msg()) {
            sent++;
        } else {
            impl_->pc.fail_refresh(name);
        }
    }
    return sent;
}

MavlinkAdapter::ParamRefreshStatus MavlinkAdapter::param_refresh_status() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    const auto status = impl_->pc.refresh_status();
    ParamRefreshStatus output;
    output.active = status.active;
    output.total = status.total;
    output.completed = status.completed;
    output.pending = status.pending;
    output.round = status.round;
    output.failed = status.failed;
    return output;
}

uint64_t MavlinkAdapter::message_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->msg_count;
}

LinkQualityInfo MavlinkAdapter::link_quality() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->link_quality.snapshot();
}



// ================================================================
// 心跳看门狗
// ================================================================

// 计算距上次收到心跳的毫秒数
uint64_t MavlinkAdapter::heartbeat_age_ms() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    if (impl_->tel.last_heartbeat_ms == 0) return 0;
    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return now - impl_->tel.last_heartbeat_ms;
}

// 检查心跳是否存活（在超时范围内）
bool MavlinkAdapter::heartbeat_alive(uint64_t timeout_ms) const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    if (impl_->tel.last_heartbeat_ms == 0) return false;
    uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    uint64_t age = now - impl_->tel.last_heartbeat_ms;
    return age <= timeout_ms;
}
std::optional<SysStatusInfo> MavlinkAdapter::latest_sys_status() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.sys_status;
}

std::optional<GpsInfo> MavlinkAdapter::latest_gps() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.gps;
}


// ================================================================
// ACK 等待机制（QGC MavCommandQueue 模式）
// ================================================================

// 清除待处理的 ACK 状态
void MavlinkAdapter::clear_pending_ack() {
    mg::core::platform::LockGuard lk(impl_->mtx);
    impl_->ack_waiter.ack_received = false;
    impl_->ack_waiter.pending.reset();
}

// 等待特定命令的 ACK，使用条件变量超时机制
std::optional<CommandAckInfo> MavlinkAdapter::wait_for_ack(uint16_t command, int timeout_ms) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    // 自动清除旧状态，避免第二次调用复用过期 ACK
    if (impl_->ack_waiter.pending.has_value() && impl_->ack_waiter.pending->command != command) {
        impl_->ack_waiter.ack_received = false;
        impl_->ack_waiter.pending.reset();
    }

    impl_->ack_waiter.cv.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&]() {
        if (impl_->ack_waiter.ack_received && impl_->ack_waiter.pending.has_value()) {
            return impl_->ack_waiter.pending->command == command;
        }
        return false;
    });

    if (impl_->ack_waiter.ack_received && impl_->ack_waiter.pending.has_value() &&
        impl_->ack_waiter.pending->command == command) {
        auto result = impl_->ack_waiter.pending;
        impl_->ack_waiter.ack_received = false;
        impl_->ack_waiter.pending.reset();
        return result;
    }
    return std::nullopt;
}

// ================================================================
// 命令队列 API 实现
// ================================================================

// 阻塞等待命令完成
std::optional<CommandAckInfo> MavlinkAdapter::wait_for_command(CommandTicket ticket, int timeout_ms) {
    mg::core::platform::UniqueLock lk(impl_->mtx);

    impl_->cmd_queue.cv.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&]() {
        int idx = impl_->cmd_queue.find_by_id(ticket.id);
        if (idx < 0) return true;  // 条目已移除
        auto state = impl_->cmd_queue.entries[idx].state;
        return state == CommandQueueEntry::State::Acked ||
               state == CommandQueueEntry::State::TimedOut;
    });

    int idx = impl_->cmd_queue.find_by_id(ticket.id);
    if (idx >= 0) {
        auto& e = impl_->cmd_queue.entries[idx];
        if (e.state == CommandQueueEntry::State::Acked ||
            e.state == CommandQueueEntry::State::TimedOut) {
            CommandAckInfo info{};
            info.command = e.command;
            info.result = e.ack_result;
            info.progress = e.ack_progress;
            if (e.timed_out) {
                info.result_text = "command ack timeout";
            }
            return info;
        }
    }
    return std::nullopt;
}

// 取消待处理命令
bool MavlinkAdapter::cancel_command(CommandTicket ticket) {
    mg::core::platform::LockGuard lk(impl_->mtx);
    for (auto it = impl_->cmd_queue.entries.begin(); it != impl_->cmd_queue.entries.end(); ++it) {
        if (it->id == ticket.id &&
            (it->state == CommandQueueEntry::State::Pending ||
             it->state == CommandQueueEntry::State::InProgress)) {
            impl_->cmd_queue.entries.erase(it);
            return true;
        }
    }
    return false;
}

// 周期性检查超时
void MavlinkAdapter::tick() {
    std::vector<std::function<void()>> callbacks;
    {
        mg::core::platform::LockGuard lk(impl_->mtx);
        impl_->process_timeouts();
        callbacks = std::move(impl_->deferred_cbs);
        impl_->deferred_cbs.clear();
    }
    for (auto& callback : callbacks) callback();
}

std::vector<VideoStreamInfo> MavlinkAdapter::latest_video_streams() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->tel.video_streams;
}

} // namespace mg::core::mavlink
