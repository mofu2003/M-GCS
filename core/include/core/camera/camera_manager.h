/**
 * camera_manager.h - MAVLink Camera protocol
 * 摄像头管理 — MAVLink 相机协议，拍照/录像/设置控制
 *
 * Implements MAVLink Camera Control Interface (WIP).
 * Supports:
 *   - Camera identification
 *   - Photo capture
 *   - Video recording start/stop
 *   - Camera settings (zoom, focus, exposure)
 *
 * Design: inspired by QGC QGCCameraManager.
 */
#pragma once

#include "core/core_types.h"
#include <string>
#include <vector>
#include <functional>
#include "core/platform/platform_mutex.h"

namespace mg::core::camera {

/// Camera information
struct CameraInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    std::string model_name;
    std::string vendor_name;
    float    focal_length_mm = 0;
    float    sensor_width_mm = 0;
    float    sensor_height_mm = 0;
    bool     has_video = false;
    bool     has_photo = true;
    int      photo_count = 0;
};

/// Camera status
struct CameraStatus {
    bool     recording = false;
    bool     taking_photo = false;
    float    zoom_level = 1.0f;
    float    focus_distance = 0;
    int      photo_count = 0;
    std::string mode;  // "photo", "video", "streaming"
};

/// Camera settings
struct CameraSettings {
    float zoom_level = 1.0f;
    float exposure = 0;      // -10 to +10
    float iso = 100;
    float shutter_speed = 0; // 0 = auto
    bool  auto_exposure = true;
};

/// Callback for camera events
using CameraEventCallback = std::function<void(uint8_t system_id, uint8_t component_id,
                                                const std::string& event, const std::string& data)>;

class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    /// Process MAVLink camera messages (CAMERA_INFORMATION, CAMERA_STATUS, etc.)
    void process_message(uint8_t system_id, uint8_t component_id,
                         uint32_t msg_id, const void* msg);

    /// Get camera info for a specific camera.
    CameraInfo get_camera_info(uint8_t system_id, uint8_t component_id) const;

    /// Get camera status.
    CameraStatus get_status(uint8_t system_id, uint8_t component_id) const;

    /// Get all detected cameras.
    std::vector<CameraInfo> get_all_cameras() const;

    /// Take a photo.
    bool take_photo(uint8_t system_id, uint8_t component_id);

    /// Start/stop video recording.
    bool start_video(uint8_t system_id, uint8_t component_id);
    bool stop_video(uint8_t system_id, uint8_t component_id);

    /// Set zoom level.
    bool set_zoom(uint8_t system_id, uint8_t component_id, float zoom_level);

    /// Set camera settings.
    bool apply_settings(uint8_t system_id, uint8_t component_id, const CameraSettings& settings);

    /// Set event callback.
    void set_event_callback(CameraEventCallback cb);

private:
    struct CameraState {
        CameraInfo info;
        CameraStatus status;
        CameraSettings settings;
    };

    mutable mg::core::platform::Mutex mtx_;
    std::vector<CameraState> cameras_;
    CameraEventCallback event_cb_;

    CameraState* find_camera(uint8_t sys_id, uint8_t comp_id);
    const CameraState* find_camera(uint8_t sys_id, uint8_t comp_id) const;
    void fire_event(uint8_t sys_id, uint8_t comp_id, const std::string& event, const std::string& data);
};

} // namespace mg::core::camera
