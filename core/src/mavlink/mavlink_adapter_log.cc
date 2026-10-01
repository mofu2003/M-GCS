/**
 * mavlink_adapter_log.cc - Log download handlers
 *
 * Definitions are out of line. Dispatch, locking, timeout checks,
 * and callback execution semantics are unchanged.
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

void MavlinkAdapter::Impl::handle_log_entry(const mavlink_message_t& m)
{
    mavlink_log_entry_t le;
    mavlink_msg_log_entry_decode(&m, &le);
    LogEntry entry;
    entry.id = le.id;
    entry.time_utc = le.time_utc;
    entry.size_bytes = le.size;
    log_state.entries.push_back(entry);
}

void MavlinkAdapter::Impl::handle_log_data(const mavlink_message_t& m)
{
    if (!log_state.downloading)
        return;
    mavlink_log_data_t ld;
    mavlink_msg_log_data_decode(&m, &ld);
    if (ld.id != log_state.download_id)
        return;

    const uint32_t end = ld.ofs + ld.count;
    // 日志列表提供了已知大小时，忽略飞控偶发发送到文件尾之外的字节，
    // 避免额外字节被计入完成条件或写入本地文件。
    const uint32_t write_end = log_state.download_size > 0 ? std::min(end, log_state.download_size) : end;
    if (write_end > log_state.download_data.size()) {
        log_state.download_data.resize(write_end);
    }
    if (write_end > log_state.download_received.size()) {
        log_state.download_received.resize(write_end, 0);
    }
    for (uint32_t i = 0; i < ld.count; ++i) {
        const auto index = ld.ofs + i;
        if (log_state.download_size > 0 && index >= log_state.download_size) {
            break;
        }
        log_state.download_data[index] = ld.data[i];
        if (!log_state.download_received[index]) {
            log_state.download_received[index] = 1;
            ++log_state.download_received_bytes;
        }
    }
    log_state.download_ofs = write_end;
    log_state.download_last_data_at = std::chrono::steady_clock::now();
    if (log_state.download_size == 0 && ld.count < 90) {
        log_state.download_size = end;
    }

    const bool complete =
        (log_state.download_size > 0 && log_state.download_received_bytes >= log_state.download_size) ||
        (ld.count == 0 && log_state.download_size == 0);
    if (complete) {
        // 延迟文件写入到锁外执行，避免在 Impl::mtx 锁内做阻塞 I/O
        auto filepath = log_state.download_filepath;
        auto data = std::move(log_state.download_data);
        if (log_state.download_size > 0 && data.size() > log_state.download_size) {
            data.resize(log_state.download_size);
        }
        deferred_cbs.push_back([filepath, data = std::move(data)]() {
            FILE* f = fopen(filepath.c_str(), "wb");
            if (f) {
                size_t written = fwrite(data.data(), 1, data.size(), f);
                fclose(f);
                if (written != data.size()) {
                    MGC_LOG_WARN("[MavlinkAdapter] Log file write incomplete: %zu / %zu bytes", written, data.size());
                }
            } else {
                MGC_LOG_ERROR("[MavlinkAdapter] Failed to open log file: %s", filepath.c_str());
            }
        });
        log_state.downloading = false;
        log_state.download_completed = true;
    } else {
        mavlink_msg_log_request_data_pack(255, 0, &msg, log_state.download_target_sys, log_state.download_target_comp,
                                          log_state.download_id, log_state.download_ofs, 90);
        send_msg();
    }
}

} // namespace mg::core::mavlink
