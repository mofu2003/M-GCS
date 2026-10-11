#define MGCORE_EXPORTS
/**
 * Camera, gimbal, and video FFI entrypoints.
 *
 * 相机/云台/视频的读接口一律从 MavlinkAdapter 的设备状态缓存取值
 * （与遥测同源；设备状态的唯一事实来源在适配器层，见 S1 设计）。
 *
 * camera_id / gimbal_id 语义（S1）：> 0 视作该设备在活动系统上的组件 id
 * （MAV_COMP_ID_CAMERA = 100 起）；<= 0 表示「活动目标」——读接口退回
 * 第一个已发现的设备，命令接口退回活动目标组件。多设备 UI 选择在 S2 处理。
 */
#include "ffi_internal.h"

#include <cmath>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

namespace {

using mg::core::mavlink::CameraCaptureStatusInfo;
using mg::core::mavlink::CameraInformationInfo;
using mg::core::mavlink::CameraSettingsInfo;
using mg::core::mavlink::GimbalDeviceAttitudeStatusInfo;
using mg::core::mavlink::GimbalDeviceInformationInfo;
using mg::core::mavlink::GimbalManagerInformationInfo;
using mg::core::mavlink::GimbalManagerStatusInfo;
using mg::core::mavlink::MavlinkAdapter;
using mg::core::mavlink::StorageInformationInfo;
using mg::core::mavlink::VideoStreamStatusInfo;

/// 把 camera_id / gimbal_id 解析为 (system, component) 寻址目标。
void resolve_component(mg_core_t* core, int device_id, uint8_t& sys, uint8_t& comp) {
    sys = active_target_sys(core);
    comp = (device_id > 0 && device_id <= 255)
               ? static_cast<uint8_t>(device_id)
               : active_target_comp(core);
}

/// 选择要读取的相机：device_id > 0 按组件 id 匹配，否则取第一个已发现的相机。
std::optional<CameraInformationInfo> select_camera(mg_core_t* core, int camera_id) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return std::nullopt;
    auto cameras = mav->latest_cameras();
    if (camera_id > 0) {
        for (const auto& c : cameras) {
            if (c.component_id == static_cast<uint8_t>(camera_id)) return c;
        }
        return std::nullopt;
    }
    if (cameras.empty()) return std::nullopt;
    return cameras.front();
}

/// 选择要读取的云台管理器：device_id > 0 按组件 id 匹配，否则取第一个已发现的云台。
std::optional<GimbalManagerInformationInfo> select_gimbal(mg_core_t* core, int gimbal_id) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return std::nullopt;
    auto gimbals = mav->latest_gimbals();
    if (gimbal_id > 0) {
        for (const auto& g : gimbals) {
            if (g.component_id == static_cast<uint8_t>(gimbal_id)) return g;
        }
        return std::nullopt;
    }
    if (gimbals.empty()) return std::nullopt;
    return gimbals.front();
}

template <typename Sender>
char* send_payload_command(mg_core_t* core, int device_id, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    uint8_t sys = 0;
    uint8_t comp = 0;
    resolve_component(core, device_id, sys, comp);
    return sender(*mav, sys, comp) ? ok() : fail(MG_ERROR_SEND_FAILED, error_reason);
}

/// 四元数 (w,x,y,z) → 欧拉角 (roll,pitch,yaw)，单位为度；用于给前端一个可直接显示的姿态。
void quaternion_to_euler_deg(const float q[4], double& roll, double& pitch, double& yaw) {
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double kRad2Deg = 180.0 / 3.14159265358979323846;
    roll = std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y)) * kRad2Deg;
    const double sinp = 2.0 * (w * y - z * x);
    pitch = (std::fabs(sinp) >= 1.0 ? std::copysign(90.0, sinp) : std::asin(sinp)) * kRad2Deg;
    yaw = std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z)) * kRad2Deg;
}

} // namespace

MGCORE_API char* mg_camera_take_photo(mg_core_t* core, int camera_id) {
    return send_payload_command(core, camera_id, "camera take photo failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_take_photo(sys, comp);
        });
}

MGCORE_API char* mg_camera_start_video(mg_core_t* core, int camera_id) {
    return send_payload_command(core, camera_id, "camera start video failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_start_video(sys, comp);
        });
}

