/**
 * mavlink_log_writer.cc - MAVLink 遥测日志录制实现
 *
 * 每条记录：[uint64 大端 Unix 微秒时间戳][完整 MAVLink 帧字节]。
 * 不再写任何文件头：QGC / Mission Planner 的 .tlog 没有 magic 头，直接以
 * 第一条记录开头；旧版写的 `3E 00 00 00` + uint32 长度前缀既不是 tlog，
 * 也无法被回放器或 QGC/MP 读取。
 */
#include "core/settings/mavlink_log_writer.h"
#include "core/log/tlog_format.h"
#include "core/log/logging.h"
#include <cstring>

namespace mg::core::settings {

MavlinkLogWriter::MavlinkLogWriter() = default;

MavlinkLogWriter::~MavlinkLogWriter() {
    stop_recording();
}

bool MavlinkLogWriter::start_recording(const std::string& filepath) {
    mg::core::platform::LockGuard lk(mtx_);
    if (file_) stop_recording_locked();

    file_ = fopen(filepath.c_str(), "wb");
    if (!file_) {
        MGC_LOG_ERROR("[MavLog] Failed to open %s", filepath.c_str());
        return false;
    }

    filepath_ = filepath;
    bytes_written_ = 0;  // .tlog 无文件头，第一条记录即首帧

    MGC_LOG_INFO("[MavLog] Recording to %s", filepath.c_str());
    return true;
}

void MavlinkLogWriter::stop_recording() {
    mg::core::platform::LockGuard lk(mtx_);
    stop_recording_locked();
}

void MavlinkLogWriter::stop_recording_locked() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
        MGC_LOG_INFO("[MavLog] Stopped recording (%zu bytes)", bytes_written_);
    }
    filepath_.clear();
    bytes_written_ = 0;
}

void MavlinkLogWriter::write_frame(const uint8_t* data, size_t size) {
    mg::core::platform::LockGuard lk(mtx_);
    if (!file_ || !data || size == 0) return;

    // tlog 记录 = 8 字节大端时间戳 + 完整 MAVLink 帧字节，无长度前缀。
    const uint64_t timestamp_us = mg::core::log::tlog::now_unix_micros();
    uint8_t ts_bytes[mg::core::log::tlog::kTimestampBytes];
    mg::core::log::tlog::encode_timestamp(timestamp_us, ts_bytes);

    // 单次写入减少 syscall：绝大多数帧 < 300 字节，4KB 栈缓冲足够。
    bool write_ok = true;
    if (size <= 4096 - mg::core::log::tlog::kTimestampBytes) {
        uint8_t buf[4096];
        memcpy(buf, ts_bytes, mg::core::log::tlog::kTimestampBytes);
        memcpy(buf + mg::core::log::tlog::kTimestampBytes, data, size);
        write_ok = (fwrite(buf, 1, mg::core::log::tlog::kTimestampBytes + size, file_) ==
                    mg::core::log::tlog::kTimestampBytes + size);
    } else {
        write_ok = (fwrite(ts_bytes, 1, mg::core::log::tlog::kTimestampBytes, file_) ==
                    mg::core::log::tlog::kTimestampBytes) &&
                   (fwrite(data, 1, size, file_) == size);
    }
    if (write_ok) {
        bytes_written_ += mg::core::log::tlog::kTimestampBytes + size;
    } else {
        MGC_LOG_ERROR("[MavLog] Write failed (%zu bytes lost)", size);
    }
}

bool MavlinkLogWriter::is_recording() const {
    mg::core::platform::LockGuard lk(mtx_);
    return file_ != nullptr;
}

std::string MavlinkLogWriter::current_file() const {
    mg::core::platform::LockGuard lk(mtx_);
    return filepath_;
}

size_t MavlinkLogWriter::bytes_written() const {
    mg::core::platform::LockGuard lk(mtx_);
    return bytes_written_;
}

} // namespace mg::core::settings
