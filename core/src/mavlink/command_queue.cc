/**
 * command_queue.cc - Command queue implementation.
 */
#include "command_queue.h"

namespace mg::core::mavlink {

namespace {
/// 超时后多久内拒绝同目标同命令的新请求。
///
/// MAVLink 没有请求 ID，这个窗口用于隔离旧请求的迟到 ACK。它必须覆盖常见的
/// 串口/UDP 延迟，同时不能把用户重试困太久；5 秒与 QGC 默认命令超时同量级。
constexpr std::chrono::milliseconds kLateAckGuardMs{5000};

bool target_matches(const CommandQueueEntry& entry, uint8_t sysid, uint8_t compid) {
    // 0 表示调用方未指定目标，按通配处理。
    const bool system_matches = entry.target_sys == sysid || entry.target_sys == 0;
    const bool component_matches = entry.target_comp == compid || entry.target_comp == 0;
    return system_matches && component_matches;
}
}  // namespace

int CommandQueue::find_by_command(uint8_t sysid, uint8_t compid, uint16_t command) const {
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (target_matches(entries[i], sysid, compid) &&
            entries[i].command == command &&
            (entries[i].state == CommandQueueEntry::State::Pending ||
             entries[i].state == CommandQueueEntry::State::InProgress)) {
            return i;
        }
    }
    return -1;
}

bool CommandQueue::has_recent_timeout(uint8_t sysid, uint8_t compid, uint16_t command,
                                      std::chrono::steady_clock::time_point now) const {
    for (const auto& entry : entries) {
        if (entry.command != command || !target_matches(entry, sysid, compid)) continue;
        if (entry.state != CommandQueueEntry::State::TimedOut) continue;
        if (entry.timed_out_at == std::chrono::steady_clock::time_point{}) continue;
        const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - entry.timed_out_at);
        if (age >= std::chrono::milliseconds::zero() && age <= kLateAckGuardMs) {
            return true;
        }
    }
    return false;
}

int CommandQueue::find_by_id(uint64_t id) const {
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (entries[i].id == id) return i;
    }
    return -1;
}

uint64_t CommandQueue::handle_ack(uint8_t sysid, uint8_t compid, uint16_t command,
                                  uint8_t result, uint8_t progress) {
    int idx = -1;
    const auto now = std::chrono::steady_clock::now();
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        auto& candidate = entries[i];
        if (!target_matches(candidate, sysid, compid) || candidate.command != command) continue;
        if (candidate.state == CommandQueueEntry::State::TimedOut) {
            if (candidate.timed_out_at != std::chrono::steady_clock::time_point{}) {
                const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - candidate.timed_out_at);
                if (age >= std::chrono::milliseconds::zero() && age <= kLateAckGuardMs) {
                    // 旧请求已超时，这个 ACK 不可安全归属；直接吞掉，不能落到
                    // 后面同命令的新条目上。
                    return 0;
                }
            }
            continue;
        }
        if (candidate.state == CommandQueueEntry::State::Pending ||
            candidate.state == CommandQueueEntry::State::InProgress) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return 0;

    auto& e = entries[idx];
    if (result == 5) {  // MAV_RESULT_IN_PROGRESS
        e.state = CommandQueueEntry::State::InProgress;
        e.outcome = CommandOutcome::InProgress;
        e.ack_progress = progress;
        e.ack_timeout_ms = 30000;
        e.send_time = std::chrono::steady_clock::now();
    } else {
        e.state = CommandQueueEntry::State::Acked;
        e.ack_result = result;
        e.outcome = result == 0 ? CommandOutcome::Accepted : CommandOutcome::Rejected;
        e.timed_out = false;
        e.send_time = std::chrono::steady_clock::now();
    }
    cv.notify_all();
    return e.id;
}

std::vector<CommandQueueEntry> CommandQueue::check_timeouts(
    const std::function<bool(const CommandQueueEntry&)>& resend_fn) {
    auto now = std::chrono::steady_clock::now();
    std::vector<CommandQueueEntry> completed;
    bool notified = false;

    for (auto& e : entries) {
        const bool is_pending = e.state == CommandQueueEntry::State::Pending;
        const bool is_in_progress = e.state == CommandQueueEntry::State::InProgress;
        if (!is_pending && !is_in_progress) continue;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - e.send_time).count();
        if (elapsed < e.ack_timeout_ms) continue;

        if (is_pending && e.try_count < e.max_tries) {
            e.try_count++;
            e.confirmation++;
            e.send_time = now;
            resend_fn(e);
        } else {
            e.state = CommandQueueEntry::State::TimedOut;
            e.timed_out = true;
            e.outcome = CommandOutcome::Timeout;
            e.ack_result = 4;  // MAV_RESULT_FAILED
            e.send_time = now;
            e.timed_out_at = now;
            completed.push_back(e);
            notified = true;
        }
    }
    if (notified) cv.notify_all();
    return completed;
}

void CommandQueue::purge(std::chrono::milliseconds max_age) {
    auto now = std::chrono::steady_clock::now();
    entries.erase(
        std::remove_if(entries.begin(), entries.end(),
            [&](const CommandQueueEntry& e) {
                if (e.state == CommandQueueEntry::State::Pending ||
                    e.state == CommandQueueEntry::State::InProgress)
                    return false;
                auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - e.send_time);
                return age > max_age;
            }),
        entries.end());
}

} // namespace mg::core::mavlink
