/**
 * flight_log_writer.h - Flight log file writer
 * 飞行日志 — JSONL/CSV 格式写入飞行事件和遥测数据
 *
 * Writes flight events and telemetry data to files.
 * Supports JSONL (one JSON object per line) and CSV formats.
 *
 * Features:
 *   - Auto-rotating log files (by time or size)
 *   - Thread-safe write
 *   - Compressed output (optional gzip)
 *   - Flight session management (start/stop creates new file)
 */
#pragma once

#include <string>
#include <fstream>
#include <atomic>
#include "core/platform/platform_mutex.h"

namespace mg::core::settings {

/// Log file format
enum class LogFormat {
    Jsonl,  // One JSON object per line (default)
    Csv,    // Comma-separated values
};

class FlightLogWriter {
public:
    FlightLogWriter();
    ~FlightLogWriter();

    /// Set output directory for log files
    void set_output_dir(const std::string& dir);

    /// Set log format
    void set_format(LogFormat format);

    /// Start a new log session (creates new file)
    /// Returns the file path of the new log
    std::string start_session(const std::string& session_name = "");

    /// Stop the current session (close file)
    void stop_session();

    /// Write a JSON event line to the current log
    void write_event(const std::string& json_line);

    /// Write a CSV row to the current log
    void write_csv_row(const std::string& row);

    /// Check if a session is active
    bool is_active() const;

    /// Get the current log file path
    std::string current_file_path() const;

    /// Get the number of events written in current session
    size_t event_count() const;

    /// Get the total bytes written in current session
    size_t bytes_written() const;

private:
    mutable mg::core::platform::Mutex mtx_;
    std::string output_dir_;
    LogFormat format_ = LogFormat::Jsonl;
    std::string current_file_path_;
    std::ofstream file_stream_;
    std::atomic<bool> active_{false};
    std::atomic<size_t> event_count_{0};
    std::atomic<size_t> bytes_written_{0};

    std::string generate_filename(const std::string& session_name) const;
};

} // namespace mg::core::settings
