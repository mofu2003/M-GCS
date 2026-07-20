/**
 * flight_logging_service.h - 飞行日志服务 — 管理日志启停、事件写入
 */
#pragma once

#include <atomic>
#include "core/platform/platform_mutex.h"
#include <functional>
#include <string>

namespace mg::core::settings {

class FlightLoggingService {
public:
    using WriteCallback = std::function<void(const std::string& json_event_line)>;

    FlightLoggingService();
    ~FlightLoggingService();

    void set_write_callback(WriteCallback cb);
    [[nodiscard]] std::string status_json() const;
    [[nodiscard]] bool is_active() const;

    bool start();
    bool stop();

    bool append_event(const std::string& source, int severity, const std::string& text);

    void on_statustext(int severity, const std::string& text);
    void on_connection_state(const std::string& state, const std::string& reason);
    void on_command_result(const std::string& command, bool accepted, const std::string& detail);

private:
    WriteCallback write_cb_;
    std::atomic<bool> active_{false};
    mutable mg::core::platform::Mutex mtx_;
    std::atomic<size_t> event_count_{0};
    std::atomic<uint64_t> started_at_ms_{0};
};

} // namespace mg::core::settings