MGCORE_API char* mg_camera_stop_video(mg_core_t* core, int camera_id) {
    return send_payload_command(core, camera_id, "camera stop video failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_stop_video(sys, comp);
        });
}

MGCORE_API char* mg_camera_set_zoom(mg_core_t* core, int camera_id, float zoom) {
    return send_payload_command(core, camera_id, "camera set zoom failed",
        [zoom](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_set_zoom(sys, comp, 0, zoom);
        });
}

MGCORE_API char* mg_camera_set_mode(mg_core_t* core, int camera_id, int mode) {
    return send_payload_command(core, camera_id, "camera set mode failed",
        [mode](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_set_mode(sys, comp, static_cast<uint8_t>(mode));
        });
}

MGCORE_API char* mg_camera_stop_capture(mg_core_t* core, int camera_id) {
    return send_payload_command(core, camera_id, "camera stop capture failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_stop_capture(sys, comp);
        });
}

MGCORE_API char* mg_camera_set_focus(mg_core_t* core, int camera_id, int focus_type, float focus_value) {
    return send_payload_command(core, camera_id, "camera set focus failed",
        [focus_type, focus_value](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_set_focus(sys, comp, static_cast<uint8_t>(focus_type), focus_value);
        });
}

MGCORE_API char* mg_camera_request_info(mg_core_t* core, int camera_id) {
    return send_payload_command(core, camera_id, "camera request info failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_request_info(sys, comp);
        });
}

MGCORE_API char* mg_camera_get_status(mg_core_t* core, int camera_id) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    auto selected = select_camera(core, camera_id);
    if (!selected) {
        // 「已查询、当前无该相机」是合法状态，不是调用失败（D5）。
        jb.field("available", false);
        auto* mav = get_active_mavlink(core);
        jb.field("count", mav ? static_cast<int>(mav->latest_cameras().size()) : 0);
        jb.end_object();
        return to_cstr(jb.str());
    }

    auto* mav = get_active_mavlink(core);
    const uint8_t sys = selected->system_id;
    const uint8_t comp = selected->component_id;
    std::optional<CameraSettingsInfo> settings = mav->camera_settings(sys, comp);
    std::optional<CameraCaptureStatusInfo> capture = mav->camera_capture_status(sys, comp);
    std::vector<StorageInformationInfo> storage = mav->camera_storage(sys, comp);

    jb.field("available", true);
    jb.field("system_id", static_cast<int>(sys));
    jb.field("component_id", static_cast<int>(comp));
    jb.field("camera_device_id", static_cast<int>(selected->camera_device_id));
    jb.field("vendor_name", selected->vendor_name);
    jb.field("model_name", selected->model_name);
    jb.field("flags", static_cast<long long>(selected->flags));
    jb.field("resolution_h", static_cast<int>(selected->resolution_h));
    jb.field("resolution_v", static_cast<int>(selected->resolution_v));
    jb.field("focal_length", selected->focal_length);
    jb.field("has_photo", (selected->flags & 0x01) != 0);   // CAMERA_CAP_FLAGS_CAPTURE_IMAGE
    jb.field("has_video", (selected->flags & 0x02) != 0);   // CAMERA_CAP_FLAGS_CAPTURE_VIDEO
    jb.field("mode_id", settings ? static_cast<int>(settings->mode_id) : -1);
    jb.field("zoom_level", settings ? settings->zoom_level : 1.0f);
    jb.field("recording", capture ? (capture->video_status == 1) : false);
    jb.field("taking_photo", capture ? (capture->image_status == 1) : false);
    jb.field("photo_count", capture ? static_cast<int>(capture->image_count) : 0);
    jb.field("image_status", capture ? static_cast<int>(capture->image_status) : 0);
    jb.field("video_status", capture ? static_cast<int>(capture->video_status) : 0);
    jb.field("available_capacity_mib", capture ? capture->available_capacity : 0.0f);

    mg::core::telemetry::JsonBuilder st;
    st.begin_object();
    st.field("count", static_cast<int>(storage.size()));
    if (!storage.empty()) {
        const auto& s = storage.front();
        st.field("storage_id", static_cast<int>(s.storage_id));
        st.field("status", static_cast<int>(s.status));
        st.field("total_capacity_mib", s.total_capacity);
        st.field("used_capacity_mib", s.used_capacity);
        st.field("available_capacity_mib", s.available_capacity);
        st.field("name", s.name);
    }
    st.end_object();
    jb.raw_field("storage", st.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_command_gimbal_control(mg_core_t* core, float pitch_deg, float yaw_deg) {
    return send_payload_command(core, 0, "gimbal control failed",
        [pitch_deg, yaw_deg](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.cmd_gimbal_control(sys, comp, pitch_deg, yaw_deg);
        });
}

MGCORE_API char* mg_command_gimbal_recenter(mg_core_t* core) {
    return send_payload_command(core, 0, "gimbal recenter failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.cmd_gimbal_recenter(sys, comp);
        });
}

