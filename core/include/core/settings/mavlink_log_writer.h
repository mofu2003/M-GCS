/**
 * mavlink_log_writer.h - MAVLink log recording
 * MAVLink 日志 — 录制原始 MAVLink 帧到 .bin 文件，兼容 MP/QGC
 *
 * Records raw MAVLink frames to .bin files for offline analysis.
 * Compatible with MissionPlanner and QGC log formats.
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
