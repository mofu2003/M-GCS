#pragma once

// MAVLink 协议适配器，负责 MAVLink 消息的编解码、遥测缓存和命令发送

#include "core/core_types.h"
#include "core/core_status.h"
#include "core/mission/mission_types.h"
#include "core/firmware/firmware_adapter.h"
#include "core/ftp/mavlink_ftp.h"
// 内部 helper 的签名需要 UniqueLock（发送时临时解锁，见 ADR-0005）。
#include "core/platform/platform_mutex.h"
#include <array>
#include <functional>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace mg::core::settings { class MavlinkLogWriter; }

namespace mg::core::mavlink {

// Forward declarations for internal types (used in private method signatures)
struct MissionTransferState;

/// 向传输层发送原始 MAVLink 字节的回调
using SendCallback = std::function<bool(const uint8_t* data, size_t size)>;

/// 心跳消息信息（msg id 0）
struct HeartbeatInfo {
    uint8_t system_id = 0;
    uint8_t component_id = 0;
    uint8_t autopilot_type = 0;
    uint8_t vehicle_type = 0;
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
    uint8_t system_status = 0;
    bool armed = false;
    char flight_mode[24] = {0};   // Decoded flight mode name
};

/// MAVLink signing 自动探测候选项。name 只用于 UI/持久化归属，不参与 HMAC。
struct SigningCandidate {
    std::string name;
    std::array<uint8_t, 32> key{};
    uint64_t persisted_timestamp = 0;
};

using SigningDetectedCallback = std::function<void(const SigningCandidate&)>;

/// 活动飞控 MAVLink 接收链路的短窗口质量统计。
struct LinkQualityInfo {
    bool measuring = true;
    uint32_t received = 0;
    uint32_t lost = 0;
    uint8_t quality_pct = 0;
};

/// 系统状态消息信息（msg id 1）
struct SysStatusInfo {
    uint8_t system_id = 0;
    uint32_t onboard_control_sensors_present = 0;
    uint32_t onboard_control_sensors_enabled = 0;
    uint32_t onboard_control_sensors_health = 0;
    int16_t battery_voltage = 0;
    int16_t battery_current = 0;
    int8_t battery_remaining = -1;
};

/// 原始 GPS 信息（msg id 24）
struct GpsInfo {
    uint8_t system_id = 0;
    int32_t lat = 0;
    int32_t lon = 0;
    int32_t alt = 0;
    uint8_t fix_type = 0;
    uint8_t satellites_visible = 0;
};

/// 缩放 IMU 数据（msg id 26 / 116 / 129）— 加速度计、陀螺仪、磁力计
struct ImuInfo {
    uint8_t system_id = 0;
    /// IMU 实例号，0 对应 IMU1。SCALED_IMU/2/3 分别是 0/1/2。
    uint8_t instance = 0;
    // Accelerometer (mG)
    int16_t xacc = 0;
    int16_t yacc = 0;
    int16_t zacc = 0;
    // Gyroscope (mrad/s)
    int16_t xgyro = 0;
    int16_t ygyro = 0;
    int16_t zgyro = 0;
    // Magnetometer (mgauss)
    int16_t xmag = 0;
    int16_t ymag = 0;
    int16_t zmag = 0;
    // Temperature (cdegC)
    int16_t temperature = 0;
};

/// 气压计数据（msg id 29）
struct PressureInfo {
    uint8_t system_id = 0;
    float press_abs = 0.0f;       // hPa
    float press_diff = 0.0f;      // hPa
    int16_t temperature = 0;      // cdegC
};

/// 姿态信息（msg id 30）
struct AttitudeInfo {
    uint8_t system_id = 0;
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float rollspeed = 0.0f;
    float pitchspeed = 0.0f;
    float yawspeed = 0.0f;
};

/// VFR HUD 飞行仪表数据（msg id 74）— 空速、地速、高度、航向、油门
struct VfrHudInfo {
    uint8_t system_id = 0;
    float airspeed = 0.0f;       // m/s
    float groundspeed = 0.0f;    // m/s
    float alt = 0.0f;            // m MSL
    float climb = 0.0f;          // m/s
    int16_t heading = 0;         // deg
    uint16_t throttle = 0;       // %
};

/// 飞控固件版本信息（msg id 148）
struct FirmwareVersionInfo {
    uint8_t system_id = 0;
    uint8_t component_id = 0;
    uint8_t firmware_major = 0;
    uint8_t firmware_minor = 0;
    uint8_t firmware_patch = 0;
    uint8_t firmware_type = 0;
    uint64_t capabilities = 0;
    uint16_t vendor_id = 0;
    uint16_t product_id = 0;
    uint64_t uid = 0;
    char git_hash[17] = {0};
    bool received = false;
};


/// 飞控状态文本消息（msg id 253）
struct StatustextInfo {
    uint8_t system_id = 0;
    uint8_t severity = 0;          // MAV_SEVERITY_xxx (0=emergency, 6=info, 7=debug)
    char text[51] = {0};           // max 50 chars + null
};

/// 全局位置融合估计（msg id 33）
struct GlobalPositionInfo {
    uint8_t system_id = 0;
    int32_t lat = 0;               // degE7
    int32_t lon = 0;               // degE7
    int32_t alt = 0;               // mm MSL
    int32_t relative_alt = 0;      // mm AGL
    int16_t vx = 0;                // cm/s (north)
    int16_t vy = 0;                // cm/s (east)
    int16_t vz = 0;                // cm/s (down)
    uint16_t hdg = 0;              // cdeg (0-35999), UINT16_MAX = unknown
};

/// 飞控返航点位置（msg id 242）
struct HomePositionInfo {
    uint8_t system_id = 0;
    int32_t latitude = 0;          // degE7
    int32_t longitude = 0;         // degE7
    int32_t altitude = 0;          // mm MSL
    float x = 0.0f;               // local frame (m)
    float y = 0.0f;               // local frame (m)
    float z = 0.0f;               // local frame (m)
    float approach_x = 0.0f;      // approach vector (m)
    float approach_y = 0.0f;      // approach vector (m)
    float approach_z = 0.0f;      // approach vector (m)
};
/// 参数值信息（msg id 22）
struct ParamInfo {
    std::string name;
    float value = 0.0f;
    uint8_t type = 0;
    uint16_t index = 0;
    uint16_t count = 0;
};


/// 排队命令的最终去向。
///
/// 单看 `result` 无法把"链路层没发出去"或"等 ACK 超时"和"飞控明确拒绝"
/// 区分开：三种情况历史上都会落到 MAV_RESULT_FAILED(4)。调用方必须按本枚举
/// 分支，不要再解析 `result_text` 里的字符串（plan 阶段 1B）。
enum class CommandOutcome : uint8_t {
    Accepted,     // MAV_RESULT_ACCEPTED
    Rejected,     // 飞控回了非 ACCEPTED 的 COMMAND_ACK
    InProgress,   // MAV_RESULT_IN_PROGRESS，仍在执行
    Timeout,      // 重试预算内没有收到 COMMAND_ACK（含迟到 ACK 隔离窗口）
    SendFailed,   // 链路层发送失败，命令没有离开本机
};

/// 稳定符号名，作为 FFI 响应里的 `outcome` 字段（小写 snake_case）。
inline const char* command_outcome_name(CommandOutcome outcome) {
    switch (outcome) {
        case CommandOutcome::Accepted:   return "accepted";
        case CommandOutcome::Rejected:   return "rejected";
        case CommandOutcome::InProgress: return "in_progress";
        case CommandOutcome::Timeout:    return "timeout";
        case CommandOutcome::SendFailed: return "send_failed";
    }
    return "unknown";
}

/// 命令确认信息（msg id 77）— 命令执行结果
struct CommandAckInfo {
    uint8_t result = 0;          // MAV_RESULT_xxx
    uint16_t command = 0;        // MAV_CMD_xxx that was acknowledged
    uint8_t progress = 0;        // progress (0-100) for IN_PROGRESS
    std::string result_text;     // optional text
    CommandOutcome outcome = CommandOutcome::Accepted;  // 机器可读去向
};

/// 飞控主动发出的 COMMAND_LONG。用于少数由飞控驱动的交互流程，
/// 例如 ArduPilot 六面加速度计校准的姿态请求。
struct CommandLongInfo {
    uint8_t system_id = 0;
    uint8_t component_id = 0;
    uint8_t target_system = 0;
    uint8_t target_component = 0;
    uint16_t command = 0;
    float param1 = 0.0f;
};

/// 罗盘校准进度（msg id 191，ArduPilot MAV_CAL 协议）。
struct MagCalibrationProgressInfo {
    uint8_t system_id = 0;
    uint8_t compass_id = 0;
    uint8_t cal_mask = 0;
    uint8_t cal_status = 0;
    uint8_t attempt = 0;
    uint8_t completion_pct = 0;
};

/// 罗盘校准结果（msg id 192，ArduPilot MAV_CAL 协议）。
struct MagCalibrationReportInfo {
    uint8_t system_id = 0;
    uint8_t compass_id = 0;
    uint8_t cal_mask = 0;
    uint8_t cal_status = 0;
    bool autosaved = false;
    float fitness = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float offset_z = 0.0f;
    float diagonal_x = 0.0f;
    float diagonal_y = 0.0f;
    float diagonal_z = 0.0f;
    float offdiag_x = 0.0f;
    float offdiag_y = 0.0f;
    float offdiag_z = 0.0f;
};

/// 命令队列票据 — 用于跟踪已入队命令的状态
struct CommandTicket {
    uint64_t id = 0;        // 队列条目 ID
    uint16_t command = 0;   // MAV_CMD_xxx
};

/// 遥控通道数据（msg id 65）— 18 路原始 RC 输入
struct RcChannelsInfo {
    uint8_t system_id = 0;
    uint32_t time_boot_ms = 0;
    uint16_t chan[18] = {};       // Raw PWM values (us), 0 = not available
    uint8_t chancount = 0;        // Total number of active channels
    uint8_t rssi = 0;             // 0-254, 255 = unknown
};

/// 舵机/电机原始输出值（msg id 36）
struct ServoOutputInfo {
    uint8_t system_id = 0;
    uint32_t time_usec = 0;
    uint16_t servo[16] = {};      // Raw PWM output values (us)
    uint8_t port = 0;             // ArduPilot: port 0 = outputs 1-16, port 1 = 17-32
};

/// 电池状态信息（msg id 147）— 详细电池数据
struct BatteryInfo {
    uint8_t system_id = 0;
    int8_t  battery_remaining = -1;  // -1 = unknown
    float   voltage_v = 0.0f;        // Volts
    float   current_a = 0.0f;        // Amps (negative = discharging)
    float   consumed_mah = 0.0f;     // mAh consumed
    float   time_remaining_s = 0.0f; // seconds remaining
    int8_t  temperature_c = -127;    // degC, -127 = unknown
    uint8_t function = 0;            // MAV_BATTERY_FUNCTION
    uint8_t type = 0;                // MAV_BATTERY_TYPE
    int8_t  charge_state = 0;        // MAV_BATTERY_CHARGE_STATE
};

/// 高分辨率 IMU 数据（msg id 105）
struct HighresImuInfo {
    uint8_t system_id = 0;
    /// IMU 实例号，来自报文的 id 字段（0 对应 IMU1）。
    uint8_t instance = 0;
    float xacc = 0.0f;          // m/s^2
    float yacc = 0.0f;
    float zacc = 0.0f;
    float xgyro = 0.0f;         // rad/s
    float ygyro = 0.0f;
    float zgyro = 0.0f;
    float xmag = 0.0f;          // Gauss
    float ymag = 0.0f;
    float zmag = 0.0f;
    float abs_pressure = 0.0f;  // hPa
    float diff_pressure = 0.0f; // hPa
    float pressure_alt = 0.0f;  // Altitude from pressure
    float temperature = 0.0f;   // degC
    uint32_t fields_updated = 0; // Bitmask of which fields have been updated
};

/// 扩展系统状态（msg id 324）— VTOL 状态、着陆状态
struct ExtendedSysStateInfo {
    uint8_t system_id = 0;
    uint8_t vtol_state = 0;     // MAV_VTOL_STATE
    uint8_t landed_state = 0;   // MAV_LANDED_STATE
};

/// 当前任务航点序列号（msg id 42）
struct MissionCurrentInfo {
    uint8_t  system_id = 0;
    uint16_t seq = 0;           // Current mission item sequence number
    uint8_t  total = 0;         // Total mission items (ArduPilot 4.0+)
    uint8_t  mission_mode = 0;  // Mission mode (ArduPilot 4.0+)
};


/// 起飞前检查状态（msg id 415）— 检查失败位掩码
struct PrearmCheckInfo {
    uint8_t  system_id = 0;
    uint32_t check_mask = 0;
};

/// 振动水平和裁剪计数（msg id 241）
struct VibrationInfo {
    uint8_t  system_id = 0;
    float    vibration_x = 0;
    float    vibration_y = 0;
    float    vibration_z = 0;
    uint32_t clipping_0 = 0;
    uint32_t clipping_1 = 0;
    uint32_t clipping_2 = 0;
};

/// 电调遥测数据（msg id 291）— 单个 ESC/电机遥测
struct EscStatusInfo {
    uint8_t  system_id = 0;
    uint8_t  index = 0;
    uint8_t  count = 0;
    uint32_t error_count = 0;
    float    rpm = 0;
    float    voltage = 0;
    float    current = 0;
    float    temperature = 0;
};

/// 风速风向估计（msg id 231）
struct WindEstimateInfo {
    uint8_t  system_id = 0;
    float    wind_x = 0;
    float    wind_y = 0;
    float    wind_z = 0;
    float    var_horiz = 0;
    float    var_vert = 0;
};

/// 距离传感器/近距传感器数据（msg id 132）
struct DistanceSensorInfo {
    uint8_t  system_id = 0;
    uint8_t  id = 0;              // Sensor ID
    uint8_t  orientation = 0;     // MAV_SENSOR_ORIENTATION
    uint16_t current_distance = 0; // cm, UINT16_MAX if no reading
    uint16_t min_distance = 0;    // cm
    uint16_t max_distance = 0;    // cm
    uint8_t  type = 0;            // MAV_DISTANCE_SENSOR
    uint8_t  covariance = 0;      // measurement variance, 0 = unknown
};

/// ADS-B 跟踪飞行器信息（msg id 246）
struct AdsbVehicleInfo {
    uint32_t icao_address = 0;
    int32_t  lat = 0;             // degE7
    int32_t  lon = 0;             // degE7
    int32_t  altitude = 0;        // mm (altitude_type dependent)
    uint16_t heading = 0;         // centidegrees
    uint16_t hor_velocity = 0;    // cm/s
    int16_t  ver_velocity = 0;    // cm/s
    char     callsign[10] = {};
    uint8_t  emitter_type = 0;
    uint8_t  tslc = 0;            // time since last comm (seconds)
    uint16_t flags = 0;
    uint16_t squawk = 0;
};

/// 视频流信息（msg id 269）— VIDEO_STREAM_INFORMATION
struct VideoStreamInfo {
    uint8_t  stream_id = 0;
    uint8_t  type = 0;            // VIDEO_STREAM_TYPE
    uint16_t width = 0;
    uint16_t height = 0;
    float    framerate = 0;
    uint32_t bitrate = 0;
    uint16_t rotation = 0;
    uint16_t hfov = 0;
    uint16_t flags = 0;           // bit 0: running
    char     name[32] = {};
    char     uri[160] = {};
};

/// 视频流状态（msg id 270）— VIDEO_STREAM_STATUS
/// 与 VIDEO_STREAM_INFORMATION 按 (system_id, component_id, stream_id) 配对。
struct VideoStreamStatusInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint8_t  stream_id = 0;
    uint16_t framerate = 0;
    uint32_t bitrate = 0;
    uint16_t flags = 0;           // bit 0: running
    uint16_t resolution_h = 0;
    uint16_t resolution_v = 0;
    uint16_t rotation = 0;
    uint16_t hfov = 0;
    uint8_t  camera_device_id = 0;
};

