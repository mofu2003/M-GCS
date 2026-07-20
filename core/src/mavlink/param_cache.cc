/**
 * param_cache.cc - 参数缓存状态机实现
 */
#include "param_cache.h"

namespace mg::core::mavlink {

void ParamCache::start_download(uint8_t sys, uint8_t comp) {
    download_state = DownloadState::WaitingList;
    list_retry_count = 0;
    target_sys = sys;
    target_comp = comp;
    last_send_time = std::chrono::steady_clock::now();
    missing_indices.clear();
    failed_indices.clear();
    batch_queue.clear();
    pending_writes.clear();
}

void ParamCache::on_param_received(uint16_t index) {
    missing_indices.erase(index);
    if (download_state == DownloadState::WaitingList) {
        download_state = DownloadState::Receiving;
        list_retry_count = 0;
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
                return 3;
            }
        }
    }

    // 2. 缺失参数恢复超时
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

    // 3. 写入超时
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
                it = pending_writes.erase(it);
                continue;
            }
        }
        ++it;
    }

    return 0;
}

} // namespace mg::core::mavlink
