/**
 * video_decoder.cc - 本地视频源解码器实现（见 ADR-0008）
 *
 * 帧缓冲与生命周期控制与后端无关；FFmpeg 适配只在 MGCORE_HAS_FFMPEG 时编译。
 * 没有后端时所有入口诚实返回「不可用 / 未运行」，不做任何假进度。
 */
#include "core/video/video_decoder.h"

#include "core/log/logging.h"

#include <cctype>
#include <chrono>
#include <cstdio>
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
    out.width = width_;
    out.height = height_;
    out.pts_us = pts_us_;
    out.sequence = sequence_;
    if (capacity < rgba_.size()) return 0; // 容量不足：整体失败，不写半帧
    std::memcpy(dst, rgba_.data(), rgba_.size());
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

bool VideoDecoder::start_recording(const std::string&, std::string& error) {
    error = "local video recording backend unavailable in this build";
    return false;
}
void VideoDecoder::stop_recording() {}
RecordingStatus VideoDecoder::recording_status() const { return RecordingStatus{}; }
bool VideoDecoder::capture_frame(const std::string&, std::string& error) {
    error = "local video capture backend unavailable in this build";
    return false;
}
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

    // ---- 录制（码流复制，不重新编码）----
    // 只保护状态与写帧调用；打开/关闭文件等阻塞 I/O 在锁外做（见 A7）。
    mutable mg::core::platform::Mutex rec_mtx;
    AVFormatContext* rec_ctx = nullptr;
    AVStream* rec_stream = nullptr;
    bool rec_active = false;
    std::string rec_path;
    std::string rec_error;
    int64_t rec_bytes = 0;
    int64_t rec_first_pts_us = 0;
    int64_t rec_last_pts_us = 0;
    bool rec_has_pts = false;
    int64_t rec_synth_next = 0; // 源没有时间戳时的合成时间轴（输出时基）

    /// 写一帧码流。调用方必须持 rec_mtx；返回 false 表示写失败，需要收尾。
    bool write_recording_packet(const AVPacket* pkt, const AVRational in_tb);

    void reset() {
        if (rgba_frame) { av_frame_free(&rgba_frame); rgba_frame = nullptr; }
        if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
        if (codec_ctx) { avcodec_free_context(&codec_ctx); codec_ctx = nullptr; }
        if (fmt_ctx) { avformat_close_input(&fmt_ctx); fmt_ctx = nullptr; }
        video_stream_idx = -1;
        rgba.clear();
        rec_ctx = nullptr; // 由 stop_recording() 关闭，这里只断开引用
        rec_stream = nullptr;
        rec_active = false;
    }
};