/// 相机信息（msg id 259）— CAMERA_INFORMATION
struct CameraInformationInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    uint32_t firmware_version = 0;
    float    focal_length = 0;
    float    sensor_size_h = 0;
    float    sensor_size_v = 0;
    uint32_t flags = 0;           // CAMERA_CAP_FLAGS
    uint16_t resolution_h = 0;
    uint16_t resolution_v = 0;
    uint16_t cam_definition_version = 0;
    char     vendor_name[32] = {};
    char     model_name[32] = {};
    uint8_t  lens_id = 0;
    uint8_t  gimbal_device_id = 0;
    uint8_t  camera_device_id = 0;
};

/// 相机设置（msg id 260）— CAMERA_SETTINGS
struct CameraSettingsInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    uint8_t  mode_id = 0;         // CAMERA_MODE
    float    zoom_level = 1.0f;
    float    focus_level = 0;
    uint8_t  camera_device_id = 0;
};

/// 相机拍摄状态（msg id 262）— CAMERA_CAPTURE_STATUS
struct CameraCaptureStatusInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    float    image_interval = 0;
    uint32_t recording_time_ms = 0;
    float    available_capacity = 0;  // MiB
    uint8_t  image_status = 0;        // CAMERA_CAPTURE_STATUS image_status
    uint8_t  video_status = 0;        // CAMERA_CAPTURE_STATUS video_status
    uint32_t image_count = 0;
    uint8_t  camera_device_id = 0;
};

