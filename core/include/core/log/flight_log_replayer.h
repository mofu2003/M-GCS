/**
 * flight_log_replayer.h - 飞行日志回放器
 *
 * 从 tlog 文件读取 MAVLink 消息并按原始时间戳回放。
 * 用于离线调试和飞行数据分析。
 *
 * 用法：
 *   FlightLogReplayer replayer;
 *   replayer.open("flight.tlog");
 *   replayer.set_speed(2.0);  // 2x 速度
 *   while (replayer.next_message()) {
 *       // 处理 replayer.current_message()
 *   }
 *
 * tlog 文件格式（QGC/MP 标准）：
 *   每条记录: [uint64_t timestamp_us] [uint16_t length] [uint8_t payload[length]]
 */
#ifndef MGCORE_FLIGHT_LOG_REPLAYER_H
#define MGCORE_FLIGHT_LOG_REPLAYER_H

#include "common/mavlink.h"
#include <string>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <chrono>

namespace mg::core::log {

/// 日志条目元数据
struct LogFileInfo {
    std::string filepath;
    uint64_t total_bytes = 0;
    uint64_t message_count = 0;
    uint64_t duration_us = 0;  // 飞行时长（微秒）
    uint64_t first_timestamp = 0;
    uint64_t last_timestamp = 0;
};

/// 飞行日志回放器
class FlightLogReplayer {
public:
    FlightLogReplayer();
    ~FlightLogReplayer();

    /// 打开 tlog 文件
    bool open(const std::string& filepath);

    /// 关闭文件
    void close();

    /// 文件是否已打开
    bool is_open() const { return file_ != nullptr; }

    /// 获取文件信息（需先 open）
    LogFileInfo file_info() const { return info_; }

    /// 设置回放速度倍率（1.0 = 实时，2.0 = 2 倍速，0 = 尽快）
    void set_speed(double speed) { speed_ = speed; }

    /// 读取下一条消息。返回 false 表示文件结束。
    /// 如果 speed_ > 0，会按原始时间间隔等待。
    bool next_message();

    /// 获取当前消息（需在 next_message() 返回 true 后调用）
    const mavlink_message_t& current_message() const { return current_msg_; }

    /// 获取当前消息的原始时间戳（微秒）
    uint64_t current_timestamp_us() const { return current_ts_; }

    /// 获取回放进度 (0.0 - 1.0)
    float progress() const;

    /// 跳转到指定百分比位置 (0.0 - 1.0)
    bool seek(float percent);

    /// 重置到文件开头
    void rewind();

    /// 使用回调模式回放（自动调用 message_handler）
    /// handler 返回 false 可提前终止
    void replay_all(const std::function<bool(const mavlink_message_t&, uint64_t ts)>& handler);

private:
    FILE* file_ = nullptr;
    LogFileInfo info_;
    double speed_ = 1.0;
    mavlink_message_t current_msg_{};
    uint64_t current_ts_ = 0;
    uint64_t first_ts_ = 0;
    uint64_t last_read_ts_ = 0;
    std::chrono::steady_clock::time_point last_wall_time_;
    bool header_read_ = false;
};

} // namespace mg::core::log

#endif /* MGCORE_FLIGHT_LOG_REPLAYER_H */
