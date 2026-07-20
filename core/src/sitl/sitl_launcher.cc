/**
 * sitl_launcher.cc - SITL launcher implementation
 * SITL 仿真实现 — 平台相关进程管理
 *
 * Platform-specific SITL process management.
 * Placeholder: Full implementation requires process spawning API.
 */
#include "core/sitl/sitl_launcher.h"
#include "core/log/logging.h"
#include <sstream>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

namespace mg::core::sitl {

SitlLauncher::SitlLauncher() = default;
SitlLauncher::~SitlLauncher() { stop(); }

bool SitlLauncher::start(const SitlConfig& config) {
    mg::core::platform::LockGuard lk(mtx_);
    if (status_ == SitlStatus::Running) return false;

    config_ = config;
    set_status(SitlStatus::Starting, "launching SITL...");

    // Build SITL command arguments
    std::string vehicle;
    switch (config.vehicle) {
        case SitlVehicle::Copter:   vehicle = "ArduCopter"; break;
        case SitlVehicle::Plane:    vehicle = "ArduPlane";  break;
        case SitlVehicle::Rover:    vehicle = "APMrover2";  break;
        case SitlVehicle::Sub:      vehicle = "ArduSub";    break;
        case SitlVehicle::Tracker:  vehicle = "AntennaTracker"; break;
    }

    std::string out_arg = "udp:127.0.0.1:" + std::to_string(config.udp_port);

    MGC_LOG_INFO("[SITL] Vehicle: %s, UDP: %s", vehicle.c_str(), out_arg.c_str());

#ifdef _WIN32
    // Windows: use CreateProcess with proper argument splitting
    // Build command line for python + sim_vehicle.py
    std::string py_script = "sim_vehicle.py";
    std::string py_args = "-v " + vehicle + " --no-mavproxy --out " + out_arg;
    if (!config.home.empty()) {
        py_args += " --home " + config.home;
    }
    if (config.speedup != 1.0) {
        py_args += " --speedup " + std::to_string(config.speedup);
    }

    std::string full_cmd = "python " + py_script + " " + py_args;
    MGC_LOG_INFO("[SITL] Command: %s", full_cmd.c_str());

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    char cmd_buf[2048];
    strncpy(cmd_buf, full_cmd.c_str(), sizeof(cmd_buf) - 1);
    cmd_buf[sizeof(cmd_buf) - 1] = '\0';

    if (CreateProcessA(nullptr, cmd_buf, nullptr, nullptr, FALSE,
                       CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        process_handle_ = pi.hProcess;  // Retain handle for proper cleanup
        pid_ = static_cast<int>(pi.dwProcessId);
    } else {
        pid_ = -1;
    }
#else
    // POSIX: use fork + execvp with proper argv splitting
    std::vector<std::string> args = {"sim_vehicle.py", "-v", vehicle, "--no-mavproxy",
                                      "--out", out_arg};
    if (!config.home.empty()) {
        args.push_back("--home");
        args.push_back(config.home);
    }
    if (config.speedup != 1.0) {
        args.push_back("--speedup");
        args.push_back(std::to_string(config.speedup));
    }

    std::vector<char*> argv;
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_ = fork();
    if (pid_ == 0) {
        execvp("sim_vehicle.py", argv.data());
        _exit(1);
    }
#endif

    if (pid_ > 0) {
        set_status(SitlStatus::Running, "SITL started (PID=" + std::to_string(pid_) + ")");
        return true;
    }

    set_status(SitlStatus::Error, "failed to start SITL process");
    return false;
}

void SitlLauncher::stop() {
    mg::core::platform::LockGuard lk(mtx_);
    if (pid_ <= 0) return;

    MGC_LOG_INFO("[SITL] Stopping PID=%d", pid_);

#ifdef _WIN32
    if (process_handle_) {
        TerminateProcess(static_cast<HANDLE>(process_handle_), 0);
        CloseHandle(static_cast<HANDLE>(process_handle_));
        process_handle_ = nullptr;
    }
#else
    kill(pid_, SIGTERM);
    int status = 0;
    waitpid(pid_, &status, 0);  // Reap child to avoid zombie
#endif

    pid_ = 0;
    set_status(SitlStatus::Stopped, "SITL stopped");
}

SitlStatus SitlLauncher::status() const {
    mg::core::platform::LockGuard lk(mtx_);
    return status_;
}

std::string SitlLauncher::get_endpoint() const {
    mg::core::platform::LockGuard lk(mtx_);
    return "tcp://127.0.0.1:" + std::to_string(config_.port);
}

int SitlLauncher::get_pid() const {
    mg::core::platform::LockGuard lk(mtx_);
    return pid_;
}

void SitlLauncher::set_status_callback(SitlStatusCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    status_cb_ = std::move(cb);
}

bool SitlLauncher::is_available() {
#ifdef _WIN32
    // Windows: use 'where' to find sim_vehicle.py
    int ret = system("where sim_vehicle.py >nul 2>&1");
    if (ret == 0) return true;
    // Also check via python -m
    ret = system("python -c \"import sys; sys.exit(0 if __import__('importlib').util.find_spec('pymavlink') else 1)\" >nul 2>&1");
    return ret == 0;
#else
    int ret = system("which sim_vehicle.py > /dev/null 2>&1");
    return ret == 0;
#endif
}

void SitlLauncher::set_status(SitlStatus s, const std::string& msg) {
    status_ = s;
    MGC_LOG_INFO("[SITL] %s", msg.c_str());
    if (status_cb_) status_cb_(s, msg);
}

} // namespace mg::core::sitl