/// 打开输入流并准备解码器；成功返回 true。
bool VideoDecoder::open_input_impl(Impl& impl, const std::string& uri) {
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

VideoDecoder::VideoDecoder() : impl_(std::make_unique<Impl>()) {}

VideoDecoder::~VideoDecoder() { stop(); }

bool VideoDecoder::backend_available() { return true; }
const char* VideoDecoder::backend_name() { return "ffmpeg"; }

bool VideoDecoder::start(const std::string& uri) {
    if (uri.empty()) return false;
    stop();
    if (!open_input_impl(*impl_, uri)) {
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
    stop_recording(); // 仍在录制时先写完文件尾，再释放输入上下文
    if (impl_) impl_->reset();
    frames_.clear();
    mg::core::platform::LockGuard lk(state_mtx_);
    uri_.clear();
}

bool VideoDecoder::running() const { return running_; }

// ================================================================
// 录制：把源码流复制进文件（不重新编码）
// ================================================================

namespace {

/// 建好录制输出上下文并写文件头。调用时**不持锁**——这里全是阻塞 I/O。
/// 失败时返回 nullptr 并填 error，不留下半开的上下文。
AVFormatContext* open_recording_output(const std::string& path, const AVStream* in_stream,
                                       AVStream** out_stream, std::string& error) {
    AVFormatContext* out = nullptr;
    if (avformat_alloc_output_context2(&out, nullptr, nullptr, path.c_str()) < 0 || !out) {
        error = "unsupported recording container: " + path;
        return nullptr;
    }
    AVStream* stream = avformat_new_stream(out, nullptr);
    if (!stream) {
        error = "cannot create recording stream";
        avformat_free_context(out);
        return nullptr;
    }
    if (avcodec_parameters_copy(stream->codecpar, in_stream->codecpar) < 0) {
        error = "cannot copy codec parameters";
        avformat_free_context(out);
        return nullptr;
    }
    stream->codecpar->codec_tag = 0;
    stream->time_base = in_stream->time_base;
    if (!(out->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out->pb, path.c_str(), AVIO_FLAG_WRITE) < 0) {
            error = "cannot open recording file: " + path;
            avformat_free_context(out);
            return nullptr;
        }
    }
    if (avformat_write_header(out, nullptr) < 0) {
        error = "cannot write recording header (codec may not fit the container): " + path;
        if (out->pb) avio_closep(&out->pb);
        avformat_free_context(out);
        return nullptr;
    }
    *out_stream = stream;
    return out;
}

/// 收尾并释放录制输出。同样**不持锁**。
void close_recording_output(AVFormatContext* out) {
    if (!out) return;
    (void)av_write_trailer(out);
    if (out->pb) avio_closep(&out->pb);
    avformat_free_context(out);
}

/// 按扩展名选择图片编码器：png / jpg|jpeg；其它返回 NONE。
AVCodecID image_codec_for_path(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    const std::string ext =
        dot == std::string::npos ? std::string() : path.substr(dot + 1);
    std::string lower;
    for (char c : ext) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (lower == "png") return AV_CODEC_ID_PNG;
    if (lower == "jpg" || lower == "jpeg") return AV_CODEC_ID_MJPEG;
    return AV_CODEC_ID_NONE;
}

/// 把 RGBA8888 帧编码成图片写入 path。调用方**不持锁**（全程阻塞 I/O）。
bool write_image_file(const std::string& path, const uint8_t* rgba, int width, int height,
                      std::string& error) {
    const AVCodecID id = image_codec_for_path(path);
    if (id == AV_CODEC_ID_NONE) {
        error = "unsupported image format (expected .png or .jpg): " + path;
        return false;
    }
    const AVCodec* codec = avcodec_find_encoder(id);
    if (!codec) {
        error = "image encoder unavailable in this build: " + path;
        return false;
    }
    // PNG 编码器原生支持 RGBA；MJPEG 编码器用 YUVJ420P（JPEG 全色域）。
    // 直接选这两种稳定支持的格式，避免依赖已废弃的 AVCodec::pix_fmts。
    const AVPixelFormat dst_fmt =
        (id == AV_CODEC_ID_PNG) ? AV_PIX_FMT_RGBA : AV_PIX_FMT_YUVJ420P;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    AVFrame* frame = av_frame_alloc();
    SwsContext* sws = nullptr;
    bool ok = false;
    do {
        if (!ctx || !frame) { error = "out of memory"; break; }
        ctx->width = width;
        ctx->height = height;
        ctx->pix_fmt = dst_fmt;
        ctx->time_base = AVRational{1, 25};
        if (id == AV_CODEC_ID_MJPEG) ctx->color_range = AVCOL_RANGE_JPEG;
        if (avcodec_open2(ctx, codec, nullptr) < 0) { error = "cannot open image encoder"; break; }
        frame->format = dst_fmt;
        frame->width = width;
        frame->height = height;
        if (av_frame_get_buffer(frame, 32) < 0) { error = "cannot allocate image frame"; break; }
        sws = sws_getContext(width, height, AV_PIX_FMT_RGBA, width, height, dst_fmt, SWS_BILINEAR,
                             nullptr, nullptr, nullptr);
        if (!sws) { error = "cannot create image scaler"; break; }
        const uint8_t* src[4] = {rgba, nullptr, nullptr, nullptr};
        const int src_stride[4] = {width * 4, 0, 0, 0};
        sws_scale(sws, src, src_stride, 0, height, frame->data, frame->linesize);

        if (avcodec_send_frame(ctx, frame) < 0) { error = "cannot encode image frame"; break; }
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) { error = "out of memory"; break; }
        const int rr = avcodec_receive_packet(ctx, pkt);
        if (rr < 0) {
            av_packet_free(&pkt);
            error = "image encoder produced no packet";
            break;
        }
        const int pkt_size = pkt->size;
        FILE* fp = std::fopen(path.c_str(), "wb");
        if (!fp) {
            av_packet_free(&pkt);
            error = "cannot open capture file: " + path;
            break;
        }
        const size_t written = std::fwrite(pkt->data, 1, static_cast<size_t>(pkt_size), fp);
        const bool flush_ok = std::fflush(fp) == 0;
        std::fclose(fp);
        av_packet_free(&pkt);
        if (written != static_cast<size_t>(pkt_size) || !flush_ok) {
            error = "failed writing capture file: " + path;
            break;
        }
        ok = true;
    } while (false);

    if (sws) sws_freeContext(sws);
    if (frame) av_frame_free(&frame);
    if (ctx) avcodec_free_context(&ctx);
    return ok;
}

/// 源没有时间戳时，按帧率推一条合成时间轴（输出时基单位）。
int64_t synthetic_step(const AVStream* in_stream, const AVStream* out_stream) {
    AVRational fr = in_stream->avg_frame_rate;
    if (fr.num <= 0 || fr.den <= 0) fr = AVRational{30, 1};
    AVRational tb = out_stream->time_base;
    if (tb.num <= 0 || tb.den <= 0) tb = AVRational{1, 1000000};
    const AVRational frame_tb{fr.den, fr.num};
    const int64_t step = av_rescale_q(1, frame_tb, tb);
    return step > 0 ? step : 1;
}

} // namespace