/// 存储信息（msg id 261）— STORAGE_INFORMATION
struct StorageInformationInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    float    total_capacity = 0;      // MiB
    float    used_capacity = 0;
    float    available_capacity = 0;
    float    read_speed = 0;          // MiB/s
    float    write_speed = 0;
    uint8_t  storage_id = 0;
    uint8_t  storage_count = 0;
    uint8_t  status = 0;              // STORAGE_STATUS
    uint8_t  type = 0;                // STORAGE_TYPE
    char     name[32] = {};
    uint8_t  storage_usage = 0;       // STORAGE_USAGE_FLAG
};

/// 云台管理器信息（msg id 280）— GIMBAL_MANAGER_INFORMATION
struct GimbalManagerInformationInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    uint32_t cap_flags = 0;           // GIMBAL_MANAGER_CAP_FLAGS
    float    roll_min = 0, roll_max = 0;
    float    pitch_min = 0, pitch_max = 0;
    float    yaw_min = 0, yaw_max = 0;
    uint8_t  gimbal_device_id = 0;
};

/// 云台管理器状态（msg id 281）— GIMBAL_MANAGER_STATUS
struct GimbalManagerStatusInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    uint32_t flags = 0;
    uint8_t  gimbal_device_id = 0;
    uint8_t  primary_control_sysid = 0;
    uint8_t  primary_control_compid = 0;
    uint8_t  secondary_control_sysid = 0;
    uint8_t  secondary_control_compid = 0;
};

/// 云台设备信息（msg id 283）— GIMBAL_DEVICE_INFORMATION
struct GimbalDeviceInformationInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint64_t uid = 0;                 // 官方定义为 uint64_t（曾误写成 uint8_t，会截断）
    uint32_t time_boot_ms = 0;
    uint32_t firmware_version = 0;
    uint32_t hardware_version = 0;
    float    roll_min = 0, roll_max = 0;
    float    pitch_min = 0, pitch_max = 0;
    float    yaw_min = 0, yaw_max = 0;
    uint16_t cap_flags = 0;
    uint16_t custom_cap_flags = 0;
    char     vendor_name[32] = {};
    char     model_name[32] = {};
    char     custom_name[32] = {};
    uint8_t  gimbal_device_id = 0;
    uint32_t cap_flags2 = 0;          // 官方定义为 uint32_t（曾误写成 uint16_t，会截断）
};

