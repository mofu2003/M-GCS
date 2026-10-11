/**
 * flight_log_replayer.h - 飞行日志回放器（.tlog）
 *
 * 从 .tlog 文件读取 MAVLink 消息并按原始时间戳回放，用于离线调试与飞行
 * 数据分析。磁盘格式与写入器共用 core/log/tlog_format.h 的契约：
 *   [uint64 大端 Unix 微秒时间戳][完整 MAVLink 帧字节]（无长度前缀）。
 *
 * 用法：
 *   FlightLogReplayer replayer;
 *   replayer.open("flight.tlog");
 *   replayer.set_speed(2.0);  // 2x 速度；0 = 尽快，不做实时等待
 *   while (replayer.next_message()) {
 *       // 处理 replayer.current_message()
 *   }
 */
#ifndef MGCORE_FLIGHT_LOG_REPLAYER_H
#define MGCORE_FLIGHT_LOG_REPLAYER_H

#include "ardupilotmega/mavlink.h"
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

    /// 跳转到指定百分比位置 (0.0 - 1.0)。
    ///
    /// 落点对齐到「记录边界」：定位到起点字节不小于目标偏移的第一条完整记录，
    /// 并把它挂为待读记录——因此 seek 之后第一次 next_message() 返回的正是
    /// 落点记录本身，而不是它后面的一条。记录之间没有长度前缀，从任意字节
    /// 偏移无法可靠重同步，所以这里按记录顺序前进，不做裸字节扫描。
    /// 目标偏移已在文件末尾（percent == 1.0）或找不到可读记录时返回 false。
    bool seek(float percent);

    /// 重置到文件开头
    void rewind();

    /// 使用回调模式回放（自动调用 message_handler）
    /// handler 返回 false 可提前终止
    void replay_all(const std::function<bool(const mavlink_message_t&, uint64_t ts)>& handler);

private:
    /// 读取下一条记录（时间戳前缀 + 完整 MAVLink 帧），成功返回 true。
    /// 文件结束返回 false 并置 eof_；读到半条帧返回 false 并置 truncated_。
    bool read_record(uint64_t& timestamp_us);

    FILE* file_ = nullptr;
    LogFileInfo info_;
    double speed_ = 1.0;
    mavlink_message_t current_msg_{};
    uint64_t current_ts_ = 0;
    // seek() 落点记录：解析一次后挂起，交给下一次 next_message() 产出，
    // 保证 seek 后的第一条消息就是落点记录。
    mavlink_message_t pending_msg_{};
    uint64_t pending_ts_ = 0;
    bool has_pending_ = false;
    uint64_t first_ts_ = 0;
    uint64_t last_read_ts_ = 0;
    std::chrono::steady_clock::time_point last_wall_time_;
    bool header_read_ = false;
    bool eof_ = false;
    bool truncated_ = false;

public:
    /// 是否读到过半条记录（文件被截断），而非正常 EOF。
    /// open() 的扫描与回放读取共用同一个标志：open 后即可知道文件是否完整，
    /// rewind()/seek()/close() 会重新评估。用于 UI 提示。
    bool truncated() const { return truncated_; }
};

} // namespace mg::core::log

#endif /* MGCORE_FLIGHT_LOG_REPLAYER_H */