/// 写一帧码流。调用方必须持 rec_mtx；返回 false 表示写失败，需要收尾。
bool VideoDecoder::Impl::write_recording_packet(const AVPacket* pkt, const AVRational in_tb) {
    AVPacket* copy = av_packet_clone(pkt);
    if (!copy) return true; // 单帧分配失败不打断解码，也不当作录制失败
    AVRational out_tb = rec_stream->time_base;
    if (out_tb.num <= 0 || out_tb.den <= 0) out_tb = in_tb;
    const int64_t step = synthetic_step(fmt_ctx->streams[video_stream_idx], rec_stream);
    if (copy->pts == AV_NOPTS_VALUE && copy->dts == AV_NOPTS_VALUE) {
        copy->pts = copy->dts = rec_synth_next;
    } else {
        if (copy->dts == AV_NOPTS_VALUE) copy->dts = copy->pts;
        if (copy->pts == AV_NOPTS_VALUE) copy->pts = copy->dts;
    }
    av_packet_rescale_ts(copy, in_tb, out_tb);
    if (copy->dts > rec_synth_next) rec_synth_next = copy->dts + step;
    const int64_t pts_us = av_rescale_q(copy->pts, out_tb, AV_TIME_BASE_Q);
    copy->stream_index = rec_stream->index;
    copy->pos = -1;
    const int ret = av_interleaved_write_frame(rec_ctx, copy); // 成功后接管所有权
    if (ret < 0) {
        av_packet_free(&copy);
        rec_error = "failed to write recording packet";
        return false;
    }
    if (rec_ctx->pb) {
        const int64_t tell = avio_tell(rec_ctx->pb);
        if (tell > 0) rec_bytes = tell;
    }
    if (!rec_has_pts) {
        rec_first_pts_us = pts_us;
        rec_has_pts = true;
    }
    rec_last_pts_us = pts_us;
    return true;
}