MGCORE_API char* mg_gimbal_request_info(mg_core_t* core) {
    return send_payload_command(core, 0, "gimbal request info failed",
        [](MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.gimbal_request_info(sys, comp);
        });
}

MGCORE_API char* mg_command_gimbal_configure(mg_core_t* core, int gimbal_device_id,
                                             int primary_sys, int primary_comp,
                                             int secondary_sys, int secondary_comp) {
    return send_payload_command(core, 0, "gimbal configure failed",
        [gimbal_device_id, primary_sys, primary_comp, secondary_sys, secondary_comp](
            MavlinkAdapter& mav, uint8_t sys, uint8_t comp) {
            return mav.cmd_gimbal_configure(sys, comp,
                                            static_cast<uint8_t>(gimbal_device_id),
                                            static_cast<uint8_t>(primary_sys),
                                            static_cast<uint8_t>(primary_comp),
                                            static_cast<uint8_t>(secondary_sys),
                                            static_cast<uint8_t>(secondary_comp));
        });
}

MGCORE_API char* mg_gimbal_get_status(mg_core_t* core, int gimbal_id) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    auto selected = select_gimbal(core, gimbal_id);
    if (!selected) {
        jb.field("available", false);
        auto* mav = get_active_mavlink(core);
        jb.field("count", mav ? static_cast<int>(mav->latest_gimbals().size()) : 0);
        jb.end_object();
        return to_cstr(jb.str());
    }

    auto* mav = get_active_mavlink(core);
    const uint8_t sys = selected->system_id;
    const uint8_t comp = selected->component_id;
    std::optional<GimbalManagerStatusInfo> status = mav->gimbal_manager_status(sys, comp);
    std::optional<GimbalDeviceInformationInfo> device = mav->gimbal_device_information(sys, comp);

    jb.field("available", true);
    jb.field("system_id", static_cast<int>(sys));
    jb.field("component_id", static_cast<int>(comp));
    jb.field("gimbal_device_id", static_cast<int>(selected->gimbal_device_id));
    jb.field("cap_flags", static_cast<long long>(selected->cap_flags));
    jb.field("yaw_min", selected->yaw_min);
    jb.field("yaw_max", selected->yaw_max);
    jb.field("pitch_min", selected->pitch_min);
    jb.field("pitch_max", selected->pitch_max);
    jb.field("manager_flags", status ? static_cast<long long>(status->flags) : 0);
    jb.field("primary_control_sysid", status ? static_cast<int>(status->primary_control_sysid) : 0);
    jb.field("primary_control_compid", status ? static_cast<int>(status->primary_control_compid) : 0);
    jb.field("device_vendor_name", device ? device->vendor_name : "");
    jb.field("device_model_name", device ? device->model_name : "");
    jb.field("device_cap_flags", device ? static_cast<int>(device->cap_flags) : 0);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_gimbal_get_attitude(mg_core_t* core, int gimbal_id) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    auto* mav = get_active_mavlink(core);
    std::optional<GimbalDeviceAttitudeStatusInfo> attitude;
    if (mav) {
        auto gimbals = mav->latest_gimbals();
        if (gimbal_id > 0) {
            for (const auto& g : gimbals) {
                if (g.component_id == static_cast<uint8_t>(gimbal_id)) {
                    attitude = mav->gimbal_device_attitude_status(g.system_id, g.component_id);
                    break;
                }
            }
        } else if (!gimbals.empty()) {
            attitude = mav->gimbal_device_attitude_status(gimbals.front().system_id,
                                                          gimbals.front().component_id);
        }
    }

    if (!attitude) {
        jb.field("available", false);
        jb.end_object();
        return to_cstr(jb.str());
    }

    double roll = 0, pitch = 0, yaw = 0;
    quaternion_to_euler_deg(attitude->q, roll, pitch, yaw);
    jb.field("available", true);
    jb.field("system_id", static_cast<int>(attitude->system_id));
    jb.field("component_id", static_cast<int>(attitude->component_id));
    jb.field("gimbal_device_id", static_cast<int>(attitude->gimbal_device_id));
    jb.field("roll_deg", roll);
    jb.field("pitch_deg", pitch);
    jb.field("yaw_deg", yaw);
    jb.field("angular_velocity_x", attitude->angular_velocity_x);
    jb.field("angular_velocity_y", attitude->angular_velocity_y);
    jb.field("angular_velocity_z", attitude->angular_velocity_z);
    jb.field("failure_flags", static_cast<int>(attitude->failure_flags));
    jb.field("flags", static_cast<long long>(attitude->flags));
    jb.field("time_boot_ms", static_cast<long long>(attitude->time_boot_ms));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_video_start(mg_core_t* core, const char* uri) {
    CHECK_CORE(core);
    if (!uri || uri[0] == '\0')
        return fail(MG_ERROR_INVALID_PARAM, "video uri is required");
    // 本地视频解码后端（S3）尚未实现。此前这里误发 camera_start_video（让相机开始录像），
    // 与「启动一个视频源」语义不符；改为显式返回未支持，避免静默做错事。
    return fail(MG_ERROR_NOT_SUPPORTED,
                "local video decoding backend not implemented yet (planned slice S3)");
}

