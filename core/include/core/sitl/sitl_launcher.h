/**
 * sitl_launcher.h - SITL (Software In The Loop) integration
 * SITL 仿真 — 启动和管理 ArduPilot 仿真进程
 *
 * Launches and manages ArduPilot SITL simulator processes.
 * Supports:
 *   - Start/stop SITL
 *   - Connection management
 *   - Vehicle type selection
 *   - Weather/terrain configuration
 *
 * Design: inspired by MP SITL.cs.
 */
#pragma once

#include "core/core_types.h"
#include "core/platform/platform_mutex.h"
#include <string>
#include <functional>
#include <cstdint>

namespace mg::core::sitl {

/// SITL vehicle type
enum class SitlVehicle : uint8_t {
    Copter,
    Plane,
    Rover,
    Sub,
    Tracker,
};

/// SITL status
enum class SitlStatus : uint8_t {
    Stopped,
    Starting,
    Running,
    Error,
};

/// SITL configuration
struct SitlConfig {
    SitlVehicle vehicle = SitlVehicle::Copter;
    int    port = 5760;           // TCP port for SITL
    int    udp_port = 14550;      // UDP port for MAVLink
    std::string home;             // Home position "lat,lon,alt,heading"
    double speedup = 1.0;         // Simulation speed multiplier
    bool   use_terrain = false;   // Enable terrain following
};

/// Callback for SITL status changes
using SitlStatusCallback = std::function<void(SitlStatus status, const std::string& message)>;

class SitlLauncher {
public:
    SitlLauncher();
    ~SitlLauncher();

    /// Start SITL simulator.
    bool start(const SitlConfig& config);

    /// Stop SITL simulator.
    void stop();

    /// Get current status.
    SitlStatus status() const;

    /// Get SITL connection endpoint (e.g. "tcp://127.0.0.1:5760").
    std::string get_endpoint() const;

    /// Get SITL process ID (0 if not running).
    int get_pid() const;

    /// Set status callback.
    void set_status_callback(SitlStatusCallback cb);

    /// Check if SITL is available on this system.
    static bool is_available();

private:
    mutable mg::core::platform::Mutex mtx_;
    SitlConfig config_;
    SitlStatus status_ = SitlStatus::Stopped;
    int pid_ = 0;
    SitlStatusCallback status_cb_;

#ifdef _WIN32
    void* process_handle_ = nullptr;  // HANDLE (avoid windows.h in header)
#endif

    /// 持锁时只更新状态与日志，不触发回调；回调由调用方在解锁后执行，
    /// 避免在持锁路径调用外部回调（A3）。
    void set_status_locked(SitlStatus s, const std::string& msg);
};

} // namespace mg::core::sitl