/// 云台设备姿态状态（msg id 285）— GIMBAL_DEVICE_ATTITUDE_STATUS
struct GimbalDeviceAttitudeStatusInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t time_boot_ms = 0;
    float    q[4] = {1.0f, 0.0f, 0.0f, 0.0f};  // 姿态四元数 (w,x,y,z)
    float    angular_velocity_x = 0, angular_velocity_y = 0, angular_velocity_z = 0;
    uint16_t failure_flags = 0;
    uint32_t flags = 0;               // GIMBAL_DEVICE_FLAGS
    uint8_t  target_system = 0;
    uint8_t  target_component = 0;
    float    delta_yaw = 0;
    float    delta_yaw_velocity = 0;
    uint8_t  gimbal_device_id = 0;
};

// 各消息类型的回调函数类型定义
using HeartbeatCallback       = std::function<void(const HeartbeatInfo&)>;
using SysStatusCallback       = std::function<void(const SysStatusInfo&)>;
using GpsCallback             = std::function<void(const GpsInfo&)>;
using ImuCallback             = std::function<void(const ImuInfo&)>;
using PressureCallback        = std::function<void(const PressureInfo&)>;
using AttitudeCallback        = std::function<void(const AttitudeInfo&)>;
using VfrHudCallback          = std::function<void(const VfrHudInfo&)>;
using FirmwareVersionCallback = std::function<void(const FirmwareVersionInfo&)>;
using ParamCallback           = std::function<void(const ParamInfo&)>;
using CommandAckCallback      = std::function<void(const CommandAckInfo&)>;
using CommandLongCallback     = std::function<void(const CommandLongInfo&)>;
using MagCalibrationProgressCallback = std::function<void(const MagCalibrationProgressInfo&)>;
using MagCalibrationReportCallback = std::function<void(const MagCalibrationReportInfo&)>;
using RcChannelsCallback     = std::function<void(const RcChannelsInfo&)>;
using ServoOutputCallback    = std::function<void(const ServoOutputInfo&)>;
using StatustextInfoCallback    = std::function<void(const StatustextInfo&)>;
using GlobalPositionCallback     = std::function<void(const GlobalPositionInfo&)>;
using HomePositionCallback       = std::function<void(const HomePositionInfo&)>;
using StatustextCallback = std::function<void(const std::string& text, uint8_t severity)>;
using BatteryCallback            = std::function<void(const BatteryInfo&)>;
using HighresImuCallback         = std::function<void(const HighresImuInfo&)>;
using ExtendedSysStateCallback   = std::function<void(const ExtendedSysStateInfo&)>;
using PrearmCheckCallback      = std::function<void(const PrearmCheckInfo&)>;
using VibrationCallback        = std::function<void(const VibrationInfo&)>;
using EscStatusCallback        = std::function<void(const EscStatusInfo&)>;
using WindEstimateCallback     = std::function<void(const WindEstimateInfo&)>;
using DistanceSensorCallback   = std::function<void(const DistanceSensorInfo&)>;
using AdsbVehicleCallback      = std::function<void(const AdsbVehicleInfo&)>;
using VideoStreamCallback      = std::function<void(const VideoStreamInfo&)>;
using MissionReachedCallback   = std::function<void(uint16_t seq)>;

// MAVLink 协议适配器核心类，封装消息解析、遥测缓存、命令发送和任务协议
class MavlinkAdapter {
public:
    MavlinkAdapter();
    ~MavlinkAdapter();

    // 喂入原始字节流进行 MAVLink 解析
    /// Feed raw bytes and return whether at least one valid MAVLink frame was parsed.
    /// The return value lets the connection watchdog distinguish real protocol
    /// traffic from random bytes/noise on a serial or UDP transport.
    bool feed_raw(const uint8_t* data, size_t size);
    // 设置底层发送回调
    void set_send_callback(SendCallback cb);

    /// 绑定 .tlog 录制 sink（由 Vehicle 绑定到自己的 MavlinkLogWriter）。
    /// 传入 nullptr 解绑。录制写盘只在所有 adapter 锁之外发生，见 ADR-0006。
    void set_log_sink(mg::core::settings::MavlinkLogWriter* writer);

    // MAVLink signing。key 必须是 32 字节；状态只属于当前适配器，
    // 不写入生成库的全局 MAVLINK_COMM_0。
    bool set_signing_key(const uint8_t* key, size_t key_size,
                         bool sign_outgoing = true,
                         uint64_t persisted_timestamp = 0,
                         const std::string& key_name = {},
                         bool auto_detected = false);
    /// 设置自动探测候选密钥。收到已签名但当前未启用签名的包时，按 HMAC
    /// 逐个匹配；命中后自动启用并保留命中的名称。
    bool set_signing_candidates(const std::vector<SigningCandidate>& candidates);
    void clear_signing_candidates();
    void set_signing_detected_callback(SigningDetectedCallback callback);
    void clear_signing_key();
    bool signing_enabled() const;
    mavlink_signing_status_t signing_status() const;
    std::string signing_key_name() const;
    bool signing_auto_detected() const;
    size_t signing_candidate_count() const;
    uint64_t signing_timestamp() const;
    // 发送 SETUP_SIGNING；该配置消息始终以未签名方式发送。
    bool send_setup_signing(uint8_t target_system,
                            uint8_t target_component,
                            const uint8_t* key,
                            size_t key_size,
                            uint64_t initial_timestamp = 0);

    // 发送 GCS 心跳包
    bool send_heartbeat();
    // 请求数据流（旧方式）
    bool request_data_stream(uint8_t target_system, uint8_t target_component,
                             uint8_t stream_id, uint16_t rate_hz);
    // 请求飞控版本信息
    bool request_autopilot_version(uint8_t target_system, uint8_t target_component);
    /// 请求飞控上报 home 点（MAV_CMD_GET_HOME_POSITION）。
    ///
    /// ArduPilot 只在首次上电和每次解锁时发一次 HOME_POSITION，之后不再流式发送，
    /// 所以 GCS 连晚了就永远收不到。QGC 的注释与做法见其 APMFirmwarePlugin。
    bool request_home_position(uint8_t target_system, uint8_t target_component);
    // 请求完整参数列表
    bool request_param_list(uint8_t target_system, uint8_t target_component);
    // 按名称请求单个参数
    bool request_param_read_by_name(uint8_t target_system, uint8_t target_component,
                                    const char* param_id);

    /// 向飞控发送 PARAM_SET 以修改参数值
    /// Returns true if the message was sent successfully.
    /// The FC will respond with PARAM_VALUE confirming the new value.
    bool set_param(uint8_t target_system, uint8_t target_component,
                   const char* param_id, float param_value, uint8_t param_type);


