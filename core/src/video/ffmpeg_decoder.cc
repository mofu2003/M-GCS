/**
 * ffmpeg_decoder.cc - FFmpeg 视频解码器实现
 *
 * 没有 FFmpeg 时编译为空实现（所有方法返回 false/空值）。
 * 有 FFmpeg 时：打开流 → 读包 → 解码 → sws_scale YUV→RGB → 回调
 */
#include "core/video/ffmpeg_decoder.h"
#include "core/log/logging.h"
#include <cstring>

namespace mg::core::video {

// ================================================================
// 无 FFmpeg 时的空实现
// ================================================================
#ifndef MGCORE_HAS_FFMPEG

FFmpegDecoder::FFmpegDecoder() = default;
FFmpegDecoder::~FFmpegDecoder() { close(); }
bool FFmpegDecoder::open(const std::string&) { return false; }
void FFmpegDecoder::close() {}
bool FFmpegDecoder::is_open() const { return false; }
void FFmpegDecoder::set_frame_callback(FrameCallback cb) { frame_cb_ = std::move(cb); }
bool FFmpegDecoder::start_decode() { return false; }
void FFmpegDecoder::stop_decode() {}
bool FFmpegDecoder::start_recording(const std::string&) { return false; }
void FFmpegDecoder::stop_recording() {}
bool FFmpegDecoder::is_recording() const { return false; }
bool FFmpegDecoder::save_screenshot(const std::string&) { return false; }
int FFmpegDecoder::width() const { return 0; }
int FFmpegDecoder::height() const { return 0; }
float FFmpegDecoder::fps() const { return 0; }
void FFmpegDecoder::decode_loop() {}

#else

// ================================================================
// FFmpeg 实现
// ================================================================

FFmpegDecoder::FFmpegDecoder() = default;

FFmpegDecoder::~FFmpegDecoder() {
    close();
}

bool FFmpegDecoder::open(const std::string& uri) {
    if (opened_) return true;

    // 打开输入流
    if (avformat_open_input(&fmt_ctx_, uri.c_str(), nullptr, nullptr) < 0) {
        MGC_LOG_ERROR("[FFmpeg] Failed to open: %s", uri.c_str());
        return false;
    }

    // 查找流信息
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) {
        MGC_LOG_ERROR("[FFmpeg] Failed to find stream info");
        close();
        return false;
    }

    // 查找视频流
    for (unsigned i = 0; i < fmt_ctx_->nb_streams; i++) {
        if (fmt_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx_ = i;
            break;
        }
    }
    if (video_stream_idx_ < 0) {
        MGC_LOG_ERROR("[FFmpeg] No video stream found");
        close();
        return false;
    }

    // 打开解码器
    const AVCodec* codec = avcodec_find_decoder(
        fmt_ctx_->streams[video_stream_idx_]->codecpar->codec_id);
    if (!codec) {
        MGC_LOG_ERROR("[FFmpeg] Unsupported codec");
        close();
        return false;
    }

    codec_ctx_ = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx_, fmt_ctx_->streams[video_stream_idx_]->codecpar);
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
        MGC_LOG_ERROR("[FFmpeg] Failed to open codec");
        close();
        return false;
    }

    // 初始化 YUV→RGB 转换器
    sws_ctx_ = sws_getContext(
        codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
        codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws_ctx_) {
        MGC_LOG_ERROR("[FFmpeg] Failed to create sws context");
        close();
        return false;
    }

    // 分配最近帧缓冲
    last_frame_w_ = codec_ctx_->width;
    last_frame_h_ = codec_ctx_->height;
    last_frame_rgb_ = new uint8_t[last_frame_w_ * last_frame_h_ * 3];

    opened_ = true;
    MGC_LOG_INFO("[FFmpeg] Opened %s: %dx%d", uri.c_str(), codec_ctx_->width, codec_ctx_->height);
    return true;
}

void FFmpegDecoder::close() {
    stop_decode();
    stop_recording();

    if (last_frame_rgb_) { delete[] last_frame_rgb_; last_frame_rgb_ = nullptr; }
    if (sws_ctx_) { sws_freeContext(sws_ctx_); sws_ctx_ = nullptr; }
    if (codec_ctx_) { avcodec_free_context(&codec_ctx_); codec_ctx_ = nullptr; }
    if (fmt_ctx_) { avformat_close_input(&fmt_ctx_); fmt_ctx_ = nullptr; }
    video_stream_idx_ = -1;
    opened_ = false;
}

bool FFmpegDecoder::is_open() const { return opened_; }

void FFmpegDecoder::set_frame_callback(FrameCallback cb) { frame_cb_ = std::move(cb); }

bool FFmpegDecoder::start_decode() {
    if (!opened_ || decoding_) return false;
    decoding_ = true;
    decode_thread_ = std::thread(&FFmpegDecoder::decode_loop, this);
    return true;
}

