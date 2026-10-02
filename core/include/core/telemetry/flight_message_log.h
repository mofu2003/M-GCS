/**
 * flight_message_log.h - 飞控消息（STATUSTEXT）历史
 *
 * 遥测缓存里只留"最新一条"状态文本，够给车辆名之类的逻辑用；消息框需要的是历史，
 * 所以单独放一个带自己锁的小类，不往 telemetry_cache.h 里堆（该头有体积上限守卫）。
 */
#ifndef MGCORE_FLIGHT_MESSAGE_LOG_H
#define MGCORE_FLIGHT_MESSAGE_LOG_H

#include "core/platform/platform_mutex.h"

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace mg::core::telemetry {

/// 一条飞控消息。
struct FlightMessageEntry {
    uint64_t seq = 0;          // 单调递增，前端按它增量拉取
    uint64_t timestamp_ms = 0; // 收到时刻（UTC 毫秒）
    uint8_t severity = 0;      // MAV_SEVERITY
    std::string text;
};

/// 最近 N 条飞控消息的环形缓冲。
class FlightMessageLog {
public:
    /// 保留的历史条数。
    static constexpr size_t kMaxMessages = 200;

    void add(const std::string& text, uint8_t severity) {
        mg::core::platform::LockGuard lk(mtx_);
        FlightMessageEntry entry;
        entry.seq = next_seq_++;
        entry.timestamp_ms = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
        entry.severity = severity;
        entry.text = text;
        entries_.push_back(std::move(entry));
        while (entries_.size() > kMaxMessages) entries_.pop_front();
    }

    /// seq 大于 since_seq 的条目，供前端增量拉取。
    std::vector<FlightMessageEntry> messages_since(uint64_t since_seq) const {
        mg::core::platform::LockGuard lk(mtx_);
        std::vector<FlightMessageEntry> out;
        for (const auto& entry : entries_) {
            if (entry.seq > since_seq) out.push_back(entry);
        }
        return out;
    }

    /// 已产生的消息总数（含被环形缓冲淘汰的）。
    uint64_t count() const {
        mg::core::platform::LockGuard lk(mtx_);
        return next_seq_ - 1;
    }

private:
    mutable mg::core::platform::Mutex mtx_;
    std::deque<FlightMessageEntry> entries_;
    uint64_t next_seq_ = 1;
};

} // namespace mg::core::telemetry

#endif /* MGCORE_FLIGHT_MESSAGE_LOG_H */
