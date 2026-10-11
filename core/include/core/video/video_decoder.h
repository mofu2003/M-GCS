/**
 * video_decoder.h - 本地视频源解码器与「最新帧」缓冲
 *
 * 后端无关的帧交付契约（见 ADR-0008）：
 *   - 解码线程把 RGBA8888 帧写进 VideoFrameBuffer（单槽 + 序列号）；
 *   - 读取方（FFI 轮询）拷贝取走最新帧，不持有解码器内部指针。
 *
 * 没有被编译进来的后端时（MGCORE_HAS_FFMPEG 未定义）：backend_available()
 * 为 false，start() 返回 false，running() 为 false——诚实降级，不谎报正在播放。
 *
 * 帧交付路径与后端无关，因此帧缓冲可脱离 FFmpeg 用合成帧单测。
 */
#ifndef MGCORE_VIDEO_DECODER_H
#define MGCORE_VIDEO_DECODER_H

#include "core/platform/platform_mutex.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace mg::core::video {

/// 单槽「最新帧」缓冲。写入覆盖上一帧；读取拷贝取走，视频只关心最新画面。
class VideoFrameBuffer {
public:
    struct Snapshot {
        int width = 0;
        int height = 0;
        int64_t pts_us = 0;
        uint64_t sequence = 0;
    };

    /// 写入一帧 RGBA8888。width/height 非正或数据为空时忽略。
    void publish(const uint8_t* rgba, int width, int height, int64_t pts_us);

    /// 拷贝最新帧到 dst（RGBA8888，stride = width*4）。
    /// 容量不足或尚无帧时返回 0，且不写出部分数据。
    size_t copy_latest(uint8_t* dst, size_t capacity, Snapshot& out) const;

    /// 只读元数据（不拷贝像素）。无帧时各字段为 0。
    Snapshot latest() const;

    bool has_frame() const;
    void clear();

private:
    mutable mg::core::platform::Mutex mtx_;
    std::vector<uint8_t> rgba_;
    int width_ = 0;
    int height_ = 0;
    int64_t pts_us_ = 0;
    uint64_t sequence_ = 0;
};

/// 本地视频源解码器。同一时刻最多一个源；再次 start 会替换上一个。
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();
    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    /// 是否编译进了可用的解码后端。
    static bool backend_available();
    /// 后端名：无后端为 "none"，否则为 "ffmpeg"。
    static const char* backend_name();

    bool start(const std::string& uri);
    void stop();
    bool running() const;
    std::string uri() const;

    VideoFrameBuffer& frames() { return frames_; }
    const VideoFrameBuffer& frames() const { return frames_; }

private:
    void decode_loop();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    VideoFrameBuffer frames_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    mutable mg::core::platform::Mutex state_mtx_;
    std::string uri_;
};

} // namespace mg::core::video

#endif /* MGCORE_VIDEO_DECODER_H */
