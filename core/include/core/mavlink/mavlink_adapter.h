#pragma once

// MAVLink 协议适配器，负责 MAVLink 消息的编解码、遥测缓存和命令发送

#include "core/core_types.h"
#include "core/core_status.h"
#include "core/mission/mission_types.h"
#include "core/firmware/firmware_adapter.h"
#include "core/ftp/mavlink_ftp.h"
#include <functional>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <map>

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


/// 命令确认信息（msg id 77）— 命令执行结果
struct CommandAckInfo {
    uint8_t result = 0;          // MAV_RESULT_xxx
    uint16_t command = 0;        // MAV_CMD_xxx that was acknowledged
    uint8_t progress = 0;        // progress (0-100) for IN_PROGRESS
    std::string result_text;     // optional text
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
    bool cmd_emergency_stop(uint8_t target_system, uint8_t target_component);

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

    /// 绕指定点以给定半径和速度绕圈
    bool cmd_orbit(uint8_t target_sys, uint8_t target_comp,
                   double lat, double lon, float radius_m, float velocity_ms, float altitude_m);

    /// 设置感兴趣区域位置
    bool cmd_set_roi(uint8_t target_sys, uint8_t target_comp, double lat, double lon, float alt);

    /// 取消感兴趣区域
    bool cmd_roi_stop(uint8_t target_sys, uint8_t target_comp);

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

    /// Get provenance for the cached mission plan.
    mg::core::mission::MissionPlanProvenance mission_get_plan_provenance() const;

    /// Get the cached mission plan and provenance atomically.
    mg::core::mission::MissionPlanSnapshot mission_get_plan_snapshot() const;

    // ================================================================
    // Rally Points (MAV_MISSION_TYPE_RALLY = 2)
    // ================================================================

    /// Download rally points from FC (async - check rally_get_progress())
    bool rally_download(uint8_t target_system, uint8_t target_component);

    /// Upload rally points to FC (async - check rally_get_progress())
    bool rally_upload(uint8_t target_system, uint8_t target_component,
                      const mg::core::mission::MissionPlan& plan);

    /// Clear all rally points on FC
    bool rally_clear(uint8_t target_system, uint8_t target_component);

    /// Get the current rally point plan (thread-safe snapshot)
    mg::core::mission::MissionPlan rally_get_plan() const;

    /// Get rally point transfer progress
    mg::core::mission::MissionTransferProgress rally_get_progress() const;

    // ================================================================
    // Geofence (MAV_MISSION_TYPE_FENCE = 1)
    // ================================================================

    /// Download geofence from FC (async - check fence_get_progress())
    bool fence_download(uint8_t target_system, uint8_t target_component);

    /// Upload geofence to FC (async - check fence_get_progress())
    bool fence_upload(uint8_t target_system, uint8_t target_component,
                      const mg::core::mission::MissionPlan& plan);

    /// Clear all geofence items on FC
    bool fence_clear(uint8_t target_system, uint8_t target_component);

    /// Get the current geofence plan (thread-safe snapshot)
    mg::core::mission::MissionPlan fence_get_plan() const;

    /// Get geofence transfer progress
    mg::core::mission::MissionTransferProgress fence_get_progress() const;

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
        bool active = false;
        bool completed = false;
        bool cancelled = false;
        uint16_t id = 0;
        uint32_t received_bytes = 0;
        uint32_t total_bytes = 0;
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



    /// 清除待处理的 ACK 状态
    void clear_pending_ack();
    /// 等待特定命令的 ACK 响应，超时返回 nullopt（旧 API，保留向后兼容）
    std::optional<CommandAckInfo> wait_for_ack(uint16_t command, int timeout_ms = 3000);

    // ================================================================
    // 命令队列 API — ACK 超时重发（参照 QGC MavCommandQueue）
    // ================================================================

    /// 入队一个命令，自动处理 ACK 等待和超时重发
    /// 返回 CommandTicket 用于跟踪结果
    CommandTicket send_command_queued(
        uint8_t target_system, uint8_t target_component,
        uint16_t command,
        float p1, float p2, float p3, float p4,
        float p5, float p6, float p7,
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

    /// Download a small ArduPilot system file through MAVLink FTP.
    /// Used for board-specific metadata such as @SYS/uarts.txt.
    bool download_file(uint8_t target_system, uint8_t target_component,
                       const std::string& remote_path,
                       const std::string& local_path,
                       std::string* error = nullptr);

    bool upload_file(uint8_t target_system, uint8_t target_component,
                     const std::string& local_path,
                     const std::string& remote_path,
                     std::string* error = nullptr);

    /// List a directory on the vehicle's MAVLink FTP filesystem.
    std::vector<mg::core::ftp::DirEntry> list_directory(
        uint8_t target_system, uint8_t target_component,
        const std::string& remote_path,
        std::string* error = nullptr);

    /// Cancel an active MAVLink FTP operation.
    void cancel_file_transfer();

private:
    struct Impl;  // PIMPL 模式，隐藏实现细节
    std::unique_ptr<Impl> impl_;

    // 内部通用任务传输 helpers（消除 mission/rally/fence 三重复制）
    bool mission_type_download(uint8_t sys, uint8_t comp,
                               MissionTransferState& state, uint8_t mission_type);
    bool mission_type_upload(uint8_t sys, uint8_t comp,
                             MissionTransferState& state,
                             const mg::core::mission::MissionPlan& plan,
                             uint8_t mission_type);
    bool mission_type_clear(uint8_t sys, uint8_t comp,
                            MissionTransferState& state, uint8_t mission_type);
};

} // namespace mg::core::mavlink