    /// 设置消息发送间隔（现代替代 REQUEST_DATA_STREAM，2015 年后弃用）
    /// msg_id: MAVLink message ID (e.g. MAVLINK_MSG_ID_ATTITUDE = 30)
    /// interval_us: interval in microseconds (e.g. 100000 = 10Hz). -1 = disable, 0 = default.
    /// Returns true if the command was sent successfully.
    bool request_message_interval(uint8_t target_system, uint8_t target_component,
                                  uint32_t msg_id, int32_t interval_us);
    // ---- 命令发送的返回值语义（safety.md S2）----
    // 下面这一组命令发送接口的 bool 只表示“已交给发送层”，不代表飞控接受。
    // 判断成功必须匹配对应的 COMMAND_ACK：用 send_command_queued /
    // send_command_int_queued 拿 ticket，再 wait_for_command() 等 ACK。
    // 界面不得把“已发送”显示为成功，所以生产路径一律走 FFI 的 submit/poll。

    /// 发送 COMMAND_LONG 到飞控
    bool send_command_long(uint8_t target_system, uint8_t target_component,
                           uint16_t command, uint8_t confirmation,
                           float p1, float p2, float p3, float p4,
                           float p5, float p6, float p7);

    /// 解锁/锁定电机，force=true 跳过安全检查
    bool arm_disarm(uint8_t target_system, uint8_t target_component, bool arm, bool force = false);

    /// 通过 MAV_CMD_DO_SET_MODE 设置飞行模式
    bool set_mode(uint8_t target_system, uint8_t target_component, uint8_t base_mode, uint32_t custom_mode);

    /// 返航
    bool cmd_return_to_launch(uint8_t target_system, uint8_t target_component);

    /// 起飞到指定高度（米，相对地面）
    bool cmd_takeoff(uint8_t target_system, uint8_t target_component, float altitude_m);

    /// 在当前位置降落
    bool cmd_land(uint8_t target_system, uint8_t target_component);

    /// 紧急停机 — 立即切断电机

    // ================================================================
    // GUIDED 模式指令
    // ================================================================

    /// GUIDED: 飞往指定位置（经纬度+高度），使用 MAV_CMD_DO_REPOSITION
    /// lat_deg, lon_deg: 目标位置（度）。alt_m: 目标高度（MSL 米）。
    /// yaw_deg: 期望航向（-1 表示保持当前航向）
    bool cmd_guided_point(uint8_t target_system, uint8_t target_component,
                          double lat_deg, double lon_deg, float alt_m,
                          float yaw_deg = -1.0f, float speed_ms = 0.0f);

    /// GUIDED: 以指定速度矢量飞行（NED 坐标系: +x=北, +y=东, +z=下）
    bool cmd_guided_velocity(uint8_t target_system, uint8_t target_component,
                             float vx_ms, float vy_ms, float vz_ms);

    /// GUIDED: 暂停（悬停在当前位置）
    bool cmd_guided_pause(uint8_t target_system, uint8_t target_component);

    /// GUIDED: 在当前位置改变高度（QGC `guidedModeChangeAltitude`）
    ///
    /// `altitude_change_m` 为正表示上升。QGC 用 LOCAL_OFFSET_NED 的 position 偏移，
    /// type_mask=0xFFF8，只动 z。
    bool cmd_change_altitude(uint8_t target_system, uint8_t target_component,
                             float altitude_change_m);

    /// 绕指定点以给定半径和速度绕圈（QGC `Vehicle::guidedModeOrbit`）
    ///
    /// COMMAND_INT + `MAV_FRAME_GLOBAL`，`altitude_m` 是 AMSL；`velocity_ms <= 0`
    /// 时发 NaN 让飞控用默认速度，偏航行为固定发 `ORBIT_YAW_BEHAVIOUR_UNCHANGED`。
    bool cmd_orbit(uint8_t target_sys, uint8_t target_comp,
                   double lat, double lon, float radius_m, float velocity_ms, float altitude_m);

    /// 设置感兴趣区域位置
    bool cmd_set_roi(uint8_t target_sys, uint8_t target_comp, double lat, double lon, float alt);

    /// 取消感兴趣区域
    bool cmd_roi_stop(uint8_t target_sys, uint8_t target_comp);

    /// 设置航向（QGC `APMFirmwarePlugin::guidedModeChangeHeading` 用的
    /// MAV_CMD_CONDITION_YAW）：偏转角、最大偏航速率、方向、是否相对当前航向。
    /// 仅多旋翼/潜航器支持，固定翼与 VTOL 在 QGC 里不提供这一项。
    bool cmd_change_heading(uint8_t target_sys, uint8_t target_comp,
                            float heading_deg, float yaw_rate_dps,
                            int8_t direction, bool relative);

    /// 收放起落架
    bool cmd_landing_gear(uint8_t target_sys, uint8_t target_comp, bool deploy);


    // ================================================================
    // Mission protocol
    // ================================================================
    
    /// Download mission from FC (async - check mission_progress())
    bool mission_download(uint8_t target_system, uint8_t target_component);
    
    /// Upload local mission plan to FC (async - check mission_progress())
    bool mission_upload(uint8_t target_system, uint8_t target_component,
                        const mg::core::mission::MissionPlan& plan);

    /// Cancel the active mission read/write transaction.
    /// The local transfer state is reset, and an explicit
    /// MAV_MISSION_OPERATION_CANCELLED ACK is sent to the vehicle.
    bool mission_cancel();
    
    /// Clear all mission items on FC
    bool mission_clear(uint8_t target_system, uint8_t target_component);
    
    /// Set current waypoint by sequence number
    bool mission_set_current(uint8_t target_system, uint8_t target_component,
                             uint16_t seq);
    
    /// Get the current mission plan (thread-safe snapshot)
    mg::core::mission::MissionPlan mission_get_plan() const;
    
    /// Get current transfer progress
    mg::core::mission::MissionTransferProgress mission_get_progress() const;
    int mission_transfer_retry_count() const;

    /// Get provenance for the cached mission plan.
    mg::core::mission::MissionPlanProvenance mission_get_plan_provenance() const;

    /// Get the cached mission plan and provenance atomically.
    mg::core::mission::MissionPlanSnapshot mission_get_plan_snapshot() const;

    /// 任务执行进度：MISSION_CURRENT 给出当前目标项，MISSION_ITEM_REACHED 统计已到达项。
    /// `current_seq` / `last_reached_seq` 是飞控原始 seq（ArduPilot 含第 0 项 home）。
    struct MissionExecutionProgress {
        bool valid = false;         // 飞控任务已缓存，可以判断进度
        uint16_t current_seq = 0;   // MISSION_CURRENT 的 seq
        uint32_t total = 0;         // 缓存任务项总数
        uint16_t reached_count = 0; // 已到达项数量
        uint16_t last_reached_seq = 0;
        bool has_reached = false;   // 本次任务是否收到过 MISSION_ITEM_REACHED
        // 最后一项已到达。只代表"航段飞完"，固定翼此时还在返航/降落，
        // 所以不能直接当成任务完成——完成判定见 FlightModeManager::mission_fly_completed()。
        bool reached_end = false;
    };

