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
    if (ld.count > sizeof(ld.data)) {
        MGC_LOG_WARN("[MavlinkAdapter] LOG_DATA count %u exceeds payload capacity %zu, dropped",
                     static_cast<unsigned>(ld.count), sizeof(ld.data));
        return;
    }

    // ld.ofs 是飞控给的 uint32：先提升到 64 位再相加。直接写 `ld.ofs + ld.count`
    // 会在 ofs 接近 UINT32_MAX 时回绕成小值，使下面按 end 做的 resize 失效，而
    // 循环里的越界保护又只在 download_size > 0 时生效——download_size == 0
    // （LOG_ENTRY 里没有这个 id 的大小）时就会以回绕后的巨大下标写到 vector 之外。
    const uint64_t end64 = static_cast<uint64_t>(ld.ofs) + ld.count;
    // 只接受我们实际请求过的范围：download_ofs 是已请求的前沿，飞控不会提前发送
    // 更靠后的数据，补包也只从更小的缺口重发。越界的 ofs 属于协议异常，丢弃并等
    // 失速重发（见 process_timeouts 的补包），绝不按它扩容或写入。
    if (static_cast<uint64_t>(ld.ofs) > log_state.download_ofs) {
        MGC_LOG_WARN("[MavlinkAdapter] LOG_DATA ofs %u beyond requested %u, dropped",
                     static_cast<unsigned>(ld.ofs),
                     static_cast<unsigned>(log_state.download_ofs));
        return;
    }
    const uint32_t end = static_cast<uint32_t>(end64);
    // 日志列表提供了已知大小时，忽略飞控偶发发送到文件尾之外的字节，
    // 避免额外字节被计入完成条件或写入本地文件。
    const uint32_t write_end = log_state.download_size > 0 ? std::min(end, log_state.download_size) : end;
    if (write_end > log_state.download_data.size()) {
        log_state.download_data.resize(write_end);
    }
    if (write_end > log_state.download_received.size()) {
        log_state.download_received.resize(write_end, 0);
    }
    // 写入上界对两个条件同时生效：不越过本次已 resize 到的 write_end，也不越过
    // 已知文件尾。旧写法只在 download_size > 0 时判断，未知大小时没有上界。
    const uint32_t write_limit = log_state.download_size > 0
        ? std::min(write_end, log_state.download_size)
        : write_end;
    for (uint32_t i = 0; i < ld.count; ++i) {
        const uint32_t index = ld.ofs + i;
        if (index >= write_limit || index >= log_state.download_data.size()) {
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
