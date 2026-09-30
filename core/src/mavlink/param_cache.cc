/**
 * param_cache.cc - 参数缓存状态机实现
 */
#include "param_cache.h"

namespace mg::core::mavlink {

namespace {

int64_t wall_clock_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

void ParamCache::start_download(uint8_t sys, uint8_t comp) {
    download_state = DownloadState::WaitingList;
    download_cancelled = false;
    download_failed = false;
    download_error.clear();
    list_retry_count = 0;
    target_sys = sys;
    target_comp = comp;
    last_send_time = std::chrono::steady_clock::now();
    last_receive_time = last_send_time;
    received_indices.clear();
    missing_indices.clear();
    failed_indices.clear();
    batch_queue.clear();
    for (const auto& [name, pending] : pending_writes) {
        complete_write(
            pending.history_id,
            false,
            pending.value,
            "parameter write cancelled by a new parameter download");
    }
    pending_writes.clear();
}

bool ParamCache::cancel_download() {
    switch (download_state) {
        case DownloadState::WaitingList:
        case DownloadState::Receiving:
        case DownloadState::Recovering:
            break;
        case DownloadState::Idle:
        case DownloadState::Complete:
        case DownloadState::Failed:
            return false;
    }

    download_state = DownloadState::Idle;
    download_cancelled = true;
    download_failed = false;
    download_error = "parameter download cancelled";
    list_retry_count = 0;
    missing_indices.clear();
    batch_queue.clear();
    return true;
}

uint64_t ParamCache::record_write_request(
    const std::string& name,
    float value,
    uint8_t type) {
    if (const auto existing = pending_writes.find(name); existing != pending_writes.end()) {
        complete_write(
            existing->second.history_id,
            false,
            existing->second.value,
            "superseded by a newer parameter write");
        pending_writes.erase(existing);
    }
    const uint64_t id = next_write_id++;
    WriteRecord record;
    record.id = id;
    record.name = name;
    record.requested_value = value;
    record.type = type;
    record.requested_at_ms = wall_clock_now_ms();
    write_history.push_back(std::move(record));
    if (write_history.size() > kMaxWriteHistory) {
        write_history.erase(write_history.begin());
    }

    PendingWrite pending;
    pending.name = name;
    pending.value = value;
    pending.type = type;
    pending.history_id = id;
    pending.send_time = std::chrono::steady_clock::now();
    pending_writes[name] = std::move(pending);
    return id;
}

bool ParamCache::complete_write(
    uint64_t history_id,
    bool success,
    float actual_value,
    const std::string& error) {
    for (auto it = write_history.rbegin(); it != write_history.rend(); ++it) {
        if (it->id != history_id) continue;
        it->state = success ? WriteState::Confirmed : WriteState::Failed;
        it->actual_value = actual_value;
        it->error = error;
        it->completed_at_ms = wall_clock_now_ms();
        return true;
    }
    return false;
}

void ParamCache::on_param_received(uint16_t index) {
    if (download_state == DownloadState::Idle ||
        download_state == DownloadState::Complete ||
        download_state == DownloadState::Failed) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (expected_param_count == 0 || index < expected_param_count) {
        received_indices.insert(index);
    }
    last_receive_time = now;
    missing_indices.erase(index);
    failed_indices.erase(index);
    if (download_state == DownloadState::WaitingList) {
        download_state = DownloadState::Receiving;
        list_retry_count = 0;
    }
    if (download_state == DownloadState::Recovering && missing_indices.empty()) {
        download_state = failed_indices.empty()
            ? DownloadState::Complete
            : DownloadState::Failed;
        download_failed = download_state == DownloadState::Failed;
        download_cancelled = false;
        download_error = download_failed
            ? "parameter download failed after retries"
            : std::string();
    }
}

int ParamCache::check_timeouts(std::vector<std::function<void()>>& deferred_cbs) {
    auto now = std::chrono::steady_clock::now();

    // 1. PARAM_REQUEST_LIST 超时
    if (download_state == DownloadState::WaitingList) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_send_time).count();
        if (elapsed >= PARAM_LIST_TIMEOUT_MS) {
            if (list_retry_count < PARAM_LIST_MAX_RETRIES) {
                list_retry_count++;
                last_send_time = now;
                return 1;
            } else {
                download_state = DownloadState::Failed;
                download_failed = true;
                download_cancelled = false;
                download_error = "parameter list request timed out";
                return 3;
            }
        }
    }

    // 2. 收到部分参数后长时间没有新包：找出缺失索引并进入恢复。
    // ArduPilot 在高负载串口上可能中断 PARAM_REQUEST_LIST 的流；此前
    // Receiving 没有超时分支，导致前端永远停在同一进度。
    if (download_state == DownloadState::Receiving && expected_param_count > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_receive_time).count();
        if (elapsed >= PARAM_RECEIVING_STALL_TIMEOUT_MS) {
            missing_indices.clear();
            for (uint16_t index = 0; index < expected_param_count; index++) {
                if (received_indices.count(index) == 0) {
                    missing_indices.emplace(index, 0);
                }
            }
            if (missing_indices.empty()) {
                download_state = DownloadState::Complete;
                download_failed = false;
                download_cancelled = false;
                download_error.clear();
                return 0;
            }
            batch_queue.clear();
            int count = 0;
            for (const auto& [index, retries] : missing_indices) {
                if (count >= PARAM_BATCH_SIZE) break;
                batch_queue.push_back(index);
                count++;
            }
            download_state = DownloadState::Recovering;
            missing_timeout_start = now;
            return 2;
        }
    }

    // 3. 缺失参数恢复超时
    if (download_state == DownloadState::Recovering) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - missing_timeout_start).count();
        if (elapsed >= PARAM_MISSING_TIMEOUT_MS) {
            for (auto it = missing_indices.begin(); it != missing_indices.end(); ) {
                it->second++;
                if (it->second >= PARAM_MISSING_MAX_RETRIES) {
                    failed_indices.insert(it->first);
                    it = missing_indices.erase(it);
                } else {
                    ++it;
                }
            }
            if (missing_indices.empty()) {
                download_state = failed_indices.empty()
                    ? DownloadState::Complete
                    : DownloadState::Failed;
                download_failed = download_state == DownloadState::Failed;
                download_cancelled = false;
                download_error = download_failed
                    ? "parameter download missing values after retries"
                    : std::string();
                return failed_indices.empty() ? 0 : 3;
            }
            batch_queue.clear();
            int count = 0;
            for (auto& [idx, retries] : missing_indices) {
                if (count >= PARAM_BATCH_SIZE) break;
                batch_queue.push_back(idx);
                count++;
            }
            missing_timeout_start = now;
            return 2;
        }
    }

    // 4. 写入超时
    for (auto it = pending_writes.begin(); it != pending_writes.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second.send_time).count();
        if (elapsed >= PARAM_WRITE_TIMEOUT_MS) {
            if (it->second.retry_count < PARAM_WRITE_MAX_RETRIES) {
                it->second.retry_count++;
                it->second.send_time = now;
            } else {
                if (it->second.result_cb) {
                    auto cb = it->second.result_cb;
                    deferred_cbs.push_back([cb]() { cb(false); });
                }
                complete_write(
                    it->second.history_id,
                    false,
                    it->second.value,
                    "parameter write timed out");
                it = pending_writes.erase(it);
                continue;
            }
        }
        ++it;
    }

    return 0;
}

} // namespace mg::core::mavlink
