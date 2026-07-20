/**
 * camera_manager.cc - MAVLink Camera protocol implementation
 * 摄像头管理实现 — 相机状态管理和消息处理
 *
 * 处理 CAMERA_INFORMATION (261)、CAMERA_CAPTURE_STATUS (254)、
 * CAMERA_IMAGE_CAPTURED (263) 消息，维护相机状态。
 */
#include "core/camera/camera_manager.h"
#include "core/log/logging.h"
#include "common/mavlink.h"
#include <cstring>

namespace mg::core::camera {

CameraManager::CameraManager() = default;
CameraManager::~CameraManager() = default;

void CameraManager::process_message(uint8_t system_id, uint8_t component_id,
                                     uint32_t msg_id, const void* msg) {
    if (!msg) return;
    mg::core::platform::LockGuard lk(mtx_);

    switch (msg_id) {
        case 261: {  // CAMERA_INFORMATION
            auto* ci = static_cast<const mavlink_camera_information_t*>(msg);
            auto* cam = find_camera(system_id, component_id);
            if (!cam) return;
            cam->info.model_name = std::string(reinterpret_cast<const char*>(ci->model_name), strnlen(reinterpret_cast<const char*>(ci->model_name), 32));
            cam->info.vendor_name = std::string(reinterpret_cast<const char*>(ci->vendor_name), strnlen(reinterpret_cast<const char*>(ci->vendor_name), 32));
            cam->info.focal_length_mm = ci->focal_length;
            cam->info.sensor_width_mm = ci->sensor_size_h;
            cam->info.sensor_height_mm = ci->sensor_size_v;
            cam->info.has_video = (ci->flags & CAMERA_CAP_FLAGS_CAPTURE_VIDEO) != 0;
            cam->info.has_photo = (ci->flags & CAMERA_CAP_FLAGS_CAPTURE_IMAGE) != 0;
            MGC_LOG_INFO("[Camera] CAMERA_INFORMATION: %s %s (video=%d photo=%d)",
                         cam->info.vendor_name.c_str(), cam->info.model_name.c_str(),
                         cam->info.has_video, cam->info.has_photo);
            break;
        }
        case 254: {  // CAMERA_CAPTURE_STATUS
            auto* cs = static_cast<const mavlink_camera_capture_status_t*>(msg);
            auto* cam = find_camera(system_id, component_id);
            if (!cam) return;
            cam->status.recording = (cs->video_status == 1);  // 1 = recording
            cam->status.taking_photo = (cs->image_status == 1);  // 1 = capture in progress
            cam->status.photo_count = cs->image_count;
            cam->status.recording = (cs->video_status == 1);
            break;
        }
        case 263: {  // CAMERA_IMAGE_CAPTURED
            auto* cam = find_camera(system_id, component_id);
            if (!cam) return;
            cam->info.photo_count++;
            cam->status.taking_photo = false;
            cam->status.photo_count = cam->info.photo_count;
            MGC_LOG_INFO("[Camera] Photo captured: count=%d", cam->info.photo_count);
            fire_event(system_id, component_id, "photo_captured",
                       "{\"count\":" + std::to_string(cam->info.photo_count) + "}");
            break;
        }
        default:
            break;
    }
}

CameraInfo CameraManager::get_camera_info(uint8_t system_id, uint8_t component_id) const {
    mg::core::platform::LockGuard lk(mtx_);
    auto* cam = find_camera(system_id, component_id);
    return cam ? cam->info : CameraInfo{};
}

CameraStatus CameraManager::get_status(uint8_t system_id, uint8_t component_id) const {
    mg::core::platform::LockGuard lk(mtx_);
    auto* cam = find_camera(system_id, component_id);
    return cam ? cam->status : CameraStatus{};
}

std::vector<CameraInfo> CameraManager::get_all_cameras() const {
    mg::core::platform::LockGuard lk(mtx_);
    std::vector<CameraInfo> result;
    for (auto& c : cameras_) {
        result.push_back(c.info);
    }
    return result;
}

bool CameraManager::take_photo(uint8_t system_id, uint8_t component_id) {
    MGC_LOG_INFO("[Camera] Take photo: %d.%d", system_id, component_id);
    fire_event(system_id, component_id, "take_photo", "{}");
    return true;
}

bool CameraManager::start_video(uint8_t system_id, uint8_t component_id) {
    MGC_LOG_INFO("[Camera] Start video: %d.%d", system_id, component_id);
    auto* cam = find_camera(system_id, component_id);
    if (cam) cam->status.recording = true;
    fire_event(system_id, component_id, "start_video", "{}");
    return true;
}

bool CameraManager::stop_video(uint8_t system_id, uint8_t component_id) {
    MGC_LOG_INFO("[Camera] Stop video: %d.%d", system_id, component_id);
    auto* cam = find_camera(system_id, component_id);
    if (cam) cam->status.recording = false;
    fire_event(system_id, component_id, "stop_video", "{}");
    return true;
}

bool CameraManager::set_zoom(uint8_t system_id, uint8_t component_id, float zoom_level) {
    MGC_LOG_INFO("[Camera] Set zoom: %d.%d = %.1f", system_id, component_id, zoom_level);
    mg::core::platform::LockGuard lk(mtx_);
    auto* cam = find_camera(system_id, component_id);
    if (cam) cam->status.zoom_level = zoom_level;
    return true;
}

bool CameraManager::apply_settings(uint8_t system_id, uint8_t component_id,
                                    const CameraSettings& settings) {
    MGC_LOG_INFO("[Camera] Apply settings: %d.%d zoom=%.1f iso=%.0f",
                 system_id, component_id, settings.zoom_level, settings.iso);
    return true;
}

void CameraManager::set_event_callback(CameraEventCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    event_cb_ = std::move(cb);
}

CameraManager::CameraState* CameraManager::find_camera(uint8_t sys_id, uint8_t comp_id) {
    for (auto& c : cameras_) {
        if (c.info.system_id == sys_id && c.info.component_id == comp_id) {
            return &c;
        }
    }
    // Auto-create on first access
    CameraState state;
    state.info.system_id = sys_id;
    state.info.component_id = comp_id;
    cameras_.push_back(state);
    return &cameras_.back();
}

const CameraManager::CameraState* CameraManager::find_camera(uint8_t sys_id, uint8_t comp_id) const {
    for (const auto& c : cameras_) {
        if (c.info.system_id == sys_id && c.info.component_id == comp_id) {
            return &c;
        }
    }
    return nullptr;
}

void CameraManager::fire_event(uint8_t sys_id, uint8_t comp_id,
                                const std::string& event, const std::string& data) {
    CameraEventCallback cb;
    {
        mg::core::platform::LockGuard lk(mtx_);
        cb = event_cb_;
    }
    if (cb) {
        cb(sys_id, comp_id, event, data);
    }
}

} // namespace mg::core::camera
