/**
 * firmware_flasher.cc - Firmware flashing implementation
 * 固件刷写实现 — 当前为占位实现，待集成 HTTP 下载和 MAVLink FTP
 *
 * Scaffold implementation. Full firmware flashing requires:
 * 1. HTTP client for downloading firmware
 * 2. MAVLink FTP for file transfer
 * 3. Bootloader protocol for flashing
 */
#include "core/firmware/firmware_flasher.h"
#include "core/log/logging.h"
#include <mutex>

namespace mg::core::firmware {

FirmwareFlasher::FirmwareFlasher() = default;
FirmwareFlasher::~FirmwareFlasher() = default;

FirmwareType FirmwareFlasher::detect_firmware(uint8_t autopilot_type) const {
    switch (autopilot_type) {
        case 3:  return FirmwareType::ArduPilot; // MAV_AUTOPILOT_ARDUPILOTMEGA
        case 12: return FirmwareType::PX4;       // MAV_AUTOPILOT_PX4
        default: return FirmwareType::Unknown;
    }
}

std::string FirmwareFlasher::get_firmware_url(FirmwareType type, uint8_t vehicle_type,
                                                const std::string& version) const {
    // Production path should query the firmware server instead of constructing
    // a best-effort URL locally.
    std::string base_url = "https://firmware.ardupilot.org";
    std::string vehicle;

    switch (type) {
        case FirmwareType::ArduPilot:
            switch (vehicle_type) {
                case 2:  vehicle = "Copter"; break;  // MAV_TYPE_QUADROTOR
                case 1:  vehicle = "Plane"; break;   // MAV_TYPE_FIXED_WING
                case 10: vehicle = "Rover"; break;   // MAV_TYPE_GROUND_ROVER
                default: vehicle = "Copter"; break;
            }
            return base_url + "/" + vehicle + "/" + version + "/";
        case FirmwareType::PX4:
            return "https://px4-autopilot.s3.us-east-2.amazonaws.com/";
        default:
            return "";
    }
}

std::string FirmwareFlasher::download_firmware(const std::string& url, const std::string& dest_dir) {
    MGC_LOG_INFO("[Firmware] Download: %s -> %s", url.c_str(), dest_dir.c_str());
    // Requires a production HTTP client before this can return a firmware path.
    report_progress(0, "download not implemented");
    return "";
}

bool FirmwareFlasher::flash_firmware(mg::core::transport::ConnectionManager* conn,
                                      const std::string& firmware_path,
                                      FlashProgressCallback progress_cb) {
    mg::core::platform::LockGuard lk(mtx_);
    if (flashing_) return false;

    flashing_ = true;
    cancel_requested_ = false;
    progress_cb_ = std::move(progress_cb);

    MGC_LOG_INFO("[Firmware] Flash: %s", firmware_path.c_str());
    report_progress(0, "flash not implemented");

    flashing_ = false;
    return false;
}

void FirmwareFlasher::set_progress_callback(FlashProgressCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    progress_cb_ = std::move(cb);
}

void FirmwareFlasher::cancel() {
    mg::core::platform::LockGuard lk(mtx_);
    cancel_requested_ = true;
}

bool FirmwareFlasher::is_flashing() const {
    mg::core::platform::LockGuard lk(mtx_);
    return flashing_;
}

void FirmwareFlasher::report_progress(uint8_t pct, const std::string& status) {
    if (progress_cb_) {
        FlashProgress prog;
        prog.progress_pct = pct;
        prog.status = status;
        prog.complete = (pct >= 100);
        progress_cb_(prog);
    }
}

} // namespace mg::core::firmware
