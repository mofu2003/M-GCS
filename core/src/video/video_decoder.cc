/**
 * video_decoder.cc - 本地视频源解码器实现（见 ADR-0008）
 *
 * 帧缓冲与生命周期控制与后端无关；FFmpeg 适配只在 MGCORE_HAS_FFMPEG 时编译。
 * 没有后端时所有入口诚实返回「不可用 / 未运行」，不做任何假进度。
 */
#include "core/video/video_decoder.h"

#include "core/log/logging.h"

#include <chrono>
#include <cstring>
#include <utility>

#ifdef MGCORE_HAS_FFMPEG
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#endif

namespace mg::core::video {

// ================================================================
// VideoFrameBuffer（后端无关）
// ================================================================

void VideoFrameBuffer::publish(const uint8_t* rgba, int width, int height, int64_t pts_us) {
    if (!rgba || width <= 0 || height <= 0) return;
    const size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    mg::core::platform::LockGuard lk(mtx_);
    if (rgba_.size() != bytes) rgba_.resize(bytes);
    std::memcpy(rgba_.data(), rgba, bytes);
    width_ = width;
    height_ = height;
    pts_us_ = pts_us;
    ++sequence_;
}

size_t VideoFrameBuffer::copy_latest(uint8_t* dst, size_t capacity, Snapshot& out) const {
    out = Snapshot{};
    if (!dst) return 0;
    mg::core::platform::LockGuard lk(mtx_);
    if (width_ <= 0 || height_ <= 0 || rgba_.empty()) return 0;
    if (capacity < rgba_.size()) return 0; // 容量不足：整体失败，不写半帧
    std::memcpy(dst, rgba_.data(), rgba_.size());
    out.width = width_;
    out.height = height_;
    out.pts_us = pts_us_;
    out.sequence = sequence_;
    return rgba_.size();
}

VideoFrameBuffer::Snapshot VideoFrameBuffer::latest() const {
    mg::core::platform::LockGuard lk(mtx_);
    Snapshot out;
    out.width = width_;
    out.height = height_;
    out.pts_us = pts_us_;
    out.sequence = sequence_;
    return out;
}

bool VideoFrameBuffer::has_frame() const {
    mg::core::platform::LockGuard lk(mtx_);
    return width_ > 0 && height_ > 0 && !rgba_.empty();
}

void VideoFrameBuffer::clear() {
    mg::core::platform::LockGuard lk(mtx_);
    rgba_.clear();
    rgba_.shrink_to_fit();
    width_ = 0;
    height_ = 0;
    pts_us_ = 0;
    sequence_ = 0;
}

// ================================================================
// 无后端：诚实降级
// ================================================================
#ifndef MGCORE_HAS_FFMPEG

struct VideoDecoder::Impl {};

VideoDecoder::VideoDecoder() = default;
VideoDecoder::~VideoDecoder() { stop(); }

bool VideoDecoder::backend_available() { return false; }
const char* VideoDecoder::backend_name() { return "none"; }

bool VideoDecoder::start(const std::string&) { return false; }
void VideoDecoder::stop() { frames_.clear(); }
bool VideoDecoder::running() const { return false; }
std::string VideoDecoder::uri() const {
    mg::core::platform::LockGuard lk(state_mtx_);
    return uri_;
}

void VideoDecoder::decode_loop() {}

#else // MGCORE_HAS_FFMPEG

// ================================================================
// FFmpeg 适配
// ================================================================

struct VideoDecoder::Impl {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwsContext* sws_ctx = nullptr;
    int video_stream_idx = -1;
    std::vector<uint8_t> rgba;
    AVFrame* rgba_frame = nullptr; // 借用 rgba.data() 的包装帧

