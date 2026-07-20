/**
 * flight_log_replayer.cc - 飞行日志回放器实现
 *
 * 解析 tlog 二进制格式，按时间戳回放 MAVLink 消息。
 * tlog 格式：[uint64_t timestamp_us][uint16_t length][uint8_t payload[length]]
 */
#include "core/log/flight_log_replayer.h"
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
    info_.total_bytes = ftell(file_);
    fseek(file_, 0, SEEK_SET);

    // 扫描文件获取消息数和时间范围
    uint64_t min_ts = UINT64_MAX, max_ts = 0;
    uint64_t count = 0;
    while (true) {
        uint64_t ts;
        uint16_t len;
        if (fread(&ts, sizeof(ts), 1, file_) != 1) break;
        if (fread(&len, sizeof(len), 1, file_) != 1) break;
        if (len > 1024) break;  // 防止损坏文件

        uint8_t payload[1024];
        if (fread(payload, 1, len, file_) != len) break;

        if (ts < min_ts) min_ts = ts;
        if (ts > max_ts) max_ts = ts;
        count++;
    }

    info_.message_count = count;
    info_.first_timestamp = (min_ts != UINT64_MAX) ? min_ts : 0;
    info_.last_timestamp = max_ts;
    info_.duration_us = (max_ts > min_ts) ? (max_ts - min_ts) : 0;
    first_ts_ = info_.first_timestamp;

    // 回到文件开头
    fseek(file_, 0, SEEK_SET);
    header_read_ = false;

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
    first_ts_ = 0;
    last_read_ts_ = 0;
    header_read_ = false;
}

bool FlightLogReplayer::next_message() {
    if (!file_) return false;

    // 读取 tlog 记录头
    uint64_t ts;
    uint16_t len;
    if (fread(&ts, sizeof(ts), 1, file_) != 1) return false;
    if (fread(&len, sizeof(len), 1, file_) != 1) return false;
    if (len > 1024) return false;

    uint8_t payload[1024];
    if (fread(payload, 1, len, file_) != len) return false;

    // 解析 MAVLink 消息
    mavlink_status_t status;
    mavlink_message_t msg;
    bool parsed = false;
    for (uint16_t i = 0; i < len; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, payload[i], &msg, &status)) {
            parsed = true;
            break;
        }
    }

    if (!parsed) return false;

    current_msg_ = msg;
    current_ts_ = ts;

    // 时间控制：按 speed_ 倍率等待
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
    uint64_t elapsed = current_ts_ - first_ts_;
    return static_cast<float>(elapsed) / static_cast<float>(info_.duration_us);
}

bool FlightLogReplayer::seek(float percent) {
    if (!file_ || info_.total_bytes == 0) return false;
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;

    uint64_t target_pos = static_cast<uint64_t>(info_.total_bytes * percent);
    // 对齐到记录边界（跳到最近的完整记录）
    fseek(file_, target_pos, SEEK_SET);

    // 读取并丢弃直到找到有效的记录
    uint64_t ts;
    uint16_t len;
    for (int attempts = 0; attempts < 100; attempts++) {
        if (fread(&ts, sizeof(ts), 1, file_) != 1) return false;
        if (fread(&len, sizeof(len), 1, file_) != 1) return false;
        if (len > 0 && len <= 1024) {
            // 看起来是有效的记录
            fseek(file_, -(int)(sizeof(ts) + sizeof(len)), SEEK_CUR);
            current_ts_ = ts;
            return true;
        }
        // 跳过这个字节继续搜索
        fseek(file_, -(int)(sizeof(ts) + sizeof(len) - 1), SEEK_CUR);
    }
    return false;
}

void FlightLogReplayer::rewind() {
    if (file_) {
        fseek(file_, 0, SEEK_SET);
        current_ts_ = 0;
        last_read_ts_ = 0;
        header_read_ = false;
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
