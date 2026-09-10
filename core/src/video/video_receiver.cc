/**
 * video_receiver.cc - 视频流元数据管理
 *
 * 层 1：解析 VIDEO_STREAM_INFORMATION 消息，管理流信息。
 * 层 2（待做）：FFmpeg 解码视频帧。
 */
#include "core/video/video_receiver.h"
#include "core/log/logging.h"
#include "ardupilotmega/mavlink.h"
#include <cstring>

namespace mg::core::video {

VideoReceiver::VideoReceiver() = default;
VideoReceiver::~VideoReceiver() { stop(); }

bool VideoReceiver::start(const std::string& uri, VideoSource source) {
    mg::core::platform::LockGuard lk(mtx_);
    if (status_ == ReceiverStatus::Running) return false;

    MGC_LOG_INFO("[Video] Starting receiver: %s (type=%d)", uri.c_str(), (int)source);
    status_ = ReceiverStatus::Starting;

    // 层 2：FFmpeg 解码集成点
    // avformat_open_input(&fmt_ctx, uri.c_str(), nullptr, nullptr);
    // ...

    status_ = ReceiverStatus::Running;
    return true;
}

void VideoReceiver::stop() {
    mg::core::platform::LockGuard lk(mtx_);
    if (status_ == ReceiverStatus::Stopped) return;

    MGC_LOG_INFO("[Video] Stopping receiver");
    status_ = ReceiverStatus::Stopped;
}

ReceiverStatus VideoReceiver::status() const {
    mg::core::platform::LockGuard lk(mtx_);
    return status_;
}

void VideoReceiver::set_frame_callback(VideoFrameCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);
    frame_cb_ = std::move(cb);
}

// 解析 VIDEO_STREAM_INFORMATION (msg 269)
// 飞控/伴飞电脑发来此消息，告知可用的视频流及其 URI
void VideoReceiver::process_stream_info(uint8_t system_id, uint8_t component_id,
                                          const void* msg) {
    if (!msg) return;
    const auto* info = static_cast<const mavlink_video_stream_information_t*>(msg);

    mg::core::platform::LockGuard lk(mtx_);

    // 按 stream_id 查找或新建
    VideoStreamInfo* stream = nullptr;
    for (auto& s : streams_) {
        if (s.stream_id == info->stream_id &&
            s.system_id == system_id &&
            s.component_id == component_id) {
            stream = &s;
            break;
        }
    }
    if (!stream) {
        streams_.emplace_back();
        stream = &streams_.back();
    }

    // 填充流信息
    stream->system_id = system_id;
    stream->component_id = component_id;
    stream->stream_id = info->stream_id;
    stream->width = info->resolution_h;
    stream->height = info->resolution_v;
    stream->fps = info->framerate;
    stream->port = 0;  // 从 URI 解析

    // 解析 URI
    if (info->uri[0]) {
        stream->uri = std::string(info->uri, strnlen(info->uri, 160));
    }

    // 映射流类型
    switch (info->type) {
        case VIDEO_STREAM_TYPE_RTSP:     stream->source_type = VideoSource::RTSP; break;
        case VIDEO_STREAM_TYPE_RTPUDP:   stream->source_type = VideoSource::UDP; break;
        case VIDEO_STREAM_TYPE_TCP_MPEG: stream->source_type = VideoSource::TCP; break;
        default:                         stream->source_type = VideoSource::RTSP; break;
    }

    // flags bit 0: 流是否正在运行
    stream->available = (info->flags & 0x01) != 0;

    MGC_LOG_INFO("[Video] Stream %d: %s %dx%d@%.0fHz uri=%s",
                 info->stream_id, info->name,
                 info->resolution_h, info->resolution_v,
                 info->framerate, stream->uri.c_str());
}

std::vector<VideoStreamInfo> VideoReceiver::get_streams() const {
    mg::core::platform::LockGuard lk(mtx_);
    return streams_;
}

} // namespace mg::core::video