    MissionExecutionProgress mission_get_execution_progress() const;

    // ================================================================
    // Rally Points (MAV_MISSION_TYPE_RALLY = 2)
    // ================================================================

    /// Download rally points from FC (async - check rally_get_progress())
    bool rally_download(uint8_t target_system, uint8_t target_component);

    /// Upload rally points to FC (async - check rally_get_progress())
    bool rally_upload(uint8_t target_system, uint8_t target_component,
                      const mg::core::mission::MissionPlan& plan);

    /// Cancel an active rally transfer.
    bool rally_cancel();

    /// Clear all rally points on FC
    bool rally_clear(uint8_t target_system, uint8_t target_component);

    /// Get the current rally point plan (thread-safe snapshot)
    mg::core::mission::MissionPlan rally_get_plan() const;

    /// Get the cached rally plan and provenance atomically.
    mg::core::mission::MissionPlanSnapshot rally_get_plan_snapshot() const;

    /// Get rally point transfer progress
    mg::core::mission::MissionTransferProgress rally_get_progress() const;
    int rally_transfer_retry_count() const;

    // ================================================================
    // Geofence (MAV_MISSION_TYPE_FENCE = 1)
    // ================================================================

    /// Download geofence from FC (async - check fence_get_progress())
    bool fence_download(uint8_t target_system, uint8_t target_component);

    /// Upload geofence to FC (async - check fence_get_progress())
    bool fence_upload(uint8_t target_system, uint8_t target_component,
                      const mg::core::mission::MissionPlan& plan);

    /// Cancel an active geofence transfer.
    bool fence_cancel();

    /// Clear all geofence items on FC
    bool fence_clear(uint8_t target_system, uint8_t target_component);

    /// Get the current geofence plan (thread-safe snapshot)
    mg::core::mission::MissionPlan fence_get_plan() const;

    /// Get the cached geofence plan and provenance atomically.
    mg::core::mission::MissionPlanSnapshot fence_get_plan_snapshot() const;

    /// Get geofence transfer progress
    mg::core::mission::MissionTransferProgress fence_get_progress() const;
    int fence_transfer_retry_count() const;

    // ================================================================
    // Log List (LOG_REQUEST_LIST protocol)
    // ================================================================

    struct LogEntry {
        uint16_t id = 0;
        uint32_t time_utc = 0;
        uint32_t size_bytes = 0;
    };

    // 从飞控请求日志条目列表
    bool log_request_list(uint8_t target_system, uint8_t target_component);

    // 获取已接收的日志条目（线程安全快照）
    std::vector<LogEntry> log_get_entries() const;

    // 日志列表总条数。飞控在 LOG_ENTRY 的 num_logs 字段给出；0 表示未知
    // （尚无 LOG_ENTRY）。UI 用它判断列表是否收齐，而不是靠“一段时间没新条目”猜。
    uint16_t log_list_expected() const;
    /// 开始日志下载
    bool log_download_start(uint8_t target_system, uint8_t target_component,
                            uint16_t log_id, const std::string& filepath);

    /// 取消正在进行的日志下载
    bool log_download_cancel();

    /// 获取日志下载进度（0.0-1.0）
    float log_download_progress() const;

    /// 检查日志下载是否正在进行
    bool log_download_active() const;

    struct LogDownloadStatus {
        bool active = false;   // 正在接收 LOG_DATA
        bool saving = false;   // 字节已收齐，正在落盘（完成态尚未确定）
        bool completed = false;
        bool cancelled = false;
        uint16_t id = 0;
        uint32_t received_bytes = 0;
        uint32_t total_bytes = 0;
        bool size_declared = false;  // 大小来自 LOG_ENTRY（非 0），可作完成校验基准
        uint32_t saved_bytes = 0;  // 实际写入本地文件的字节数
        std::string filepath;
        std::string error;
    };

    LogDownloadStatus log_download_status();
    /// 开始执行任务（MAV_CMD_MISSION_START）
    bool mission_start(uint8_t target_system, uint8_t target_component);

    /// 请求日志数据下载
    bool log_request_data(uint8_t target_system, uint8_t target_component,
                          uint32_t id, uint32_t ofs, uint32_t count);

    /// 结束日志下载会话
    bool log_request_end(uint8_t target_system, uint8_t target_component);

    /// 擦除飞控上所有日志
    bool log_erase(uint8_t target_system, uint8_t target_component);

    /// 拍照（MAV_CMD_IMAGE_START_CAPTURE）
    bool camera_take_photo(uint8_t target_system, uint8_t target_component);

    /// 开始视频录制
    bool camera_start_video(uint8_t target_system, uint8_t target_component);

    /// 停止视频录制
    bool camera_stop_video(uint8_t target_system, uint8_t target_component);

    /// 设置相机变焦
    bool camera_set_zoom(uint8_t target_system, uint8_t target_component,
                         uint8_t zoom_type, float zoom_value);

    /// 请求相机信息
    bool camera_request_info(uint8_t target_system, uint8_t target_component);

    /// 设置相机模式（0=拍照, 1=录像）
    bool camera_set_mode(uint8_t target_system, uint8_t target_component,
                         uint8_t camera_mode);

    /// 停止拍照
    bool camera_stop_capture(uint8_t target_system, uint8_t target_component);

    /// 设置相机对焦
    bool camera_set_focus(uint8_t target_system, uint8_t target_component,
                          uint8_t focus_type, float focus_value);

    // ================================================================
    // 云台控制
    // ================================================================

    /// 云台俯仰偏航控制（MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW）
    bool cmd_gimbal_control(uint8_t target_system, uint8_t target_component,
                            float pitch_deg, float yaw_deg);

    /// 云台归中
    bool cmd_gimbal_recenter(uint8_t target_system, uint8_t target_component);

    /// 请求云台管理器信息
    bool gimbal_request_info(uint8_t target_system, uint8_t target_component);

    /// 云台配置（设置主控/副控）
    bool cmd_gimbal_configure(uint8_t target_system, uint8_t target_component,
                              uint8_t gimbal_device_id,
                              uint8_t primary_sys, uint8_t primary_comp,
                              uint8_t secondary_sys, uint8_t secondary_comp);

    /// 请求视频流信息
    bool video_request_stream_info(uint8_t target_system, uint8_t target_component);

