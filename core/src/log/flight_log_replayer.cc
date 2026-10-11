/**
 * flight_log_replayer.cc - 飞行日志回放器实现（.tlog）
 *
 * 解析 QGC / Mission Planner 通用 .tlog 格式：
 *   [uint64 大端 Unix 微秒时间戳][完整 MAVLink 帧字节]
 * 无长度前缀，逐字节喂给 mavlink_frame_char_buffer 直到组帧完成。
 * 使用调用方自有的解析缓冲，不占用全局 MAVLINK_COMM_* 通道状态。
 */
#include "core/log/flight_log_replayer.h"
#include "core/log/tlog_format.h"
#include "core/log/logging.h"
#include <cstring>
#include <thread>

namespace mg::core::log {

FlightLogReplayer::FlightLogReplayer() = default;

FlightLogReplayer::~FlightLogReplayer() {
    close();
}

bool FlightLogReplayer::open(const std::string& filepath) {
    close();

    file_ = fopen(filepath.c_str(), "rb");
    if (!file_) {
        MGC_LOG_ERROR("[Replayer] Failed to open: %s", filepath.c_str());
        return false;
    }

    info_.filepath = filepath;

    // 获取文件大小
    fseek(file_, 0, SEEK_END);
    info_.total_bytes = static_cast<uint64_t>(ftell(file_));
    fseek(file_, 0, SEEK_SET);

    // 扫描以获得消息数与时间范围（使用自有解析缓冲，逐记录读）
    uint64_t min_ts = UINT64_MAX, max_ts = 0, count = 0;
    uint64_t ts = 0;
    while (read_record(ts)) {
        if (ts < min_ts) min_ts = ts;
        if (ts > max_ts) max_ts = ts;
        count++;
    }

    info_.message_count = count;
    info_.first_timestamp = (min_ts != UINT64_MAX) ? min_ts : 0;
    info_.last_timestamp = max_ts;
    info_.duration_us = (max_ts > min_ts) ? (max_ts - min_ts) : 0;
    first_ts_ = info_.first_timestamp;

    // 回到文件开头，重置回放状态
    fseek(file_, 0, SEEK_SET);
    header_read_ = false;
    last_read_ts_ = 0;
    current_ts_ = 0;
    current_msg_ = {};
    has_pending_ = false;
    pending_ts_ = 0;
    eof_ = false;
    // truncated_ 保留上面的扫描结果：文件本身半条记录截断必须让调用方看到，
    // 在这里清成 false 会把「文件不完整」伪装成正常打开。

    MGC_LOG_INFO("[Replayer] Opened %s: %llu messages, %.1f sec",
                 filepath.c_str(), (unsigned long long)count,
                 info_.duration_us / 1000000.0);
    return true;
}

void FlightLogReplayer::close() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
    info_ = {};
    current_msg_ = {};
    current_ts_ = 0;
    pending_ts_ = 0;
    has_pending_ = false;
    first_ts_ = 0;
    last_read_ts_ = 0;
    header_read_ = false;
    eof_ = false;
    truncated_ = false;
}

bool FlightLogReplayer::read_record(uint64_t& timestamp_us) {
    if (!file_) return false;

    uint8_t ts_bytes[tlog::kTimestampBytes];
    if (fread(ts_bytes, 1, tlog::kTimestampBytes, file_) != tlog::kTimestampBytes) {
        eof_ = true;  // 正常结束（或末尾残半个时间戳）
        return false;
    }
    timestamp_us = tlog::decode_timestamp(ts_bytes);

    // 记录只含帧字节，没有长度前缀：逐字节解析到完整帧为止。
    mavlink_message_t rx{};
    mavlink_status_t rx_status{};
    mavlink_message_t msg{};
    mavlink_status_t status{};
    int c;
    while ((c = fgetc(file_)) != EOF) {
        const uint8_t framing =
            mavlink_frame_char_buffer(&rx, &rx_status, static_cast<uint8_t>(c), &msg, &status);
        if (framing == MAVLINK_FRAMING_OK) {
            current_msg_ = msg;
            current_ts_ = timestamp_us;
            return true;
        }
    }

    // 读到了时间戳却没能组出完整帧：文件在记录中途被截断。
    truncated_ = true;
    return false;
}

