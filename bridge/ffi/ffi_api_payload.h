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
MGCORE_API char* mg_gimbal_get_status(mg_core_t* core, int gimbal_id);
MGCORE_API char* mg_gimbal_get_attitude(mg_core_t* core, int gimbal_id);

MGCORE_API char* mg_video_start(mg_core_t* core, const char* uri);
MGCORE_API char* mg_video_stop(mg_core_t* core);
MGCORE_API char* mg_video_status(mg_core_t* core);
MGCORE_API char* mg_video_get_streams(mg_core_t* core);

// 本地视频解码/渲染（ADR-0008）。没有编译进解码后端时诚实降级：
// renderer_status 返回 available:false / backend:"none"，mg_video_start 返回
// MG_ERROR_NOT_SUPPORTED，mg_video_poll_frame 返回 0。
MGCORE_API char* mg_video_renderer_status(mg_core_t* core);

// 拉取最新一帧 RGBA8888 到 dst（stride = width*4）。返回写入字节数；
// 0 表示当前没有新帧或没有可用后端；-1 表示参数错误（dst 容量不足）。
// out_* 可为 NULL。dst 由调用方分配，至少 width*height*4 字节。
MGCORE_API int mg_video_poll_frame(mg_core_t* core, unsigned char* dst, int dst_capacity,
                                   int* out_width, int* out_height, long long* out_pts_us,
                                   unsigned long long* out_sequence);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_PAYLOAD_H */
