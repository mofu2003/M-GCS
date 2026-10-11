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
    // 飞控可能因重传/丢包重发同一条目；按 id 去重，避免列表出现重复行和
    // 计数超过 num_logs。
    for (const auto& existing : log_state.entries) {
        if (existing.id == le.id) return;
    }
    LogEntry entry;
    entry.id = le.id;
    entry.time_utc = le.time_utc;
    entry.size_bytes = le.size;
    log_state.entries.push_back(entry);
    // num_logs 是所有 LOG_ENTRY 都带的总数；用它判定列表是否收齐。
    log_state.list_num_logs = le.num_logs;
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

    const bool all_bytes =
        log_state.download_size > 0 &&
        log_state.download_received_bytes >= log_state.download_size;
    // 空 LOG_DATA（count == 0）是飞控“没有更多数据”的结束信号。
    const bool stream_end = ld.count == 0;
    const bool declared = log_state.download_size_declared;
    if (all_bytes || (stream_end && !declared)) {
        // 延迟文件写入到锁外执行，避免在 Impl::mtx 锁内做阻塞 I/O。
        // P2：完成态在写入成功后由回调置位，写失败通过 download_error 暴露。
        auto filepath = log_state.download_filepath;
        auto data = std::move(log_state.download_data);
        if (log_state.download_size > 0 && data.size() > log_state.download_size) {
            data.resize(log_state.download_size);
        }
        log_state.downloading = false;
        log_state.download_write_pending = true;
        log_state.download_saved_bytes = 0;
        deferred_cbs.push_back([this, filepath, data = std::move(data)]() {
            size_t written = 0;
            std::string error;
            FILE* f = fopen(filepath.c_str(), "wb");
            if (!f) {
                error = "failed to open log file: " + filepath;
            } else {
                written = fwrite(data.data(), 1, data.size(), f);
                if (fclose(f) != 0 && error.empty()) {
                    error = "failed to flush log file: " + filepath;
                }
                if (error.empty() && written != data.size()) {
                    error = "short write: " + std::to_string(written) + "/" +
                            std::to_string(data.size()) + " bytes";
                }
            }
            // 回调在 mtx 之外执行（process()/tick() 出锁后），此处重新加锁更新状态。
            mg::core::platform::LockGuard lk(mtx);
            log_state.download_write_pending = false;
            log_state.download_saved_bytes = static_cast<uint32_t>(written);
            if (error.empty()) {
                log_state.download_completed = true;
            } else {
                log_state.download_error = error;
                MGC_LOG_ERROR("[MavlinkAdapter] Log file write failed: %s", error.c_str());
            }
        });
    } else if (stream_end) {
        // 飞控用空帧表示结束，但已收到的字节少于 LOG_ENTRY.size（大小过期或
        // 记录损坏）。明确报告截断，不写半个文件，也不谎报完成，避免“报完成但
        // 文件不完整”。
        log_state.downloading = false;
        log_state.download_data.clear();
        log_state.download_error = "log truncated: received " +
            std::to_string(log_state.download_received_bytes) + " of " +
            std::to_string(log_state.download_size) + " bytes";
        MGC_LOG_ERROR(
            "[MavlinkAdapter] Log download truncated: id=%u received=%u expected=%u",
            static_cast<unsigned>(log_state.download_id),
            static_cast<unsigned>(log_state.download_received_bytes),
            static_cast<unsigned>(log_state.download_size));
    } else {
        mavlink_msg_log_request_data_pack(255, 0, &msg, log_state.download_target_sys, log_state.download_target_comp,
                                          log_state.download_id, log_state.download_ofs, 90);
        queue_msg();
    }
}

} // namespace mg::core::mavlink
