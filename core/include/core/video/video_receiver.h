/**
 * video_receiver.h - Video stream receiving
 * 视频接收 — RTSP/UDP/GStreamer 视频流接收与管理
 *
 * Receives video streams from companion computers or IP cameras.
 * Supports RTSP, UDP, and GStreamer pipelines.
 *
 * Design: inspired by QGC VideoManager.
 */
#pragma once

#include "core/core_types.h"
#include "core/platform/platform_mutex.h"
#include "common/mavlink.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace mg::core::video {

/// Video source type
enum class VideoSource : uint8_t {
    RTSP,
    UDP,
    TCP,
    GStreamer,
    disabled,
};

/// Video stream information
struct VideoStreamInfo {
    uint8_t  system_id = 0;
    uint8_t  component_id = 0;
    uint32_t stream_id = 0;
    VideoSource source_type = VideoSource::RTSP;
    std::string uri;              // "rtsp://..." or "udp://..."
    uint16_t port = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    float    fps = 0;
    bool     available = false;
};

/// Video receiver status
enum class ReceiverStatus : uint8_t {
    Stopped,
    Starting,
    Running,
    Error,
};

/// Callback for video frame data
using VideoFrameCallback = std::function<void(const uint8_t* data, size_t size,
                                               uint16_t width, uint16_t height)>;

class VideoReceiver {
public:
    VideoReceiver();
    ~VideoReceiver();

    /// Start receiving from a video source.
    bool start(const std::string& uri, VideoSource source = VideoSource::RTSP);

    /// Stop receiving.
    void stop();

    /// Get current status.
    ReceiverStatus status() const;

    /// Set frame callback (for rendering).
    void set_frame_callback(VideoFrameCallback cb);

    /// Process MAVLink VIDEO_STREAM_INFORMATION message.
    void process_stream_info(uint8_t system_id, uint8_t component_id,
                              const void* msg);

    /// Get available video streams.
    std::vector<VideoStreamInfo> get_streams() const;

private:
    mutable mg::core::platform::Mutex mtx_;
    ReceiverStatus status_ = ReceiverStatus::Stopped;
    VideoFrameCallback frame_cb_;
    std::vector<VideoStreamInfo> streams_;
};

} // namespace mg::core::video
