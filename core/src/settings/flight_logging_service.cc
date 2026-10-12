/**
 * flight_logging_service.cc - 飞行日志服务实现
 */
#include "core/settings/flight_logging_service.h"
#include "core/telemetry/json_builder.h"
#include "core/platform/platform_mutex.h"
#include "core/log/logging.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace mg::core::settings {

FlightLoggingService::FlightLoggingService() = default;
FlightLoggingService::~FlightLoggingService() { stop(); }

std::string FlightLoggingService::status_json() const {
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("active", active_.load());
    jb.field("events", static_cast<int64_t>(event_count_.load()));
    jb.field("started_at_ms", static_cast<int64_t>(started_at_ms_.load()));
    jb.end_object();
    return jb.str();
}

bool FlightLoggingService::is_active() const {
    return active_.load();
}

bool FlightLoggingService::start() {
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (active_.load()) return true;
        event_count_ = 0;
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        started_at_ms_ = static_cast<uint64_t>(now);
        active_ = true;
    }
    append_event("system", 7, "flight logging started");
    MGC_LOG_INFO("[FlightLog] Started");
    return true;
}

bool FlightLoggingService::stop() {
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (!active_.load()) return true;
    }
    append_event("system", 7, "flight logging stopped");
    active_ = false;
    MGC_LOG_INFO("[FlightLog] Stopped (%zu events)", event_count_.load());
    return true;
}

bool FlightLoggingService::append_event(const std::string& source, int severity, const std::string& text) {
    // 锁内只做记账和 JSON 组装；写回调必须在锁外调用，否则会在
    // MAVLink 读线程上持锁进入外层互斥量与 FFI 事件回调（仓库禁区）。
    WriteCallback cb;
    std::string line;
    {
        mg::core::platform::LockGuard lk(mtx_);
        if (!active_.load()) return false;
        event_count_++;
        if (!write_cb_) return true;

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");

        mg::core::telemetry::JsonBuilder jb;
        jb.begin_object();
        jb.field("timestamp", ts.str());
        jb.field("source", source);
        jb.field("severity", severity);
        jb.field("text", text);
        jb.end_object();
        line = jb.str();
        cb = write_cb_;
    }
    cb(line);
    return true;
}

void FlightLoggingService::on_statustext(int severity, const std::string& text) {
    append_event("fc.statustext", severity, text);
}

void FlightLoggingService::on_connection_state(const std::string& state, const std::string& reason) {
    append_event("connection", 7, state + ":" + reason);
}

void FlightLoggingService::on_command_result(const std::string& command, bool accepted, const std::string& detail) {
    append_event("command", accepted ? 6 : 4, command + ":" + (accepted ? "accepted" : detail));
}

} // namespace mg::core::settings
