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
#include "mavlink_adapter_state.h"
#include <array>
#include <cstring>
#include <chrono>
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


// ParamCache, MissionTransferState, telemetry/link/log state live in their
// own internal headers; keep this header focused on Impl and dispatch.

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
    void feed_mag_progress_compat(const uint8_t* data, size_t size);

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

    bool send_command_queue_entry(const CommandQueueEntry& e);

    // ================================================================
    // 消息处理器（在 process() 中持锁调用）
    // ================================================================

    // 处理心跳消息
    void handle_heartbeat(const mavlink_message_t& m);

    // 处理系统状态消息
    void handle_sys_status(const mavlink_message_t& m);

    // 处理 GPS 原始数据
    void handle_gps(const mavlink_message_t& m);

    // 处理缩放 IMU 数据。SCALED_IMU/2/3（26/116/129）三个消息的字段顺序、
    // 类型和长度完全相同，只有消息 ID 不同，所以共用同一个解码函数，
    // 用 instance 参数区分是第几个 IMU。
    void handle_scaled_imu(const mavlink_message_t& m, uint8_t instance);

    // 处理气压计数据
    void handle_scaled_pressure(const mavlink_message_t& m);

    // 处理姿态数据
    void handle_attitude(const mavlink_message_t& m);

    // 处理 VFR HUD 飞行仪表数据
    void handle_vfr_hud(const mavlink_message_t& m);

    // 处理飞控版本信息
    void handle_autopilot_version(const mavlink_message_t& m);

    // 处理参数值响应
    // 处理参数值（下载接收 + 写入确认）
    void handle_param_value(const mavlink_message_t& m);

    // 处理参数写入明确拒绝
    void handle_param_error(const mavlink_message_t& m);

    // 处理命令确认（路由到命令队列 + 唤醒旧 waiter）
    void handle_command_ack(const mavlink_message_t& m);

    void handle_command_long(const mavlink_message_t& m);

    void handle_mag_cal_progress(const mavlink_message_t& m);

    void handle_mag_cal_report(const mavlink_message_t& m);

    // 处理遥控通道数据
    void handle_rc_channels(const mavlink_message_t& m);

    // 处理舵机原始输出
    void handle_servo_output_raw(const mavlink_message_t& m);

    // 处理飞控状态文本
    void handle_statustext(const mavlink_message_t& m);

    // 处理全局位置数据
    void handle_global_position(const mavlink_message_t& m);

    // 处理返航点位置
    void handle_home_position(const mavlink_message_t& m);

    // 处理电池状态
    void handle_battery_status(const mavlink_message_t& m);

    // 处理高分辨率 IMU 数据
    void handle_highres_imu(const mavlink_message_t& m);

    // 处理扩展系统状态
    void handle_extended_sys_state(const mavlink_message_t& m);

    // 处理振动数据
    void handle_vibration(const mavlink_message_t& m);

    // 处理电调状态数据
    void handle_esc_status(const mavlink_message_t& m);

    // 处理距离传感器数据
    void handle_distance_sensor(const mavlink_message_t& m);

    // 处理 ADS-B 飞行器数据
    void handle_adsb_vehicle(const mavlink_message_t& m);

    // 处理视频流信息（VIDEO_STREAM_INFORMATION, msg 269）
    void handle_video_stream_information(const mavlink_message_t& m);

    // ================================================================
    // 相机/云台消息处理器
    // ================================================================

    // 相机信息（msg 261）— 转发给 CameraManager 解析
    void handle_camera_information(const mavlink_message_t& m);

    // 相机拍照状态（msg 254）
    void handle_camera_capture_status(const mavlink_message_t& m);

    // 云台管理器信息（msg 280）
    void handle_gimbal_manager_info(const mavlink_message_t& m);

    // 云台管理器状态（msg 281）
    void handle_gimbal_manager_status(const mavlink_message_t& m);

    // ================================================================
    // 任务协议处理器
    // ================================================================

    // 按 mission_type 选择对应的传输状态（委托 mission_transfer.h 自由函数）
    MissionTransferState& select_transfer(uint8_t mission_type);

    // 查找当前正在上传的传输状态（委托 mission_transfer.h 自由函数）
    MissionTransferState* find_uploading();

    // 处理任务计数消息（开始下载）
    void handle_mission_count(const mavlink_message_t& m);

    // 处理任务项（INT 格式，用于下载接收）
    void handle_mission_item_int(const mavlink_message_t& m);

    // 处理旧版任务项（浮点格式，用于兼容下载接收）
    void handle_mission_item(const mavlink_message_t& m);

    void mark_mission_item_sent(MissionTransferState& state, uint16_t seq);

    // 处理任务请求（INT 格式，用于上传响应）
    void handle_mission_request_int(const mavlink_message_t& m);

    // 处理旧版任务请求（非 INT 格式，用旧版 MISSION_ITEM 回复）
    void handle_mission_request(const mavlink_message_t& m);

    // 处理任务确认消息
    void handle_mission_ack(const mavlink_message_t& m);

    // 处理日志条目列表
    void handle_log_entry(const mavlink_message_t& m);

    // 处理日志数据块（完成时写入文件）
    void handle_log_data(const mavlink_message_t& m);

    // 处理当前任务航点
    void handle_mission_current(const mavlink_message_t& m);

    // 处理任务航点到达通知
    void handle_mission_item_reached(const mavlink_message_t& m);

    // 任务发送辅助函数
    // 请求下载下一个任务项
    bool send_mission_request_int(uint8_t sysid, uint8_t compid, uint16_t seq);

    // 发送任务确认
    bool send_mission_ack(uint8_t sysid, uint8_t compid, uint8_t type);

    // 发送任务项（INT 格式）
    bool send_mission_item_int(uint8_t sysid, uint8_t compid, const mg::core::mission::MissionItem& item);

    // 发送任务项（旧版浮点格式），用于兼容只请求 MISSION_ITEM 的飞控。
    bool send_mission_item(uint8_t sysid, uint8_t compid, const mg::core::mission::MissionItem& item);

    // 发送 PARAM_REQUEST_READ（按索引请求单个参数）
    bool send_param_request_read_by_index(uint8_t sysid, uint8_t compid, int16_t param_index);

    // 处理参数超时（在 process() 中调用）
    void process_param_timeouts();

    // 处理完整解码的 MAVLink 消息（获取锁，处理后在锁外执行回调）
    void process(const mavlink_message_t& m);

};

} // namespace mg::core::mavlink

#endif /* MGCORE_MAVLINK_ADAPTER_INTERNAL_H */