    // 各消息类型的回调注册方法
    void on_heartbeat(HeartbeatCallback cb);
    void on_sys_status(SysStatusCallback cb);
    void on_gps(GpsCallback cb);
    void on_imu(ImuCallback cb);
    void on_pressure(PressureCallback cb);
    void on_attitude(AttitudeCallback cb);
    void on_vfr_hud(VfrHudCallback cb);
    void on_firmware_version(FirmwareVersionCallback cb);
    void on_param(ParamCallback cb);
    void on_command_ack(CommandAckCallback cb);
    void on_command_long(CommandLongCallback cb);
    void on_mag_cal_progress(MagCalibrationProgressCallback cb);
    void on_mag_cal_report(MagCalibrationReportCallback cb);
    void on_rc_channels(RcChannelsCallback cb);
    void on_servo_output(ServoOutputCallback cb);
    void on_statustext(StatustextCallback cb);
    void on_global_position(GlobalPositionCallback cb);
    void on_home_position(HomePositionCallback cb);
    void on_battery(BatteryCallback cb);
    void on_highres_imu(HighresImuCallback cb);
    void on_extended_sys_state(ExtendedSysStateCallback cb);
    void on_prearm_check(PrearmCheckCallback cb);
    void on_vibration(VibrationCallback cb);
    void on_esc_status(EscStatusCallback cb);
    void on_wind_estimate(WindEstimateCallback cb);
    void on_distance_sensor(DistanceSensorCallback cb);
    void on_adsb_vehicle(AdsbVehicleCallback cb);
    void on_video_stream_info(VideoStreamCallback cb);
    void on_mission_reached(MissionReachedCallback cb);

    // 获取最新遥测数据的线程安全快照
    std::optional<HeartbeatInfo>        latest_heartbeat() const;
    std::optional<AttitudeInfo>         latest_attitude() const;
    std::optional<VfrHudInfo>           latest_vfr_hud() const;
    std::optional<ImuInfo>              latest_imu() const;
    std::optional<PressureInfo>         latest_pressure() const;
    std::optional<FirmwareVersionInfo>  latest_firmware_version() const;

    std::optional<SysStatusInfo>        latest_sys_status() const;
    std::optional<GpsInfo>              latest_gps() const;

    std::optional<CommandAckInfo> latest_command_ack() const;
    std::optional<RcChannelsInfo>  latest_rc_channels() const;
    std::optional<StatustextInfo>     latest_statustext_info() const;
    std::optional<GlobalPositionInfo>  latest_global_position() const;
    std::optional<HomePositionInfo>    latest_home_position() const;
    std::optional<ServoOutputInfo> latest_servo_output() const;
    std::optional<BatteryInfo>         latest_battery() const;
    std::optional<HighresImuInfo>      latest_highres_imu() const;
    std::optional<ExtendedSysStateInfo> latest_extended_sys_state() const;
    std::optional<PrearmCheckInfo>    latest_prearm_check() const;
    std::optional<VibrationInfo>      latest_vibration() const;
    std::optional<EscStatusInfo>      latest_esc_status() const;
    std::optional<WindEstimateInfo>   latest_wind_estimate() const;

    /// 获取已发现的视频流列表
    std::vector<VideoStreamInfo>      latest_video_streams() const;
    /// 获取已上报的视频流状态（VIDEO_STREAM_STATUS）快照
    std::vector<VideoStreamStatusInfo> latest_video_stream_status() const;

    /// 获取已发现的相机（CAMERA_INFORMATION）列表
    std::vector<CameraInformationInfo> latest_cameras() const;
    /// 按 (系统, 组件) 取单个相机的信息/设置/拍摄状态；未收到返回 nullopt
    std::optional<CameraInformationInfo>   camera_information(uint8_t sysid, uint8_t compid) const;
    std::optional<CameraSettingsInfo>      camera_settings(uint8_t sysid, uint8_t compid) const;
    std::optional<CameraCaptureStatusInfo> camera_capture_status(uint8_t sysid, uint8_t compid) const;
    /// 某个相机组件上报的全部存储卷
    std::vector<StorageInformationInfo>    camera_storage(uint8_t sysid, uint8_t compid) const;

    /// 获取已发现的云台管理器（GIMBAL_MANAGER_INFORMATION）列表
    std::vector<GimbalManagerInformationInfo> latest_gimbals() const;
    std::optional<GimbalManagerInformationInfo>   gimbal_manager_information(uint8_t sysid, uint8_t compid) const;
    std::optional<GimbalManagerStatusInfo>        gimbal_manager_status(uint8_t sysid, uint8_t compid) const;
    std::optional<GimbalDeviceInformationInfo>    gimbal_device_information(uint8_t sysid, uint8_t compid) const;
    std::optional<GimbalDeviceAttitudeStatusInfo> gimbal_device_attitude_status(uint8_t sysid, uint8_t compid) const;
    std::string last_statustext() const;
    uint8_t last_statustext_severity() const;
    std::vector<ParamInfo> all_params() const;
    /// 从本地缓存按名称获取单个参数，未找到返回 nullopt
    std::optional<ParamInfo> get_param_by_name(const char* param_id) const;

    // 参数总数
    uint16_t param_count() const;
    // 已接收参数数量
    size_t params_received() const;
    // 参数下载状态机实际确认过的索引数量（取消后不再增长）
    size_t params_download_received() const;
    /// 参数下载是否仍在进行。
    bool param_download_active() const;
    /// 最近一次参数下载是否被用户取消。
    bool param_download_cancelled() const;
    /// 最近一次参数下载是否因超时/缺失失败。
    bool param_download_failed() const;
    /// 最近一次参数下载的终态错误文本。
    std::string param_download_error() const;
    /// 参数列表请求或补包阶段的重试次数。
    int param_download_retry_count() const;
    /// 当前仍缺失的参数数量。
    int param_download_missing_count() const;
    /// 补包耗尽后仍失败的参数数量。
    int param_download_failed_count() const;
    /// 取消当前参数下载；没有活动下载时返回 false。
    bool cancel_param_download();
    /// 最近一批参数写入的结果，按请求顺序从旧到新返回。
    struct ParamChangeRecord {
        uint64_t id = 0;
        std::string name;
        float requested_value = 0.0f;
        float actual_value = 0.0f;
        uint8_t type = 0;
        std::string state;
        std::string error;
        int64_t requested_at_ms = 0;
        int64_t completed_at_ms = 0;
    };
    std::vector<ParamChangeRecord> param_change_history() const;
    struct ParamRefreshStatus {
        bool active = false;
        int total = 0;
        int completed = 0;
        int pending = 0;
        int round = 0;
        std::vector<std::string> failed;
    };
    /// 启动批量刷新并返回成功发送的请求数。
    int start_param_refresh(uint8_t target_system,
                            uint8_t target_component,
                            const std::vector<std::string>& names);
    /// 当前批量刷新状态。
    ParamRefreshStatus param_refresh_status() const;
    // 已处理消息总数
    uint64_t message_count() const;
    LinkQualityInfo link_quality() const;
    /// 获取距上次收到心跳的毫秒数，未收到过返回 0
    uint64_t heartbeat_age_ms() const;