MGCORE_API char* mg_video_stop(mg_core_t* core) {
    CHECK_CORE(core);
    return fail(MG_ERROR_NOT_SUPPORTED,
                "local video decoding backend not implemented yet (planned slice S3)");
}

MGCORE_API char* mg_video_status(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    auto* mav = get_active_mavlink(core);
    auto streams = mav ? mav->latest_video_streams() : std::vector<mg::core::mavlink::VideoStreamInfo>{};
    auto statuses = mav ? mav->latest_video_stream_status() : std::vector<VideoStreamStatusInfo>{};
    jb.field("available", !streams.empty() || !statuses.empty());
    // 本地解码未实现，S1 阶段不谎报正在播放。
    jb.field("playing", false);
    jb.field("stream_count", static_cast<int>(streams.size()));

    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    for (const auto& s : streams) {
        mg::core::telemetry::JsonBuilder obj;
        obj.begin_object();
        obj.field("stream_id", static_cast<int>(s.stream_id));
        obj.field("type", static_cast<int>(s.type));
        obj.field("name", s.name);
        obj.field("uri", s.uri);
        obj.field("running", (s.flags & 0x01) != 0);
        for (const auto& st : statuses) {
            if (st.stream_id == s.stream_id) {
                obj.field("framerate", static_cast<int>(st.framerate));
                obj.field("bitrate", static_cast<long long>(st.bitrate));
                obj.field("resolution_h", static_cast<int>(st.resolution_h));
                obj.field("resolution_v", static_cast<int>(st.resolution_v));
                break;
            }
        }
        obj.end_object();
        arr.append_json(obj.str());
    }
    arr.end_array();
    jb.raw_field("streams", arr.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_video_get_streams(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");

    auto streams = mav->latest_video_streams();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("count", static_cast<int>(streams.size()));

    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    for (const auto& s : streams) {
        mg::core::telemetry::JsonBuilder obj;
        obj.begin_object();
        obj.field("stream_id", static_cast<int>(s.stream_id));
        obj.field("type", static_cast<int>(s.type));
        obj.field("width", static_cast<int>(s.width));
        obj.field("height", static_cast<int>(s.height));
        obj.field("framerate", static_cast<double>(s.framerate));
        obj.field("bitrate", static_cast<int64_t>(s.bitrate));
        obj.field("rotation", static_cast<int>(s.rotation));
        obj.field("flags", static_cast<int>(s.flags));
        obj.field("name", s.name);
        obj.field("uri", s.uri);
        obj.field("available", (s.flags & 0x01) != 0);
        obj.end_object();
        arr.append_json(obj.str());
    }
    arr.end_array();
    jb.raw_field("streams", arr.str());
    jb.end_object();
    return to_cstr(jb.str());
}
