/**
 * test_video_pipeline.cc - 本地视频帧缓冲与 video FFI 契约（ADR-0008）
 *
 * 覆盖两个后端无关的部分：
 *   1. VideoFrameBuffer 的写入/拷贝/容量不足/清空语义（合成帧，无需 FFmpeg）。
 *   2. 本地视频 FFI 契约：renderer_status 的后端能力、start/stop 的诚实降级、
 *      poll_frame 返回 0、video_status.playing 为 false，以及录制/截图入口的参数
 *      与状态校验（S4）。断言按 VideoDecoder::backend_available() 分支，因此
 *      MGCORE_HAS_FFMPEG 开或关都能通过。
 *
 * 真机/FFmpeg 解码路径本身（连接真实码流、录制落盘）不在单元测试覆盖，见 ADR-0008 的
 * 平台矩阵与 docs/features/video-camera-gimbal.md 的手动验证步骤。
 */
#include "ffi_api.h"
#include "core/video/video_decoder.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using mg::core::video::VideoDecoder;
using mg::core::video::VideoFrameBuffer;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static bool json_has(const char* json, const char* needle) {
    return json && std::strstr(json, needle) != nullptr;
}

int main() {
    fprintf(stderr, "=== Video Pipeline Tests ===\n");

    // ---- VideoFrameBuffer ----
    {
        fprintf(stderr, "\n[Group] VideoFrameBuffer\n");
        VideoFrameBuffer buf;
        CHECK(!buf.has_frame(), "empty buffer has no frame");

        VideoFrameBuffer::Snapshot snap = buf.latest();
        CHECK(snap.width == 0 && snap.height == 0 && snap.sequence == 0,
              "empty buffer latest() is zeroed");

        // 2x2 RGBA
        const uint8_t f1[16] = {
            1,2,3,4,   5,6,7,8,
            9,10,11,12, 13,14,15,16,
        };
        buf.publish(f1, 2, 2, 1000);
        CHECK(buf.has_frame(), "publish makes frame available");

        snap = buf.latest();
        CHECK(snap.width == 2 && snap.height == 2 && snap.pts_us == 1000 && snap.sequence == 1,
              "latest() reports geometry, pts and sequence");

        uint8_t out[16] = {0};
        VideoFrameBuffer::Snapshot got;
        size_t n = buf.copy_latest(out, sizeof(out), got);
        CHECK(n == 16, "copy_latest copies the whole frame");
        CHECK(std::memcmp(out, f1, 16) == 0, "copied pixels match published pixels");
        CHECK(got.width == 2 && got.height == 2, "copy_latest reports geometry");

        // 容量不足：整体失败，不写半帧
        uint8_t small[8];
        std::memset(small, 0xAB, sizeof(small));
        size_t n2 = buf.copy_latest(small, sizeof(small), got);
        CHECK(n2 == 0, "copy_latest fails when capacity is too small");
        bool untouched = true;
        for (uint8_t b : small) if (b != 0xAB) untouched = false;
        CHECK(untouched, "copy_latest does not write a partial frame");

        // 覆盖写：sequence 递增，latest() 反映新帧
        const uint8_t f2[16] = {
            16,15,14,13, 12,11,10,9,
            8,7,6,5,     4,3,2,1,
        };
        buf.publish(f2, 2, 2, 2000);
        n = buf.copy_latest(out, sizeof(out), got);
        CHECK(n == 16 && got.sequence == 2 && got.pts_us == 2000, "publish overwrites latest frame");
        CHECK(std::memcmp(out, f2, 16) == 0, "overwritten pixels are the new frame");

        // 非法输入被忽略，不破坏已有帧
        buf.publish(nullptr, 2, 2, 3000);
        buf.publish(f2, 0, 2, 3000);
        CHECK(buf.latest().sequence == 2, "invalid publish is ignored");

        // 尺寸变化时重分配
        const uint8_t f3[4] = {7,7,7,7};
        buf.publish(f3, 1, 1, 4000);
        uint8_t out1[4] = {0};
        n = buf.copy_latest(out1, sizeof(out1), got);
        CHECK(n == 4 && got.width == 1 && got.height == 1, "publish resizes to new geometry");

        buf.clear();
        CHECK(!buf.has_frame(), "clear drops the frame");
        CHECK(buf.latest().sequence == 0, "clear resets sequence");
    }

    // ---- VideoDecoder（按编译进来的后端分支，ADR-0008）----
    {
        fprintf(stderr, "\n[Group] VideoDecoder backend gating\n");
        const bool has_backend = VideoDecoder::backend_available();
        const std::string backend = VideoDecoder::backend_name();
        CHECK(has_backend == (backend != "none"),
              "backend_available() agrees with backend_name()");
        if (has_backend)
            CHECK(backend == "ffmpeg", "backend name is \"ffmpeg\" when a decoder is compiled in");
        else
            CHECK(backend == "none", "backend name is \"none\" without a decoder");

        VideoDecoder dec;
        // 不可达的 RTSP 源：无后端时 start 直接拒绝；有后端时会尝试连接并同样失败。
        CHECK(!dec.start("rtsp://example.invalid/stream"),
              "start() fails on an unreachable source");
        CHECK(!dec.running(), "decoder is not running after a failed start");
        CHECK(!dec.frames().has_frame(), "no frames are produced from an unreachable source");
        dec.stop(); // must be safe
    }

    // ---- FFI 契约 ----
    {
        fprintf(stderr, "\n[Group] video FFI contract\n");
        mg_core_t* core = mg_create();
        CHECK(core != nullptr, "mg_create returns a core");

        const bool has_backend = VideoDecoder::backend_available();

        char* st = mg_video_renderer_status(core);
        if (has_backend) {
            CHECK(json_has(st, "\"available\":true"), "renderer_status reports available:true");
            CHECK(json_has(st, "\"backend\":\"ffmpeg\""), "renderer_status reports backend:ffmpeg");
        } else {
            CHECK(json_has(st, "\"available\":false"), "renderer_status reports available:false");
            CHECK(json_has(st, "\"backend\":\"none\""), "renderer_status reports backend:none");
        }
        CHECK(json_has(st, "\"running\":false"), "renderer_status reports not running");
        CHECK(json_has(st, "\"success\":true"), "renderer_status is a success envelope");
        mg_free_string(st);

        char* started = mg_video_start(core, "rtsp://example.invalid/stream");
        CHECK(json_has(started, "\"success\":false"), "start fails on an unreachable source");
        if (has_backend)
            CHECK(json_has(started, "SEND_FAILED"), "start reports SEND_FAILED when the source cannot open");
        else
            CHECK(json_has(started, "NOT_SUPPORTED"), "start reports NOT_SUPPORTED without a backend");
        mg_free_string(started);

        char* empty = mg_video_start(core, "");
        CHECK(json_has(empty, "INVALID_PARAM"), "empty uri is rejected as INVALID_PARAM");
        mg_free_string(empty);

        char* stopped = mg_video_stop(core);
        CHECK(json_has(stopped, "\"success\":true"), "stop is an idempotent success");
        mg_free_string(stopped);

        char* status = mg_video_status(core);
        CHECK(json_has(status, "\"playing\":false"), "video_status does not claim to be playing");
        mg_free_string(status);

        std::vector<unsigned char> frame(4 * 4 * 4, 0);
        int w = -1, h = -1;
        long long pts = -1;
        unsigned long long seq = 99;
        int copied = mg_video_poll_frame(core, frame.data(), static_cast<int>(frame.size()),
                                         &w, &h, &pts, &seq);
        CHECK(copied == 0, "poll_frame returns 0 with no decoded frame");
        CHECK(w == 0 && h == 0, "poll_frame reports an empty frame geometry when idle");
        CHECK(mg_video_poll_frame(core, nullptr, 16, &w, &h, &pts, &seq) == -1,
              "poll_frame rejects a null destination");
        CHECK(mg_video_poll_frame(core, frame.data(), 0, &w, &h, &pts, &seq) == -1,
              "poll_frame rejects non-positive capacity");
        CHECK(mg_video_poll_frame(nullptr, frame.data(), 16, &w, &h, &pts, &seq) == -1,
              "poll_frame rejects a null core");

        mg_destroy(core);
    }

    // ---- 录制/截图 FFI 契约（S4，ADR-0008）----
    {
        fprintf(stderr, "\n[Group] video recording/capture FFI contract\n");
        const bool has_backend = VideoDecoder::backend_available();
        mg_core_t* core = mg_create();
        CHECK(core != nullptr, "mg_create returns a core");

        // 空路径永远是参数错误，先于后端检查。
        char* r = mg_video_start_recording(core, "");
        CHECK(json_has(r, "INVALID_PARAM"), "start_recording rejects an empty path");
        mg_free_string(r);

        r = mg_video_capture_frame(core, "");
        CHECK(json_has(r, "INVALID_PARAM"), "capture_frame rejects an empty path");
        mg_free_string(r);

        // 没有解码后端 → NOT_SUPPORTED；有后端但没有正在运行的本地源 → INVALID_STATE。
        r = mg_video_start_recording(core, "/tmp/mgcs_rec_test.mkv");
        if (has_backend)
            CHECK(json_has(r, "INVALID_STATE"), "start_recording needs a running local source");
        else
            CHECK(json_has(r, "NOT_SUPPORTED"), "start_recording is NOT_SUPPORTED without a backend");
        mg_free_string(r);

        r = mg_video_capture_frame(core, "/tmp/mgcs_shot_test.png");
        if (has_backend)
            CHECK(json_has(r, "INVALID_STATE"), "capture_frame needs a decoded frame");
        else
            CHECK(json_has(r, "NOT_SUPPORTED"), "capture_frame is NOT_SUPPORTED without a backend");
        mg_free_string(r);

        // 状态查询始终可用：是一个记录当前未在录制的成功信封。
        r = mg_video_recording_status(core);
        CHECK(json_has(r, "\"success\":true"), "recording_status is a success envelope");
        CHECK(json_has(r, "\"recording\":false"), "recording_status reports not recording");
        CHECK(json_has(r, "\"bytes\":0"), "recording_status reports zero bytes when idle");
        mg_free_string(r);

        // 停止录制是幂等的无害操作，任何时候都返回成功状态快照。
        r = mg_video_stop_recording(core);
        CHECK(json_has(r, "\"success\":true"), "stop_recording is an idempotent success");
        CHECK(json_has(r, "\"recording\":false"), "stop_recording leaves recording false");
        mg_free_string(r);

        mg_destroy(core);
    }

    fprintf(stderr, "\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
