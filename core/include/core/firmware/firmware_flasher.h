/**
 * firmware_flasher.h - Firmware flashing via MAVLink
 * 固件刷写 — 通过 MAVLink FTP 或 Bootloader 协议刷写固件
 *
 * Flashes ArduPilot/PX4 firmware via MAVLink FTP or bootloader protocol.
 * Supports:
 *   - Firmware version detection
 *   - Firmware download from server
 *   - Flash via MAVLink FTP
 *   - Progress reporting
 *
 * Design: inspired by MP ConfigFirmware and QGC FirmwarePlugin.
 */
#pragma once

#include "core/core_types.h"
// A2：firmware 不能依赖 transport。这里只需要指针，前置声明即可，
// 否则会形成 transport -> mavlink -> firmware -> transport 的模块环（KD-21）。
namespace mg::core::transport { class ConnectionManager; }
#include "core/platform/platform_mutex.h"
#include <string>
#include <functional>
#include <cstdint>

namespace mg::core::firmware {

/// Firmware type
enum class FirmwareType : uint8_t {
    ArduPilot,
    PX4,
    Unknown,
};

/// Flash progress
struct FlashProgress {
    uint8_t  progress_pct = 0;     // 0-100
    std::string status;            // "downloading", "flashing", "verifying"
    std::string error;
    bool     complete = false;
};

/// Callback for flash progress
using FlashProgressCallback = std::function<void(const FlashProgress&)>;

class FirmwareFlasher {
public:
    FirmwareFlasher();
    ~FirmwareFlasher();

    /// Detect current firmware type and version.
    FirmwareType detect_firmware(uint8_t autopilot_type) const;

    /// Get firmware download URL for a vehicle type.
    std::string get_firmware_url(FirmwareType type, uint8_t vehicle_type,
                                  const std::string& version = "stable") const;

    /// Download firmware binary to a local file.
    /// Returns the local file path on success.
    std::string download_firmware(const std::string& url, const std::string& dest_dir);

    /// Flash firmware via MAVLink FTP.
    bool flash_firmware(mg::core::transport::ConnectionManager* conn,
                        const std::string& firmware_path,
                        FlashProgressCallback progress_cb = nullptr);

    /// Set progress callback.
    void set_progress_callback(FlashProgressCallback cb);

    /// Cancel ongoing flash operation.
    void cancel();

    /// Check if currently flashing.
    bool is_flashing() const;

private:
    mutable mg::core::platform::Mutex mtx_;
    FlashProgressCallback progress_cb_;
    bool flashing_ = false;
    bool cancel_requested_ = false;

    void report_progress(uint8_t pct, const std::string& status);
};

} // namespace mg::core::firmware