    /// 检查心跳是否在超时范围内存活，默认 5000ms（QGC 标准）
    bool heartbeat_alive(uint64_t timeout_ms = 5000) const;
    // ================================================================
    // 带 Status 返回值的版本（提供丰富错误信息）
    //
    // 这些方法返回 Status 类型（含错误码和原因），替代原始的 bool 返回。
    // 原始 bool 函数保留以保持向后兼容，新代码应优先使用这些方法。
    // ================================================================

    /// 设置参数（带错误上下文）
    mg::core::Status set_param_status(uint8_t target_system, uint8_t target_component,
                                      const char* param_id, float param_value, uint8_t param_type);

    /// 发送 COMMAND_LONG（带错误上下文）
    mg::core::Status send_command_long_status(uint8_t target_system, uint8_t target_component,
                                              uint16_t command, uint8_t confirmation,
                                              float p1, float p2, float p3, float p4,
                                              float p5, float p6, float p7);

    /// 解锁/锁定（带错误上下文）
    mg::core::Status arm_disarm_status(uint8_t target_system, uint8_t target_component, bool arm, bool force = false);

    /// 设置飞行模式（带错误上下文）
    mg::core::Status set_mode_status(uint8_t target_system, uint8_t target_component, uint8_t base_mode, uint32_t custom_mode);

    /// GUIDED 飞往指定位置（带错误上下文）
    mg::core::Status cmd_guided_point_status(uint8_t target_system, uint8_t target_component,
                                             double lat_deg, double lon_deg, float alt_m,
                                             float yaw_deg = -1.0f, float speed_ms = 0.0f);



    // ================================================================
    // 命令队列 API — ACK 超时重发（参照 QGC MavCommandQueue）
    // ================================================================
    //
    // ACK 匹配约束：MAVLink 的 COMMAND_ACK 不带请求 ID，只能按
    // (来源 sysid, compid, command) 匹配。因此“同一目标、同一条命令尚未收到 ACK
    // 就重复下发”无法区分两次请求的 ACK 归属，必须由调用方在入队前拒绝
    // （FFI 层按 QGC 的 isPending(compId, command) 规则拒绝重复提交）。

    /// 入队一个命令，自动处理 ACK 等待和超时重发
    /// 返回 CommandTicket 用于跟踪结果
    CommandTicket send_command_queued(
        uint8_t target_system, uint8_t target_component,
        uint16_t command,
        float p1, float p2, float p3, float p4,
        float p5, float p6, float p7,
        std::function<void(const CommandAckInfo&)> result_cb = nullptr);

    /// 入队一个 COMMAND_INT（用于 ArduPilot 的 DO_REPOSITION 等命令）。
    CommandTicket send_command_int_queued(
        uint8_t target_system, uint8_t target_component,
        uint16_t command, uint8_t frame,
        float p1, float p2, float p3, float p4,
        int32_t x, int32_t y, float z,
        std::function<void(const CommandAckInfo&)> result_cb = nullptr);

    /// 便捷：arm/disarm 通过队列
    CommandTicket arm_disarm_queued(
        uint8_t target_system, uint8_t target_component,
        bool arm, bool force,
        std::function<void(const CommandAckInfo&)> result_cb = nullptr);

    /// 便捷：set_mode 通过队列
    CommandTicket set_mode_queued(
        uint8_t target_system, uint8_t target_component,
        uint8_t base_mode, uint32_t custom_mode,
        std::function<void(const CommandAckInfo&)> result_cb = nullptr);

    /// 阻塞等待命令完成，超时返回 nullopt
    std::optional<CommandAckInfo> wait_for_command(CommandTicket ticket, int timeout_ms = 5000);

    /// 取消队列中的待处理命令
    bool cancel_command(CommandTicket ticket);

    /// 周期性检查超时（由 process() 自动调用，也可手动调用）
    void tick();

    // ================================================================
    // MAVLink FTP（FILE_TRANSFER_PROTOCOL）
    //
    // 传输跑在 FTP 客户端的后台线程上：这些入口只负责发起，调用线程立刻
    // 返回；进度、结果与错误通过 file_transfer_status() 读取；取消走
    // file_transfer_cancel()。同一时刻只允许一个传输（协议是单会话），
    // 重复发起返回 false。详见 docs/features/flight-logging-and-replay.md。
    // ================================================================

    /// 发起列目录；已有传输在进行时返回 false。
    bool file_list_start(uint8_t target_system, uint8_t target_component,
                         const std::string& remote_path);

    /// 发起下载；已有传输在进行时返回 false。
    bool file_download_start(uint8_t target_system, uint8_t target_component,
                             const std::string& remote_path,
                             const std::string& local_path);

    /// 发起上传；已有传输在进行时返回 false。
    bool file_upload_start(uint8_t target_system, uint8_t target_component,
                           const std::string& local_path,
                           const std::string& remote_path);

    /// 当前/最近一次传输的状态快照（线程安全）。
    mg::core::ftp::MavlinkFtpClient::FtpTransferStatus file_transfer_status() const;

    /// 取消正在进行的传输；没有传输时返回 false。
    bool file_transfer_cancel();

    /// 等待传输结束（供 @SYS/uarts.txt 这类必须同步取回的小文件使用）。
    /// 返回 true 表示在超时前结束；超时返回 false，调用方应立即取消。
    /// 这个等待在 FFI 调用线程上发生，最长就是 timeout_ms，且不持任何
    /// adapter 锁；真正的传输在 FTP 后台线程上跑。
    bool file_transfer_wait(int timeout_ms);

private:
    struct Impl;  // PIMPL 模式，隐藏实现细节
    std::unique_ptr<Impl> impl_;

    // 内部通用任务传输 helpers（消除 mission/rally/fence 三重复制）。
    // 调用方持有 impl_->mtx（UniqueLock），helper 在发送那一帧时临时解锁，
    // 避免在锁内调用传输层回调（A3/A4，见 ADR-0005）。
    bool mission_type_download(uint8_t sys, uint8_t comp,
                               MissionTransferState& state, uint8_t mission_type,
                               mg::core::platform::UniqueLock& lk);
    bool mission_type_upload(uint8_t sys, uint8_t comp,
                             MissionTransferState& state,
                             const mg::core::mission::MissionPlan& plan,
                             uint8_t mission_type,
                             mg::core::platform::UniqueLock& lk);
    bool mission_type_clear(uint8_t sys, uint8_t comp,
                            MissionTransferState& state, uint8_t mission_type,
                            mg::core::platform::UniqueLock& lk);
    bool mission_type_cancel(MissionTransferState& state, uint8_t mission_type,
                             mg::core::platform::UniqueLock& lk);
    bool try_auto_detect_signing(const mavlink_message_t& message);
};

} // namespace mg::core::mavlink
