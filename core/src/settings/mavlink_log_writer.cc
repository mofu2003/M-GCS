/**
 * mavlink_log_writer.cc - MAVLink log recording implementation
 * MAVLink 日志实现 — 原始帧录制，兼容 MP/QGC 格式
 *
 * Writes raw MAVLink frames with file headers compatible with
 * MissionPlanner/QGC log viewers.
 */
#include "core/settings/mavlink_log_writer.h"
#include "core/log/logging.h"
#include <cstring>

namespace mg::core::settings {

// MAVLink log file magic bytes (PX4 .ulog compatible header)
static const uint8_t kLogMagic[] = {0x3E, 0x00, 0x00, 0x00}; // ">"

MavlinkLogWriter::MavlinkLogWriter() = default;

MavlinkLogWriter::~MavlinkLogWriter() {
    stop_recording();
}

bool MavlinkLogWriter::start_recording(const std::string& filepath) {
    mg::core::platform::LockGuard lk(mtx_);
    if (file_) stop_recording_locked();

    file_ = fopen(filepath.c_str(), "wb");
    if (!file_) {
        MGC_LOG_ERROR("[MavLog] Failed to open %s", filepath.c_str());
        return false;
    }

    // Write file header
    if (fwrite(kLogMagic, 1, sizeof(kLogMagic), file_) != sizeof(kLogMagic)) {
        MGC_LOG_ERROR("[MavLog] Failed to write header");
        fclose(file_);
        file_ = nullptr;
        return false;
    }

    filepath_ = filepath;
    bytes_written_ = sizeof(kLogMagic);

    MGC_LOG_INFO("[MavLog] Recording to %s", filepath.c_str());
    return true;
}

void MavlinkLogWriter::stop_recording() {
    mg::core::platform::LockGuard lk(mtx_);
    stop_recording_locked();
}

void MavlinkLogWriter::stop_recording_locked() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
        MGC_LOG_INFO("[MavLog] Stopped recording (%zu bytes)", bytes_written_);
    }
    filepath_.clear();
    bytes_written_ = 0;
}

void MavlinkLogWriter::write_frame(const uint8_t* data, size_t size) {
    mg::core::platform::LockGuard lk(mtx_);
    if (!file_) return;

    uint32_t len = static_cast<uint32_t>(size);

    // Merge header + payload into single write to reduce syscalls.
    // Most MAVLink frames are < 300 bytes, so 4KB stack buffer is safe.
    bool write_ok = true;
    if (size <= 4092) {
        uint8_t buf[4 + 4092];
        memcpy(buf, &len, 4);
        memcpy(buf + 4, data, size);
        write_ok = (fwrite(buf, 1, 4 + size, file_) == 4 + size);
    } else {
        write_ok = (fwrite(&len, 4, 1, file_) == 1) &&
                   (fwrite(data, 1, size, file_) == size);
    }
    if (write_ok) {
        bytes_written_ += 4 + size;
    } else {
        MGC_LOG_ERROR("[MavLog] Write failed (%zu bytes lost)", size);
    }
}

bool MavlinkLogWriter::is_recording() const {
    mg::core::platform::LockGuard lk(mtx_);
    return file_ != nullptr;
}

std::string MavlinkLogWriter::current_file() const {
    mg::core::platform::LockGuard lk(mtx_);
    return filepath_;
}

size_t MavlinkLogWriter::bytes_written() const {
    mg::core::platform::LockGuard lk(mtx_);
    return bytes_written_;
}

} // namespace mg::core::settings
