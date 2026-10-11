/**
 * mavlink_log_writer.h - MAVLink 遥测日志录制（.tlog）
 *
 * 把链路上的原始 MAVLink 帧写成 QGC / Mission Planner 通用的 .tlog 文件，
 * 每条记录为 `[uint64 大端 Unix 微秒时间戳][完整 MAVLink 帧字节]`。
 * 格式契约见 core/log/tlog_format.h，回放由 FlightLogReplayer 负责。
 */
#pragma once

#include <string>
#include <cstdint>
#include <cstdio>
#include "core/platform/platform_mutex.h"

namespace mg::core::settings {

class MavlinkLogWriter {
public:
    MavlinkLogWriter();
    ~MavlinkLogWriter();

    /// Start recording to a file. Returns true on success.
    bool start_recording(const std::string& filepath);

    /// Stop recording.
    void stop_recording();

    /// Write a raw MAVLink frame (header + payload + checksum).
    void write_frame(const uint8_t* data, size_t size);

    /// Check if currently recording.
    bool is_recording() const;

    /// Get current file path.
    std::string current_file() const;

    /// Get total bytes written.
    size_t bytes_written() const;

private:
    mutable mg::core::platform::Mutex mtx_;
    FILE* file_ = nullptr;
    std::string filepath_;
    size_t bytes_written_ = 0;

    /// Internal stop implementation (caller must hold mtx_).
    void stop_recording_locked();
};

} // namespace mg::core::settings
