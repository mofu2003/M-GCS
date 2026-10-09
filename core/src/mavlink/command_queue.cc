/**
 * command_queue.cc - Command queue implementation.
 */
#include "command_queue.h"

namespace mg::core::mavlink {

int CommandQueue::find_by_command(uint8_t sysid, uint8_t compid, uint16_t command) const {
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        // 0 表示调用方未指定目标，按通配处理；否则必须与登记的目标一致。
        const bool system_matches = entries[i].target_sys == sysid ||
                                    entries[i].target_sys == 0;
        const bool component_matches = entries[i].target_comp == compid ||
                                       entries[i].target_comp == 0;
        if (system_matches &&
            component_matches &&
            entries[i].command == command &&
            (entries[i].state == CommandQueueEntry::State::Pending ||
             entries[i].state == CommandQueueEntry::State::InProgress)) {
            return i;
        }
    }
    return -1;
}

int CommandQueue::find_by_id(uint64_t id) const {
    for (int i = 0; i < static_cast<int>(entries.size()); i++) {
        if (entries[i].id == id) return i;
    }
    return -1;
}

uint64_t CommandQueue::handle_ack(uint8_t sysid, uint8_t compid, uint16_t command,
                                  uint8_t result, uint8_t progress) {
    int idx = find_by_command(sysid, compid, command);
    if (idx < 0) return 0;

    auto& e = entries[idx];
    if (result == 5) {  // MAV_RESULT_IN_PROGRESS
        e.state = CommandQueueEntry::State::InProgress;
        e.ack_progress = progress;
        e.ack_timeout_ms = 30000;
        e.send_time = std::chrono::steady_clock::now();
    } else {
        e.state = CommandQueueEntry::State::Acked;
        e.ack_result = result;
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
            e.ack_result = 4;  // MAV_RESULT_FAILED
            e.send_time = now;
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
