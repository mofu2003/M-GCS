#ifndef MGCORE_FFI_API_PAYLOAD_H
#define MGCORE_FFI_API_PAYLOAD_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_camera_take_photo(mg_core_t* core, int camera_id);
MGCORE_API char* mg_camera_start_video(mg_core_t* core, int camera_id);
MGCORE_API char* mg_camera_stop_video(mg_core_t* core, int camera_id);
MGCORE_API char* mg_camera_set_zoom(mg_core_t* core, int camera_id, float zoom_level);
MGCORE_API char* mg_camera_set_mode(mg_core_t* core, int camera_id, int mode);
MGCORE_API char* mg_camera_stop_capture(mg_core_t* core, int camera_id);
MGCORE_API char* mg_camera_set_focus(mg_core_t* core, int camera_id, int focus_type, float focus_value);
MGCORE_API char* mg_camera_request_info(mg_core_t* core, int camera_id);
MGCORE_API char* mg_camera_get_status(mg_core_t* core, int camera_id);

MGCORE_API char* mg_command_gimbal_control(mg_core_t* core, float pitch_deg, float yaw_deg);
MGCORE_API char* mg_command_gimbal_recenter(mg_core_t* core);
MGCORE_API char* mg_gimbal_request_info(mg_core_t* core);
MGCORE_API char* mg_command_gimbal_configure(mg_core_t* core, int gimbal_device_id,
                                              int primary_sys, int primary_comp,
                                              int secondary_sys, int secondary_comp);

MGCORE_API char* mg_video_start(mg_core_t* core, const char* uri);
MGCORE_API char* mg_video_stop(mg_core_t* core);
MGCORE_API char* mg_video_status(mg_core_t* core);
MGCORE_API char* mg_video_get_streams(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_PAYLOAD_H */