void FFmpegDecoder::stop_decode() {
    decoding_ = false;
    if (decode_thread_.joinable()) decode_thread_.join();
}

void FFmpegDecoder::decode_loop() {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* rgb_frame = av_frame_alloc();

    // 分配 RGB 缓冲
    int buf_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx_->width, codec_ctx_->height, 1);
    uint8_t* rgb_buf = new uint8_t[buf_size];
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buf,
                         AV_PIX_FMT_RGB24, codec_ctx_->width, codec_ctx_->height, 1);

    while (decoding_) {
        if (av_read_frame(fmt_ctx_, pkt) < 0) {
            // 流结束或错误，尝试重连
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (pkt->stream_index != video_stream_idx_) {
            av_packet_unref(pkt);
            continue;
        }

        int ret = avcodec_send_packet(codec_ctx_, pkt);
        av_packet_unref(pkt);
        if (ret < 0) continue;

        while (avcodec_receive_frame(codec_ctx_, frame) == 0) {
            // YUV → RGB
            sws_scale(sws_ctx_,
                      frame->data, frame->linesize, 0, codec_ctx_->height,
                      rgb_frame->data, rgb_frame->linesize);

            // 保存最近帧（用于截图）
            memcpy(last_frame_rgb_, rgb_buf, buf_size);

            // 回调
            if (frame_cb_) {
                DecodedFrame df;
                df.data = rgb_buf;
                df.width = codec_ctx_->width;
                df.height = codec_ctx_->height;
                df.pts_us = frame->best_effort_timestamp *
                    av_q2d(fmt_ctx_->streams[video_stream_idx_]->time_base) * 1e6;
                frame_cb_(df);
            }

            // 写入录像文件
            if (recording_ && rec_fmt_ctx_) {
                av_write_frame(rec_fmt_ctx_, pkt);
            }
        }
    }

    delete[] rgb_buf;
    av_frame_free(&rgb_frame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

bool FFmpegDecoder::start_recording(const std::string& filepath) {
    if (!opened_ || recording_) return false;

    // 创建输出上下文
    if (avformat_alloc_output_context2(&rec_fmt_ctx_, nullptr, nullptr, filepath.c_str()) < 0) {
        return false;
    }

    // 复制视频流
    rec_stream_ = avformat_new_stream(rec_fmt_ctx_, nullptr);
    avcodec_parameters_copy(rec_stream_->codecpar, fmt_ctx_->streams[video_stream_idx_]->codecpar);

    // 打开输出文件
    if (!(rec_fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&rec_fmt_ctx_->pb, filepath.c_str(), AVIO_FLAG_WRITE) < 0) {
            avformat_free_context(rec_fmt_ctx_);
            rec_fmt_ctx_ = nullptr;
            return false;
        }
    }

    avformat_write_header(rec_fmt_ctx_, nullptr);
    recording_ = true;
    MGC_LOG_INFO("[FFmpeg] Recording to %s", filepath.c_str());
    return true;
}

void FFmpegDecoder::stop_recording() {
    if (!recording_) return;
    if (rec_fmt_ctx_) {
        av_write_trailer(rec_fmt_ctx_);
        if (!(rec_fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&rec_fmt_ctx_->pb);
        }
        avformat_free_context(rec_fmt_ctx_);
        rec_fmt_ctx_ = nullptr;
    }
    recording_ = false;
    MGC_LOG_INFO("[FFmpeg] Recording stopped");
}

bool FFmpegDecoder::is_recording() const { return recording_; }

bool FFmpegDecoder::save_screenshot(const std::string& filepath) {
    if (!last_frame_rgb_ || last_frame_w_ == 0) return false;

    // 保存为 PPM 格式（简单，无额外依赖）
    FILE* f = fopen(filepath.c_str(), "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", last_frame_w_, last_frame_h_);
    fwrite(last_frame_rgb_, 1, last_frame_w_ * last_frame_h_ * 3, f);
    fclose(f);
    MGC_LOG_INFO("[FFmpeg] Screenshot saved: %s", filepath.c_str());
    return true;
}

int FFmpegDecoder::width() const { return codec_ctx_ ? codec_ctx_->width : 0; }
int FFmpegDecoder::height() const { return codec_ctx_ ? codec_ctx_->height : 0; }
float FFmpegDecoder::fps() const {
    if (!codec_ctx_ || !fmt_ctx_ || video_stream_idx_ < 0) return 0;
    AVRational r = fmt_ctx_->streams[video_stream_idx_]->r_frame_rate;
    return (r.den > 0) ? static_cast<float>(r.num) / r.den : 0;
}

#endif // MGCORE_HAS_FFMPEG

} // namespace mg::core::video