    void reset() {
        if (rgba_frame) { av_frame_free(&rgba_frame); rgba_frame = nullptr; }
        if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
        if (codec_ctx) { avcodec_free_context(&codec_ctx); codec_ctx = nullptr; }
        if (fmt_ctx) { avformat_close_input(&fmt_ctx); fmt_ctx = nullptr; }
        video_stream_idx = -1;
        rgba.clear();
    }
};

namespace {

/// 打开输入流并准备解码器；成功返回 true。
bool open_input(VideoDecoder::Impl& impl, const std::string& uri) {
    if (avformat_open_input(&impl.fmt_ctx, uri.c_str(), nullptr, nullptr) < 0) {
        MGC_LOG_ERROR("[video] cannot open %s", uri.c_str());
        return false;
    }
    if (avformat_find_stream_info(impl.fmt_ctx, nullptr) < 0) {
        MGC_LOG_ERROR("[video] no stream info for %s", uri.c_str());
        return false;
    }
    for (unsigned i = 0; i < impl.fmt_ctx->nb_streams; ++i) {
        if (impl.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            impl.video_stream_idx = static_cast<int>(i);
            break;
        }
    }
    if (impl.video_stream_idx < 0) {
        MGC_LOG_ERROR("[video] no video stream in %s", uri.c_str());
        return false;
    }
    const AVCodecParameters* par = impl.fmt_ctx->streams[impl.video_stream_idx]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    if (!codec) {
        MGC_LOG_ERROR("[video] unsupported codec id %d", static_cast<int>(par->codec_id));
        return false;
    }
    impl.codec_ctx = avcodec_alloc_context3(codec);
    if (!impl.codec_ctx) return false;
    if (avcodec_parameters_to_context(impl.codec_ctx, par) < 0) return false;
    if (avcodec_open2(impl.codec_ctx, codec, nullptr) < 0) {
        MGC_LOG_ERROR("[video] cannot open codec for %s", uri.c_str());
        return false;
    }
    impl.sws_ctx = sws_getContext(impl.codec_ctx->width, impl.codec_ctx->height,
                                  impl.codec_ctx->pix_fmt,
                                  impl.codec_ctx->width, impl.codec_ctx->height,
                                  AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!impl.sws_ctx) {
        MGC_LOG_ERROR("[video] cannot create scaler for %s", uri.c_str());
        return false;
    }
    impl.rgba_frame = av_frame_alloc();
    if (!impl.rgba_frame) return false;
    const size_t bytes = static_cast<size_t>(impl.codec_ctx->width) *
                         static_cast<size_t>(impl.codec_ctx->height) * 4u;
    impl.rgba.assign(bytes, 0);
    av_image_fill_arrays(impl.rgba_frame->data, impl.rgba_frame->linesize, impl.rgba.data(),
                         AV_PIX_FMT_RGBA, impl.codec_ctx->width, impl.codec_ctx->height, 1);
    return true;
}

} // namespace

VideoDecoder::VideoDecoder() : impl_(std::make_unique<Impl>()) {}

VideoDecoder::~VideoDecoder() { stop(); }

bool VideoDecoder::backend_available() { return true; }
const char* VideoDecoder::backend_name() { return "ffmpeg"; }

bool VideoDecoder::start(const std::string& uri) {
    if (uri.empty()) return false;
    stop();
    if (!open_input(*impl_, uri)) {
        impl_->reset();
        return false;
    }
    {
        mg::core::platform::LockGuard lk(state_mtx_);
        uri_ = uri;
    }
    running_ = true;
    thread_ = std::thread(&VideoDecoder::decode_loop, this);
    return true;
}

void VideoDecoder::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    if (impl_) impl_->reset();
    frames_.clear();
    mg::core::platform::LockGuard lk(state_mtx_);
    uri_.clear();
}

bool VideoDecoder::running() const { return running_; }

std::string VideoDecoder::uri() const {
    mg::core::platform::LockGuard lk(state_mtx_);
    return uri_;
}

void VideoDecoder::decode_loop() {
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame) {
        av_packet_free(&pkt);
        av_frame_free(&frame);
        running_ = false;
        return;
    }
    const int width = impl_->codec_ctx->width;
    const int height = impl_->codec_ctx->height;

    while (running_) {
        if (av_read_frame(impl_->fmt_ctx, pkt) < 0) {
            // 流结束或暂时读不到：短暂退避后重试，避免忙等。
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        if (pkt->stream_index != impl_->video_stream_idx) {
            av_packet_unref(pkt);
            continue;
        }
        if (avcodec_send_packet(impl_->codec_ctx, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (running_ && avcodec_receive_frame(impl_->codec_ctx, frame) == 0) {
            sws_scale(impl_->sws_ctx, frame->data, frame->linesize, 0, height,
                      impl_->rgba_frame->data, impl_->rgba_frame->linesize);
            const AVRational tb = impl_->fmt_ctx->streams[impl_->video_stream_idx]->time_base;
            const int64_t pts_us = static_cast<int64_t>(
                frame->best_effort_timestamp * av_q2d(tb) * 1e6);
            frames_.publish(impl_->rgba.data(), width, height, pts_us);
            av_frame_unref(frame);
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
}

#endif // MGCORE_HAS_FFMPEG

} // namespace mg::core::video