bool VideoDecoder::start_recording(const std::string& path, std::string& error) {
    error.clear();
    if (path.empty()) {
        error = "recording path is required";
        return false;
    }
    if (!running_) {
        error = "no local video source is running";
        return false;
    }
    if (!impl_->fmt_ctx || impl_->video_stream_idx < 0) {
        error = "video stream is not ready";
        return false;
    }
    {
        mg::core::platform::LockGuard lk(impl_->rec_mtx);
        if (impl_->rec_active || impl_->rec_ctx) {
            error = "already recording";
            return false;
        }
    }
    AVStream* in_stream = impl_->fmt_ctx->streams[impl_->video_stream_idx];
    AVStream* out_stream = nullptr;
    // 打开文件与写头在锁外完成，不拖住解码线程（A7：不在持锁路径做阻塞 I/O）。
    AVFormatContext* out = open_recording_output(path, in_stream, &out_stream, error);
    if (!out) return false;
    {
        mg::core::platform::LockGuard lk(impl_->rec_mtx);
        if (impl_->rec_active || impl_->rec_ctx) {
            error = "already recording";
            close_recording_output(out);
            return false;
        }
        impl_->rec_ctx = out;
        impl_->rec_stream = out_stream;
        impl_->rec_active = true;
        impl_->rec_path = path;
        impl_->rec_error.clear();
        impl_->rec_bytes = 0;
        impl_->rec_has_pts = false;
        impl_->rec_synth_next = 0;
        impl_->rec_first_pts_us = 0;
        impl_->rec_last_pts_us = 0;
    }
    return true;
}

void VideoDecoder::stop_recording() {
    AVFormatContext* out = nullptr;
    {
        mg::core::platform::LockGuard lk(impl_->rec_mtx);
        if (!impl_->rec_ctx && !impl_->rec_active) return;
        out = impl_->rec_ctx;
        impl_->rec_ctx = nullptr;
        impl_->rec_stream = nullptr;
        impl_->rec_active = false;
    }
    // 写完文件尾、关闭文件同样在锁外（可能触发实际磁盘写入）。
    close_recording_output(out);
}

RecordingStatus VideoDecoder::recording_status() const {
    RecordingStatus status;
    mg::core::platform::LockGuard lk(impl_->rec_mtx);
    status.recording = impl_->rec_active;
    status.path = impl_->rec_path;
    status.error = impl_->rec_error;
    status.bytes = impl_->rec_bytes;
    if (impl_->rec_has_pts && impl_->rec_last_pts_us >= impl_->rec_first_pts_us)
        status.duration_us = impl_->rec_last_pts_us - impl_->rec_first_pts_us;
    return status;
}

bool VideoDecoder::capture_frame(const std::string& path, std::string& error) {
    error.clear();
    if (path.empty()) {
        error = "capture path is required";
        return false;
    }
    const VideoFrameBuffer::Snapshot snap = frames_.latest();
    if (snap.width <= 0 || snap.height <= 0) {
        error = "no frame available to capture";
        return false;
    }
    const size_t bytes = static_cast<size_t>(snap.width) * static_cast<size_t>(snap.height) * 4u;
    std::vector<uint8_t> rgba(bytes);
    VideoFrameBuffer::Snapshot copied;
    if (frames_.copy_latest(rgba.data(), rgba.size(), copied) == 0) {
        error = "no frame available to capture";
        return false;
    }
    // 编码与落盘在锁外进行（A7：不在持锁路径做阻塞 I/O）。
    return write_image_file(path, rgba.data(), snap.width, snap.height, error);
}

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
        // 录制：先复制原始码流再解码。写失败只影响录制，收尾时保留错误信息。
        {
            bool rec_failed = false;
            {
                mg::core::platform::LockGuard lk(impl_->rec_mtx);
                if (impl_->rec_active && impl_->rec_stream) {
                    const AVRational in_tb =
                        impl_->fmt_ctx->streams[impl_->video_stream_idx]->time_base;
                    if (!impl_->write_recording_packet(pkt, in_tb)) rec_failed = true;
                }
            }
            if (rec_failed) stop_recording();
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
