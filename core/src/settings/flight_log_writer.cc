/**
 * flight_log_writer.cc - Flight log file writer implementation
 * 飞行日志实现 — JSONL/CSV 写入、自动轮转、会话管理
 */
#include "core/settings/flight_log_writer.h"
#include "core/log/logging.h"
#include "core/platform/platform_mutex.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace mg::core::settings {

FlightLogWriter::FlightLogWriter() {
    // Default output dir: ./flight_logs/
    output_dir_ = "flight_logs";
}

FlightLogWriter::~FlightLogWriter() {
    stop_session();
}

void FlightLogWriter::set_output_dir(const std::string& dir) {
    mg::core::platform::LockGuard lk(mtx_);
    output_dir_ = dir;
}

void FlightLogWriter::set_format(LogFormat format) {
    mg::core::platform::LockGuard lk(mtx_);
    format_ = format;
}

std::string FlightLogWriter::start_session(const std::string& session_name) {
    mg::core::platform::LockGuard lk(mtx_);

    // Inline cleanup instead of calling stop_session() (avoids nested lock deadlock)
    if (active_.load()) {
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        MGC_LOG_INFO("[LogWriter] Previous session closed: %zu events, %zu bytes",
                     event_count_.load(), bytes_written_.load());
        active_.store(false);
    }

    // Create output directory if needed
    fs::create_directories(output_dir_);

    // Generate filename
    current_file_path_ = generate_filename(session_name);

    // Open file
    file_stream_.open(current_file_path_, std::ios::out | std::ios::trunc);
    if (!file_stream_.is_open()) {
        MGC_LOG_ERROR("[LogWriter] Failed to open %s", current_file_path_.c_str());
        return "";
    }

    active_.store(true);
    event_count_.store(0);
    bytes_written_.store(0);

    // Write header for CSV format
    if (format_ == LogFormat::Csv) {
        file_stream_ << "timestamp_ms,source,severity,text" << std::endl;
    }

    MGC_LOG_INFO("[LogWriter] Session started: %s", current_file_path_.c_str());
    return current_file_path_;
}

void FlightLogWriter::stop_session() {
    mg::core::platform::LockGuard lk(mtx_);

    if (!active_.load()) return;

    if (file_stream_.is_open()) {
        file_stream_.close();
    }

    MGC_LOG_INFO("[LogWriter] Session stopped: %zu events, %zu bytes",
                 event_count_.load(), bytes_written_.load());
    active_.store(false);
}

void FlightLogWriter::write_event(const std::string& json_line) {
    mg::core::platform::LockGuard lk(mtx_);
    if (!active_.load() || !file_stream_.is_open()) return;

    file_stream_ << json_line << "\n";
    file_stream_.flush();
    event_count_++;
    bytes_written_ += json_line.size() + 1;
}

void FlightLogWriter::write_csv_row(const std::string& row) {
    mg::core::platform::LockGuard lk(mtx_);
    if (!active_.load() || !file_stream_.is_open()) return;

    file_stream_ << row << "\n";
    file_stream_.flush();
    event_count_++;
    bytes_written_ += row.size() + 1;
}

bool FlightLogWriter::is_active() const {
    return active_.load();
}

std::string FlightLogWriter::current_file_path() const {
    mg::core::platform::LockGuard lk(mtx_);
    return current_file_path_;
}

size_t FlightLogWriter::event_count() const {
    return event_count_.load();
}

size_t FlightLogWriter::bytes_written() const {
    return bytes_written_.load();
}

std::string FlightLogWriter::generate_filename(const std::string& session_name) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << output_dir_ << "/";

    if (!session_name.empty()) {
        oss << session_name << "_";
    }

    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");

    if (format_ == LogFormat::Jsonl) {
        oss << ".jsonl";
    } else {
        oss << ".csv";
    }

    return oss.str();
}

} // namespace mg::core::settings
