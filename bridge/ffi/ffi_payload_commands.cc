#define MGCORE_EXPORTS
/**
 * Camera, gimbal, and video FFI entrypoints.
 */
#include "ffi_internal.h"

namespace {

template <typename Sender>
char* send_active_payload_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}

} // namespace

MGCORE_API char* mg_camera_take_photo(mg_core_t* core, int camera_id) {
    (void)camera_id;
    return send_active_payload_request(core, "camera take photo failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_take_photo(sys, comp);
        });
}

MGCORE_API char* mg_camera_start_video(mg_core_t* core, int camera_id) {
    (void)camera_id;
    return send_active_payload_request(core, "camera start video failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_start_video(sys, comp);
        });
}

MGCORE_API char* mg_camera_stop_video(mg_core_t* core, int camera_id) {
    (void)camera_id;
    return send_active_payload_request(core, "camera stop video failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_stop_video(sys, comp);
        });
}

MGCORE_API char* mg_camera_set_zoom(mg_core_t* core, int camera_id, float zoom) {
    (void)camera_id;
    return send_active_payload_request(core, "camera set zoom failed",
        [zoom](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_set_zoom(sys, comp, 0, zoom);
        });
}

MGCORE_API char* mg_camera_set_mode(mg_core_t* core, int camera_id, int mode) {
    (void)camera_id;
    return send_active_payload_request(core, "camera set mode failed",
        [mode](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_set_mode(sys, comp, static_cast<uint8_t>(mode));
        });
}

MGCORE_API char* mg_camera_stop_capture(mg_core_t* core, int camera_id) {
    (void)camera_id;
    return send_active_payload_request(core, "camera stop capture failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_stop_capture(sys, comp);
        });
}

MGCORE_API char* mg_camera_set_focus(mg_core_t* core, int camera_id, int focus_type, float focus_value) {
    (void)camera_id;
    return send_active_payload_request(core, "camera set focus failed",
        [focus_type, focus_value](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_set_focus(sys, comp, static_cast<uint8_t>(focus_type), focus_value);
        });
}

MGCORE_API char* mg_camera_request_info(mg_core_t* core, int camera_id) {
    (void)camera_id;
    return send_active_payload_request(core, "camera request info failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_request_info(sys, comp);
        });
}

MGCORE_API char* mg_camera_get_status(mg_core_t* core, int camera_id) {
    CHECK_CORE(core);
    (void)camera_id;
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("available", false);
    jb.field("recording", false);
    jb.field("photo_count", 0);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_command_gimbal_control(mg_core_t* core, float pitch_deg, float yaw_deg) {
    return send_active_payload_request(core, "gimbal control failed",
        [pitch_deg, yaw_deg](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.cmd_gimbal_control(sys, comp, pitch_deg, yaw_deg);
        });
}

MGCORE_API char* mg_command_gimbal_recenter(mg_core_t* core) {
    return send_active_payload_request(core, "gimbal recenter failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.cmd_gimbal_recenter(sys, comp);
        });
}

MGCORE_API char* mg_gimbal_request_info(mg_core_t* core) {
    return send_active_payload_request(core, "gimbal request info failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.gimbal_request_info(sys, comp);
        });
}

MGCORE_API char* mg_command_gimbal_configure(mg_core_t* core, int gimbal_device_id,
                                             int primary_sys, int primary_comp,
                                             int secondary_sys, int secondary_comp) {
    return send_active_payload_request(core, "gimbal configure failed",
        [gimbal_device_id, primary_sys, primary_comp, secondary_sys, secondary_comp](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.cmd_gimbal_configure(sys, comp,
                                            static_cast<uint8_t>(gimbal_device_id),
                                            static_cast<uint8_t>(primary_sys),
                                            static_cast<uint8_t>(primary_comp),
                                            static_cast<uint8_t>(secondary_sys),
                                            static_cast<uint8_t>(secondary_comp));
        });
}

MGCORE_API char* mg_video_start(mg_core_t* core, const char* uri) {
    (void)uri;
    return send_active_payload_request(core, "video start failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_start_video(sys, comp);
        });
}

MGCORE_API char* mg_video_stop(mg_core_t* core) {
    return send_active_payload_request(core, "video stop failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.camera_stop_video(sys, comp);
        });
}

MGCORE_API char* mg_video_status(mg_core_t* core) {
    CHECK_CORE(core);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("status", "stopped");
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_video_get_streams(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");

    auto streams = mav->latest_video_streams();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("count", static_cast<int>(streams.size()));

    mg::bridge::JsonBuilder arr;
    arr.begin_array();
    for (const auto& s : streams) {
        mg::bridge::JsonBuilder obj;
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
