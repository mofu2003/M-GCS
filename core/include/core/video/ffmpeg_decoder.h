/**
 * ffmpeg_decoder.h - FFmpeg 视频解码器
 *
 * 封装 FFmpeg 的解码管线：打开流 → 读包 → 解码 → YUV→RGB → 回调。
 * 支持 RTSP/UDP/TCP 视频流，H.264/H.265 编解码。
 *
 * 没有 FFmpeg 时（MGCORE_HAS_FFMPEG 未定义）编译为空实现。
 */
#ifndef MGCORE_FFMPEG_DECODER_H
#define MGCORE_FFMPEG_DECODER_H

#include <cstdint>
#include <string>
#include <functional>
#include <thread>
#include <atomic>

#ifdef MGCORE_HAS_FFMPEG
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#endif

namespace mg::core::video {

/// 解码后的帧数据（RGB 格式）
struct DecodedFrame {
    const uint8_t* data = nullptr;  // RGB24 像素数据
    int width = 0;
    int height = 0;
    int64_t pts_us = 0;            // 显示时间戳（微秒）
};

/// 帧回调类型
using FrameCallback = std::function<void(const DecodedFrame&)>;

/// FFmpeg 解码器
class FFmpegDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder();

    /// 打开视频流
    bool open(const std::string& uri);

    /// 关闭视频流
    void close();

    /// 是否已打开
    bool is_open() const;

    /// 设置帧回调（解码后自动调用）
    void set_frame_callback(FrameCallback cb);

    /// 开始解码线程
    bool start_decode();

    /// 停止解码线程
    void stop_decode();

    /// 录像到文件（开始）
    bool start_recording(const std::string& filepath);

    /// 录像到文件（停止）
    void stop_recording();

    /// 是否正在录像
    bool is_recording() const;

    /// 截图保存到文件
    bool save_screenshot(const std::string& filepath);

    /// 获取流信息
    int width() const;
    int height() const;
    float fps() const;

private:
#ifdef MGCORE_HAS_FFMPEG
    AVFormatContext* fmt_ctx_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    SwsContext* sws_ctx_ = nullptr;
    int video_stream_idx_ = -1;

    // 录像相关
    AVFormatContext* rec_fmt_ctx_ = nullptr;
    AVStream* rec_stream_ = nullptr;
    bool recording_ = false;
#endif

    FrameCallback frame_cb_;
    std::thread decode_thread_;
    std::atomic<bool> decoding_{false};
    std::atomic<bool> opened_{false};

    // 最近一帧（用于截图，只在启用 FFmpeg 时存在）
#ifdef MGCORE_HAS_FFMPEG
    uint8_t* last_frame_rgb_ = nullptr;
    int last_frame_w_ = 0;
    int last_frame_h_ = 0;
#endif

    void decode_loop();
};

} // namespace mg::core::video

#endif /* MGCORE_FFMPEG_DECODER_H */