bool FlightLogReplayer::next_message() {
    if (!file_) return false;

    uint64_t ts = 0;
    if (has_pending_) {
        // seek() 的落点记录：它已经被解析过，这里直接产出，保证 seek 后的
        // 第一条消息就是落点本身，而不是它后面的一条。
        has_pending_ = false;
        ts = pending_ts_;
        current_msg_ = pending_msg_;
        current_ts_ = pending_ts_;
    } else if (!read_record(ts)) {
        return false;
    }

    // 时间控制：按 speed_ 倍率等待（speed_ == 0 表示尽快，不等待）
    if (speed_ > 0 && last_read_ts_ > 0) {
        uint64_t delta_us = ts - last_read_ts_;
        if (delta_us > 0 && delta_us < 10000000) {  // 最多等 10 秒
            auto wall_now = std::chrono::steady_clock::now();
            if (!header_read_) {
                last_wall_time_ = wall_now;
                header_read_ = true;
            }
            uint64_t wait_us = static_cast<uint64_t>(delta_us / speed_);
            auto target = last_wall_time_ + std::chrono::microseconds(wait_us);
            if (target > wall_now) {
                std::this_thread::sleep_until(target);
            }
            last_wall_time_ = std::chrono::steady_clock::now();
        }
    } else {
        last_wall_time_ = std::chrono::steady_clock::now();
        header_read_ = true;
    }

    last_read_ts_ = ts;
    return true;
}

float FlightLogReplayer::progress() const {
    if (info_.duration_us == 0) return 0.0f;
    if (current_ts_ <= first_ts_) return 0.0f;
    uint64_t elapsed = current_ts_ - first_ts_;
    float p = static_cast<float>(elapsed) / static_cast<float>(info_.duration_us);
    return p > 1.0f ? 1.0f : p;
}

bool FlightLogReplayer::seek(float percent) {
    if (!file_ || info_.total_bytes == 0) return false;
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;

    const uint64_t target_pos = static_cast<uint64_t>(info_.total_bytes * percent);
    // 落点必须在文件内；percent == 1.0 指向末尾，没有可读记录。
    if (target_pos >= info_.total_bytes) return false;

    // .tlog 记录之间没有长度前缀，从任意字节偏移无法可靠重同步（8 字节时间戳
    // 前缀会被误当成帧字节喂进解析器，落点会随字节内容漂移）。因此按记录边界
    // 顺序前进，找到第一条起点偏移 >= target_pos 的完整记录作为落点。
    if (fseek(file_, 0, SEEK_SET) != 0) return false;
    has_pending_ = false;
    eof_ = false;
    truncated_ = false;

    uint64_t ts = 0;
    bool found = false;
    while (true) {
        const long record_start = ftell(file_);
        if (record_start < 0) return false;
        if (static_cast<uint64_t>(record_start) >= target_pos) {
            if (!read_record(ts)) break;  // 落点之前文件就结束了
            pending_msg_ = current_msg_;
            pending_ts_ = ts;
            has_pending_ = true;
            found = true;
            break;
        }
        if (!read_record(ts)) break;  // 目标偏移超出最后一条记录
    }
    if (!found) return false;

    current_ts_ = pending_ts_;
    last_read_ts_ = 0;  // 跨段跳转后不按两段之间的间隔等待
    header_read_ = false;
    return true;
}

void FlightLogReplayer::rewind() {
    if (file_) {
        fseek(file_, 0, SEEK_SET);
        current_ts_ = 0;
        pending_ts_ = 0;
        has_pending_ = false;
        last_read_ts_ = 0;
        header_read_ = false;
        eof_ = false;
        truncated_ = false;
    }
}

void FlightLogReplayer::replay_all(
    const std::function<bool(const mavlink_message_t&, uint64_t)>& handler) {
    if (!file_) return;
    rewind();
    while (next_message()) {
        if (!handler(current_msg_, current_ts_)) break;
    }
}

} // namespace mg::core::log
