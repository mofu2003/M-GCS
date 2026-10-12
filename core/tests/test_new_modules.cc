/**
 * test_new_modules.cc - Tests for recently added modules
 *
 * Covers: camera/gimbal/video FFI, FlightLogReplayer, MavlinkFtpClient
 */
#define MGCORE_EXPORTS
#include "ffi_api.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>
#include "ardupilotmega/mavlink.h"
#include "core/ftp/mavlink_ftp.h"
#include "core/log/flight_log_replayer.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/log/tlog_format.h"
#include "core/settings/mavlink_log_writer.h"
#include "core/video/video_decoder.h"
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();

static bool json_has(const char* json, const char* key) {
    return json && strstr(json, key) != nullptr;
}

namespace {

constexpr uint8_t kFtpOpenFileRO = 4;
constexpr uint8_t kFtpReadFile = 5;
constexpr uint8_t kFtpTerminateSession = 1;
constexpr uint8_t kFtpResetSessions = 2;
constexpr uint8_t kFtpListDirectory = 3;
constexpr uint8_t kFtpCreateFile = 6;
constexpr uint8_t kFtpWriteFile = 7;
constexpr uint8_t kFtpAck = 128;
constexpr uint8_t kFtpNak = 129;
constexpr size_t kFtpHeaderSize = 12;

uint16_t ftp_read_u16(const uint8_t* value) {
    return static_cast<uint16_t>(value[0]) |
           (static_cast<uint16_t>(value[1]) << 8);
}

uint32_t ftp_read_u32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8) |
           (static_cast<uint32_t>(value[2]) << 16) |
           (static_cast<uint32_t>(value[3]) << 24);
}

void ftp_write_u16(uint8_t* output, uint16_t value) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
}

void ftp_write_u32(uint8_t* output, uint32_t value) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

bool decode_ftp_message(const uint8_t* data, size_t size,
                        mavlink_file_transfer_protocol_t* ftp) {
    mavlink_message_t parse_message{};
    mavlink_message_t decoded_message{};
    mavlink_status_t parse_status{};
    mavlink_status_t decoded_status{};
    for (size_t index = 0; index < size; ++index) {
        const uint8_t framing = mavlink_frame_char_buffer(
            &parse_message, &parse_status, data[index],
            &decoded_message, &decoded_status);
        if (framing == MAVLINK_FRAMING_OK &&
            decoded_message.msgid == MAVLINK_MSG_ID_FILE_TRANSFER_PROTOCOL) {
            mavlink_msg_file_transfer_protocol_decode(&decoded_message, ftp);
            return true;
        }
    }
    return false;
}

struct FakeFtpServer {
    struct Request {
        uint16_t sequence = 0;
        uint8_t session = 0;
        uint8_t opcode = 0;
        uint8_t size = 0;
        uint32_t offset = 0;
        std::vector<uint8_t> data;
    };

    mg::core::ftp::MavlinkFtpClient* client = nullptr;
    std::vector<uint8_t> file;
    std::vector<Request> requests;
    uint8_t session = 23;
    size_t max_response_chunk = 71;
    int ignored_open_requests = 0;
    int ignored_read_requests = 0;
    uint32_t reported_file_size_extra = 0;
    bool reject_open = false;
    uint8_t open_error_code = 10;
    bool send_bad_read_offset = false;
    bool bad_read_offset_sent = false;
    // 不回复任何请求：把传输钉在等待状态，用于取消/单会话用例。
    bool silent = false;
    // 目录列表（ListDirectory）返回值，元素形如 "Fname\t123" / "Dname"。
    std::vector<std::string> dir_entries;
    // 上传（CreateFile/WriteFile）落盘内容。
    std::vector<uint8_t> uploaded_file;

    bool send(const uint8_t* bytes, size_t length) {
        mavlink_file_transfer_protocol_t ftp{};
        if (!decode_ftp_message(bytes, length, &ftp)) return false;
        if (silent) return true;

        const uint8_t* payload = ftp.payload;
        Request request;
        request.sequence = ftp_read_u16(payload);
        request.session = payload[2];
        request.opcode = payload[3];
        request.size = payload[4];
        request.offset = ftp_read_u32(payload + 8);
        request.data.assign(payload + kFtpHeaderSize,
                            payload + kFtpHeaderSize + request.size);
        requests.push_back(request);

        if (request.opcode == kFtpResetSessions) {
            reply(request, kFtpAck, 0, 0, {});
            return true;
        }

        if (request.opcode == kFtpOpenFileRO) {
            if (ignored_open_requests > 0) {
                --ignored_open_requests;
                return true;
            }
            if (reject_open) {
                reply(request, kFtpNak, 0, 0, {open_error_code});
            } else {
                std::vector<uint8_t> length_data(4);
                ftp_write_u32(length_data.data(),
                              static_cast<uint32_t>(file.size()) +
                                  reported_file_size_extra);
                // A non-size offset catches clients which read hdr.offset as
                // the OpenFileRO length instead of response data[0..3].
                reply(request, kFtpAck, session, 0xA5A55A5A, length_data);
            }
            return true;
        }

        if (request.opcode == kFtpReadFile) {
            if (ignored_read_requests > 0) {
                --ignored_read_requests;
                return true;
            }
            if (request.offset >= file.size()) {
                reply(request, kFtpNak, session, request.offset, {6});
                return true;
            }

            const size_t count = std::min<size_t>(
                {request.size, max_response_chunk, file.size() - request.offset});
            std::vector<uint8_t> chunk(
                file.begin() + request.offset,
                file.begin() + request.offset + count);
            uint32_t response_offset = request.offset;
            if (send_bad_read_offset && !bad_read_offset_sent) {
                ++response_offset;
                bad_read_offset_sent = true;
            }
            reply(request, kFtpAck, session, response_offset, chunk);
            return true;
        }

        if (request.opcode == kFtpTerminateSession) {
            reply(request, kFtpAck, request.session, 0, {});
            return true;
        }

        if (request.opcode == kFtpListDirectory) {
            if (request.offset >= dir_entries.size()) {
                // MAV_FTP_ERR_EOF = 6 表示列表结束。
                reply(request, kFtpNak, 0, 0, {6});
                return true;
            }
            std::vector<uint8_t> encoded;
            for (size_t index = request.offset; index < dir_entries.size();
                 ++index) {
                encoded.insert(encoded.end(), dir_entries[index].begin(),
                               dir_entries[index].end());
                encoded.push_back(0);
            }
            reply(request, kFtpAck, 0, 0, encoded);
            return true;
        }

        if (request.opcode == kFtpCreateFile) {
            uploaded_file.clear();
            reply(request, kFtpAck, session, 0, {});
            return true;
        }

        if (request.opcode == kFtpWriteFile) {
            uploaded_file.insert(uploaded_file.end(), request.data.begin(),
                                 request.data.end());
            reply(request, kFtpAck, session, request.offset, {});
            return true;
        }

        reply(request, kFtpNak, request.session, request.offset, {7});
        return true;
    }

    void reply(const Request& request, uint8_t opcode,
               uint8_t response_session, uint32_t offset,
               const std::vector<uint8_t>& data) {
        mavlink_file_transfer_protocol_t response{};
        response.target_system = 255;
        uint8_t* payload = response.payload;
        ftp_write_u16(payload, static_cast<uint16_t>(request.sequence + 1));
        payload[2] = response_session;
        payload[3] = opcode;
        payload[4] = static_cast<uint8_t>(data.size());
        payload[5] = request.opcode;
        ftp_write_u32(payload + 8, offset);
        if (!data.empty()) {
            std::memcpy(payload + kFtpHeaderSize, data.data(), data.size());
        }
        client->handle_ftp_message(response);
    }
};

std::vector<uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
}

std::filesystem::path ftp_test_path(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

// ================================================================
// Camera FFI Tests
// ================================================================
static void test_camera_ffi() {
    fprintf(stderr, "\n[Group] Camera FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Camera functions should return valid JSON even when disconnected
    // (they check CHECK_MAVLINK and return error JSON)

    // Take photo - should fail with not connected
    char* r = mg_camera_take_photo(core, 0);
    CHECK(r != nullptr, "camera_take_photo returns non-null");
    CHECK(json_has(r, "error"), "camera_take_photo returns error when disconnected");
    if (r) mg_free_string(r);

    // Start video
    r = mg_camera_start_video(core, 0);
    CHECK(r != nullptr, "camera_start_video returns non-null");
    if (r) mg_free_string(r);

    // Stop video
    r = mg_camera_stop_video(core, 0);
    CHECK(r != nullptr, "camera_stop_video returns non-null");
    if (r) mg_free_string(r);

    // Set zoom
    r = mg_camera_set_zoom(core, 0, 2.0f);
    CHECK(r != nullptr, "camera_set_zoom returns non-null");
    if (r) mg_free_string(r);

    // Set mode
    r = mg_camera_set_mode(core, 0, 1);
    CHECK(r != nullptr, "camera_set_mode returns non-null");
    if (r) mg_free_string(r);

    // Stop capture
    r = mg_camera_stop_capture(core, 0);
    CHECK(r != nullptr, "camera_stop_capture returns non-null");
    if (r) mg_free_string(r);

    // Set focus
    r = mg_camera_set_focus(core, 0, 0, 5.0f);
    CHECK(r != nullptr, "camera_set_focus returns non-null");
    if (r) mg_free_string(r);

    // Request info
    r = mg_camera_request_info(core, 0);
    CHECK(r != nullptr, "camera_request_info returns non-null");
    if (r) mg_free_string(r);

    // Get status - 无设备时是合法状态：success:true + available:false（D5），不是 error
    r = mg_camera_get_status(core, 0);
    CHECK(r != nullptr, "camera_get_status returns non-null");
    CHECK(json_has(r, "\"available\":false"), "camera_get_status reports available:false with no camera");
    CHECK(json_has(r, "\"success\":true"), "camera_get_status success:true (query ok, no device)");
    CHECK(!json_has(r, "\"error\""), "camera_get_status is not an error when no device");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Gimbal FFI Tests
// ================================================================
static void test_gimbal_ffi() {
    fprintf(stderr, "\n[Group] Gimbal FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Gimbal control - should fail with not connected
    char* r = mg_command_gimbal_control(core, -45.0f, 90.0f);
    CHECK(r != nullptr, "gimbal_control returns non-null");
    CHECK(json_has(r, "error"), "gimbal_control returns error when disconnected");
    if (r) mg_free_string(r);

    // Gimbal recenter
    r = mg_command_gimbal_recenter(core);
    CHECK(r != nullptr, "gimbal_recenter returns non-null");
    if (r) mg_free_string(r);

    // Gimbal request info
    r = mg_gimbal_request_info(core);
    CHECK(r != nullptr, "gimbal_request_info returns non-null");
    if (r) mg_free_string(r);

    // Gimbal configure
    r = mg_command_gimbal_configure(core, 0, 1, 1, 0, 0);
    CHECK(r != nullptr, "gimbal_configure returns non-null");
    if (r) mg_free_string(r);

    // 读接口：无设备时同样是 success:true + available:false
    r = mg_gimbal_get_status(core, 0);
    CHECK(r != nullptr, "gimbal_get_status returns non-null");
    CHECK(json_has(r, "\"available\":false"), "gimbal_get_status available:false with no gimbal");
    CHECK(json_has(r, "\"success\":true"), "gimbal_get_status success:true (query ok, no device)");
    if (r) mg_free_string(r);

    r = mg_gimbal_get_attitude(core, 0);
    CHECK(r != nullptr, "gimbal_get_attitude returns non-null");
    CHECK(json_has(r, "\"available\":false"), "gimbal_get_attitude available:false with no gimbal");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Video FFI Tests
// ================================================================
static void test_video_ffi() {
    fprintf(stderr, "\n[Group] Video FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // mg_video_start 需要 uri；空 uri 是参数错误；非空但不可达的 uri 无论是否编译解码后端
    // 都必须失败。关键回归点：它绝不能再退化成「让相机开始录像」。
    const bool video_has_backend = mg::core::video::VideoDecoder::backend_available();
    char* r = mg_video_start(core, "");
    CHECK(r != nullptr, "video_start(empty) returns non-null");
    CHECK(json_has(r, "\"error\""), "video_start(empty) returns error");
    CHECK(json_has(r, "INVALID_PARAM"), "video_start(empty) is INVALID_PARAM");
    if (r) mg_free_string(r);

    r = mg_video_start(core, "rtsp://example.invalid/main");
    CHECK(r != nullptr, "video_start(uri) returns non-null");
    CHECK(json_has(r, "\"error\""), "video_start(uri) returns error on an unreachable source");
    if (video_has_backend)
        CHECK(json_has(r, "SEND_FAILED"), "video_start(uri) is SEND_FAILED when the source cannot open");
    else
        CHECK(json_has(r, "NOT_SUPPORTED"), "video_start(uri) is NOT_SUPPORTED without a backend");
    if (r) mg_free_string(r);

    // video_stop 是幂等的「停止本地视频源」：没有后端或未运行时是无害的 no-op，
    // 返回 success（S1 曾以 NOT_SUPPORTED 报「未实现」，S3/ADR-0008 改为幂等成功）。
    r = mg_video_stop(core);
    CHECK(r != nullptr, "video_stop returns non-null");
    CHECK(json_has(r, "\"success\":true"), "video_stop is an idempotent no-op success");
    if (r) mg_free_string(r);

    // video_status：无设备时 available:false，且绝不谎报 playing:true
    r = mg_video_status(core);
    CHECK(r != nullptr, "video_status returns non-null");
    CHECK(json_has(r, "\"available\":false"), "video_status available:false with no streams");
    CHECK(json_has(r, "\"playing\":false"), "video_status playing:false (no local decode)");
    if (r) mg_free_string(r);

    // video_get_streams：无活动载具时是连接错误
    r = mg_video_get_streams(core);
    CHECK(r != nullptr, "video_get_streams returns non-null");
    CHECK(json_has(r, "\"error\""), "video_get_streams returns error when disconnected");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Failsafe FFI Tests
// ================================================================
static void test_failsafe_ffi() {
    fprintf(stderr, "\n[Group] Failsafe FFI\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Fence enable - should fail with not connected
    char* r = mg_fence_enable(core, 1);
    CHECK(r != nullptr, "fence_enable returns non-null");
    CHECK(json_has(r, "error"), "fence_enable returns error when disconnected");
    if (r) mg_free_string(r);

    // Fence disable
    r = mg_fence_enable(core, 0);
    CHECK(r != nullptr, "fence_disable returns non-null");
    if (r) mg_free_string(r);

    // Battery failsafe configure
    r = mg_battery_failsafe_configure(core, 1, 14.0f, 13.0f);
    CHECK(r != nullptr, "battery_failsafe_configure returns non-null");
    if (r) mg_free_string(r);

    // Failsafe status - returns error when no vehicle (expected)
    r = mg_failsafe_get_status(core);
    CHECK(r != nullptr, "failsafe_get_status returns non-null");
    // When disconnected, returns error JSON; when connected, returns status JSON
    bool has_status_fields = json_has(r, "battery_ok");
    bool has_error = json_has(r, "error");
    CHECK(has_status_fields || has_error, "failsafe_get_status has battery_ok or error");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// FlightLogReplayer Tests
// ================================================================
static void test_replayer() {
    fprintf(stderr, "\n[Group] tlog record -> replay round-trip\n");

    const char* test_file = "test_replay.tlog";
    std::vector<uint16_t> sent_ids;
    size_t bytes_recorded = 0;

    // 1) 用 MavlinkLogWriter 录 3 帧（走公开写入器，覆盖真实录制格式）
    {
        mg::core::settings::MavlinkLogWriter writer;
        CHECK(writer.start_recording(test_file), "writer starts recording");

        uint8_t buf[MAVLINK_MAX_PACKET_LEN];
        for (int i = 0; i < 3; i++) {
            mavlink_message_t msg{};
            switch (i) {
                case 0:
                    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR,
                        MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_STANDBY);
                    break;
                case 1:
                    mavlink_msg_attitude_pack(1, 1, &msg, 100, 0.1f, 0.2f, 0.3f,
                        0.01f, 0.02f, 0.03f);
                    break;
                default:
                    mavlink_msg_attitude_pack(1, 1, &msg, 200, 0.4f, 0.5f, 0.6f,
                        0.04f, 0.05f, 0.06f);
                    break;
            }
            const uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
            writer.write_frame(buf, len);
            sent_ids.push_back(msg.msgid);
        }
        bytes_recorded = writer.bytes_written();
        writer.stop_recording();
    }
    CHECK(sent_ids.size() == 3, "recorded 3 frames");
    CHECK(bytes_recorded > 3 * mg::core::log::tlog::kTimestampBytes,
          "recorded bytes exceed timestamps-only (writer writes frames)");

    // 2) 回放：必须读回同样的消息与时间戳，且无截断
    {
        mg::core::log::FlightLogReplayer replayer;
        CHECK(replayer.open(test_file), "replayer opens the recorded tlog");
        replayer.set_speed(0.0);  // 尽快，不做实时等待
        CHECK(replayer.file_info().message_count == 3, "replayer scans 3 records");

        int idx = 0;
        bool ids_match = true;
        bool timestamps_monotonic = true;
        uint64_t prev_ts = 0;
        while (replayer.next_message()) {
            if (idx < 3 && replayer.current_message().msgid != sent_ids[idx]) ids_match = false;
            const uint64_t ts = replayer.current_timestamp_us();
            if (ts < prev_ts) timestamps_monotonic = false;
            prev_ts = ts;
            idx++;
        }
        CHECK(idx == 3, "replayed exactly 3 messages");
        CHECK(ids_match, "replayed msgids match what was recorded");
        CHECK(timestamps_monotonic, "replayed timestamps are monotonic");
        CHECK(!replayer.truncated(), "clean recording is not reported truncated");
        CHECK(replayer.progress() >= 0.0f, "progress is computed without crash");
    }

    // 3) 截断检测：时间戳 + 半条帧 -> 不产出消息且报告 truncated
    {
        const char* truncated_file = "test_replay_truncated.tlog";
        FILE* f = fopen(truncated_file, "wb");
        CHECK(f != nullptr, "create truncated tlog file");
        if (f) {
            uint8_t ts[mg::core::log::tlog::kTimestampBytes];
            mg::core::log::tlog::encode_timestamp(1700000000000000ULL, ts);
            fwrite(ts, 1, sizeof(ts), f);
            const uint8_t half_frame[3] = {0xFD, 0x09, 0x00};  // MAVLink v2 STX + 残头
            fwrite(half_frame, 1, sizeof(half_frame), f);
            fclose(f);

            mg::core::log::FlightLogReplayer truncated;
            CHECK(truncated.open(truncated_file), "replayer opens truncated tlog");
            truncated.set_speed(0.0);
            CHECK(!truncated.next_message(), "truncated record yields no message");
            CHECK(truncated.truncated(), "truncation is reported to the caller");
            truncated.close();
            remove(truncated_file);
        }
    }

    remove(test_file);
}

// ================================================================
// Extended Command Tests
// ================================================================
static void test_extended_commands() {
    fprintf(stderr, "\n[Group] Extended Commands\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Emergency stop - should fail with not connected
    char* r = mg_command_terminate_flight(core);
    CHECK(r != nullptr, "mg_command_terminate_flight returns non-null");
    if (r) mg_free_string(r);
    r = mg_command_terminate_flight(core);
    CHECK(r != nullptr, "terminate_flight remains callable");
    if (r) mg_free_string(r);

    // Guided velocity
    r = mg_command_guided_velocity(core, 1.0f, 0.0f, 0.0f);
    CHECK(r != nullptr, "guided_velocity returns non-null");
    if (r) mg_free_string(r);

    // Guided pause
    r = mg_command_guided_pause(core);
    CHECK(r != nullptr, "guided_pause returns non-null");
    if (r) mg_free_string(r);

    // Orbit
    r = mg_command_orbit(core, "{\"lat\":39.9,\"lon\":116.4,\"radius\":50,\"speed\":5}");
    CHECK(r != nullptr, "orbit returns non-null");
    if (r) mg_free_string(r);

    // Set ROI
    r = mg_command_set_roi(core, 39.9, 116.4, 100.0f);
    CHECK(r != nullptr, "set_roi returns non-null");
    if (r) mg_free_string(r);

    // Stop ROI
    r = mg_command_stop_roi(core);
    CHECK(r != nullptr, "stop_roi returns non-null");
    if (r) mg_free_string(r);

    // Landing gear
    r = mg_command_landing_gear(core, 1);
    CHECK(r != nullptr, "landing_gear returns non-null");
    if (r) mg_free_string(r);

    // Change heading（QGC 地图菜单 Set Heading）
    r = mg_command_change_heading(core, 90.0f, 30.0f, 1);
    CHECK(r != nullptr, "change_heading returns non-null");
    if (r) mg_free_string(r);

    // Change altitude（QGC guidedModeChangeAltitude）
    r = mg_command_change_altitude(core, 10.0f);
    CHECK(r != nullptr, "change_altitude returns non-null");
    if (r) mg_free_string(r);

    // 非有限值必须本地拒绝
    r = mg_command_change_altitude(core, NAN);
    CHECK(r != nullptr, "change_altitude NaN returns non-null");
    CHECK(strstr(r, "\"success\":true") == nullptr,
          "change_altitude rejects NaN");
    if (r) mg_free_string(r);

    // Set estimator origin（QGC 地图菜单 Set Estimator Origin）
    r = mg_command_set_global_origin(core, 39.9042, 116.4074, 42.5f);
    CHECK(r != nullptr, "set_global_origin returns non-null");
    if (r) mg_free_string(r);

    // 越界经纬度必须本地拒绝，不能把无效坐标发给飞控。
    r = mg_command_set_global_origin(core, 120.0, 116.4, 10.0f);
    CHECK(r != nullptr, "set_global_origin invalid lat returns non-null");
    CHECK(strstr(r, "\"success\":true") == nullptr,
          "set_global_origin rejects lat > 90");
    if (r) mg_free_string(r);

    r = mg_command_set_global_origin(core, 39.9, 200.0, 10.0f);
    CHECK(r != nullptr, "set_global_origin invalid lon returns non-null");
    CHECK(strstr(r, "\"success\":true") == nullptr,
          "set_global_origin rejects lon > 180");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Vehicle Data Tests
// ================================================================
static void test_vehicle_data() {
    fprintf(stderr, "\n[Group] Vehicle Data\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Vehicle count
    char* r = mg_vehicle_count(core);
    CHECK(r != nullptr, "vehicle_count returns non-null");
    if (r) mg_free_string(r);

    // Flight time
    r = mg_vehicle_get_flight_time(core);
    CHECK(r != nullptr, "flight_time returns non-null");
    CHECK(json_has(r, "flight_time_s"), "flight_time has 'flight_time_s'");
    if (r) mg_free_string(r);

    // ADSB traffic
    r = mg_vehicle_get_adsb_traffic(core);
    CHECK(r != nullptr, "adsb_traffic returns non-null");
    CHECK(json_has(r, "vehicles"), "adsb_traffic has 'vehicles'");
    if (r) mg_free_string(r);

    // Distance sensors
    r = mg_vehicle_get_distance_sensors(core);
    CHECK(r != nullptr, "distance_sensors returns non-null");
    CHECK(json_has(r, "sensors"), "distance_sensors has 'sensors'");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// Telemetry Freshness Tests
// ================================================================
static void test_telemetry_freshness() {
    fprintf(stderr, "\n[Group] Telemetry Freshness\n");

    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create");

    // Freshness check - should work even without connection
    char* r = mg_telemetry_get_freshness(core);
    CHECK(r != nullptr, "telemetry_freshness returns non-null");
    CHECK(json_has(r, "heartbeat"), "telemetry_freshness has heartbeat");
    CHECK(json_has(r, "gps"), "telemetry_freshness has gps");
    CHECK(json_has(r, "attitude"), "telemetry_freshness has attitude");
    if (r) mg_free_string(r);

    mg_destroy(core);
}

// ================================================================
// MAVLink FTP Tests
// ================================================================
static void test_mavlink_ftp_download() {
    fprintf(stderr, "\n[Group] MAVLink FTP download protocol\n");

    std::string uart_text = "UARTV1\n";
    for (int index = 0; index < 12; ++index) {
        uart_text += "SERIAL" + std::to_string(index) + " UART" +
                     std::to_string(index + 1) +
                     " TX=     100 RX=     200 FlowCtrl=" +
                     std::to_string(index % 3) + "\n";
    }

    FakeFtpServer server;
    server.file.assign(uart_text.begin(), uart_text.end());
    mg::core::ftp::MavlinkFtpClient client(
        [&](const uint8_t* data, size_t size) {
            return server.send(data, size);
        });
    server.client = &client;

    const auto output_path = ftp_test_path("mgcs_ftp_uarts_test.txt");
    std::error_code ignored;
    std::filesystem::remove(output_path, ignored);

    uint32_t last_received = 0;
    uint32_t last_total = 0;
    mg::core::ftp::FtpResult result;
    const bool downloaded = client.download_file(
        1, 1, "@SYS/uarts.txt", output_path.string(),
        [&](uint32_t received, uint32_t total) {
            last_received = received;
            last_total = total;
        },
        &result);

    CHECK(downloaded && result.success,
          "OpenFileRO ACK and multi-chunk ReadFile download succeeds");
    CHECK(read_binary_file(output_path) == server.file,
          "downloaded UART metadata matches byte-for-byte");
    CHECK(last_received == server.file.size() && last_total == server.file.size(),
          "FTP progress reaches exact file length from OpenFileRO data[0..3]");

    bool open_path_exact = false;
    bool reset_before_open = false;
    bool terminated_session = false;
    bool read_requests_valid = true;
    bool read_offsets_advance = true;
    uint32_t previous_offset = 0;
    bool saw_read = false;
    for (size_t index = 0; index < server.requests.size(); ++index) {
        const auto& request = server.requests[index];
        if (request.opcode == kFtpResetSessions) {
            reset_before_open = index + 1 < server.requests.size() &&
                server.requests[index + 1].opcode == kFtpOpenFileRO;
        }
        if (request.opcode == kFtpOpenFileRO) {
            const std::string path(request.data.begin(), request.data.end());
            open_path_exact = path == "@SYS/uarts.txt" && request.size == path.size();
        } else if (request.opcode == kFtpReadFile) {
            const uint8_t expected_size = static_cast<uint8_t>(
                std::min<size_t>(239, server.file.size() - request.offset));
            read_requests_valid = read_requests_valid &&
                request.session == server.session &&
                request.size == expected_size && request.size != 0;
            if (saw_read) {
                read_offsets_advance = read_offsets_advance &&
                    request.offset > previous_offset;
            }
            previous_offset = request.offset;
            saw_read = true;
        } else if (request.opcode == kFtpTerminateSession) {
            terminated_session = request.session == server.session;
        }
    }
    CHECK(reset_before_open,
          "ResetSessions opcode 2 is sent immediately before OpenFileRO");
    CHECK(open_path_exact, "OpenFileRO sends the exact non-NUL path payload");
    CHECK(saw_read && read_requests_valid,
          "ReadFile requests carry remaining byte count and session id");
    CHECK(read_offsets_advance,
          "ReadFile request offsets advance from acknowledged byte counts");
    CHECK(terminated_session,
          "TerminateSession opcode 1 closes the ArduPilot read session");

    std::filesystem::remove(output_path, ignored);
}

static void test_mavlink_ftp_retry_and_validation() {
    fprintf(stderr, "\n[Group] MAVLink FTP retry and validation\n");

    FakeFtpServer retry_server;
    const std::string content =
        "UARTV1\nSERIAL1 USART3 TX=10 RX=20 FlowCtrl=2\n";
    retry_server.file.assign(content.begin(), content.end());
    retry_server.ignored_read_requests = 1;
    mg::core::ftp::MavlinkFtpClient retry_client(
        [&](const uint8_t* data, size_t size) {
            return retry_server.send(data, size);
        });
    retry_server.client = &retry_client;
    retry_client.set_timeout_ms(2);

    const auto retry_path = ftp_test_path("mgcs_ftp_retry_test.txt");
    std::error_code ignored;
    std::filesystem::remove(retry_path, ignored);
    mg::core::ftp::FtpResult retry_result;
    const bool retry_ok = retry_client.download_file(
        1, 1, "@SYS/uarts.txt", retry_path.string(), nullptr, &retry_result);
    CHECK(retry_ok && read_binary_file(retry_path) == retry_server.file,
          "timed-out ReadFile request is retried and download completes");

    std::vector<FakeFtpServer::Request> read_requests;
    for (const auto& request : retry_server.requests) {
        if (request.opcode == kFtpReadFile) read_requests.push_back(request);
    }
    CHECK(read_requests.size() >= 2 &&
          read_requests[0].sequence == read_requests[1].sequence &&
          read_requests[0].offset == read_requests[1].offset &&
          read_requests[0].size == read_requests[1].size,
          "FTP retry reuses the exact sequence, offset, and size");
    std::filesystem::remove(retry_path, ignored);

    FakeFtpServer timeout_server;
    timeout_server.file = retry_server.file;
    timeout_server.ignored_open_requests = 4;
    mg::core::ftp::MavlinkFtpClient timeout_client(
        [&](const uint8_t* data, size_t size) {
            return timeout_server.send(data, size);
        });
    timeout_server.client = &timeout_client;
    timeout_client.set_timeout_ms(2);
    const auto timeout_path = ftp_test_path("mgcs_ftp_timeout_test.txt");
    std::filesystem::remove(timeout_path, ignored);
    mg::core::ftp::FtpResult timeout_result;
    CHECK(!timeout_client.download_file(
              1, 1, "@SYS/uarts.txt", timeout_path.string(),
              nullptr, &timeout_result),
          "OpenFileRO fails after bounded retries");
    const bool after_timeout_ok = timeout_client.download_file(
        1, 1, "@SYS/uarts.txt", timeout_path.string(), nullptr, &timeout_result);
    std::vector<uint16_t> open_sequences;
    for (const auto& request : timeout_server.requests) {
        if (request.opcode == kFtpOpenFileRO) {
            open_sequences.push_back(request.sequence);
        }
    }
    CHECK(after_timeout_ok && open_sequences.size() >= 5 &&
          open_sequences[0] == open_sequences[3] &&
          open_sequences[4] != open_sequences[3],
          "new transaction advances sequence after all retries time out");
    std::filesystem::remove(timeout_path, ignored);

    FakeFtpServer bad_offset_server;
    bad_offset_server.file = retry_server.file;
    bad_offset_server.send_bad_read_offset = true;
    mg::core::ftp::MavlinkFtpClient bad_offset_client(
        [&](const uint8_t* data, size_t size) {
            return bad_offset_server.send(data, size);
        });
    bad_offset_server.client = &bad_offset_client;
    mg::core::ftp::FtpResult bad_offset_result;
    const auto bad_offset_path = ftp_test_path("mgcs_ftp_bad_offset_test.txt");
    std::filesystem::remove(bad_offset_path, ignored);
    CHECK(!bad_offset_client.download_file(
              1, 1, "@SYS/uarts.txt", bad_offset_path.string(),
              nullptr, &bad_offset_result) &&
          bad_offset_result.error == "invalid read response",
          "ReadFile ACK with a mismatched offset is rejected");
    CHECK(!std::filesystem::exists(bad_offset_path),
          "invalid ReadFile response does not create an output file");
}

static void test_mavlink_ftp_nak_and_empty_file() {
    fprintf(stderr, "\n[Group] MAVLink FTP NAK and empty file\n");

    FakeFtpServer nak_server;
    nak_server.reject_open = true;
    nak_server.open_error_code = 10;
    mg::core::ftp::MavlinkFtpClient nak_client(
        [&](const uint8_t* data, size_t size) {
            return nak_server.send(data, size);
        });
    nak_server.client = &nak_client;
    mg::core::ftp::FtpResult nak_result;
    const auto nak_path = ftp_test_path("mgcs_ftp_nak_test.txt");
    std::error_code ignored;
    std::filesystem::remove(nak_path, ignored);
    CHECK(!nak_client.download_file(
              1, 1, "@SYS/missing.txt", nak_path.string(),
              nullptr, &nak_result) &&
          nak_result.error_code == 10,
          "OpenFileRO NAK reads MAV_FTP_ERR from response data[0]");
    CHECK(!std::filesystem::exists(nak_path),
          "OpenFileRO NAK does not create an output file");

    FakeFtpServer generated_file_server;
    const std::string generated_content =
        "UARTV1\nSERIAL1 USART3 TX=10 RX=20 FlowCtrl=2\n";
    generated_file_server.file.assign(
        generated_content.begin(), generated_content.end());
    generated_file_server.reported_file_size_extra = 32;
    mg::core::ftp::MavlinkFtpClient generated_file_client(
        [&](const uint8_t* data, size_t size) {
            return generated_file_server.send(data, size);
        });
    generated_file_server.client = &generated_file_client;
    mg::core::ftp::FtpResult generated_file_result;
    const auto generated_file_path =
        ftp_test_path("mgcs_ftp_generated_file_test.txt");
    std::filesystem::remove(generated_file_path, ignored);
    const bool generated_file_ok = generated_file_client.download_file(
        1, 1, "@SYS/uarts.txt", generated_file_path.string(), nullptr,
        &generated_file_result);
    CHECK(generated_file_ok && generated_file_result.success &&
              read_binary_file(generated_file_path) ==
                  generated_file_server.file,
          "ReadFile EOF completes a generated ArduPilot system file");
    std::filesystem::remove(generated_file_path, ignored);

    FakeFtpServer empty_server;
    mg::core::ftp::MavlinkFtpClient empty_client(
        [&](const uint8_t* data, size_t size) {
            return empty_server.send(data, size);
        });
    empty_server.client = &empty_client;
    mg::core::ftp::FtpResult empty_result;
    const auto empty_path = ftp_test_path("mgcs_ftp_empty_test.txt");
    std::filesystem::remove(empty_path, ignored);
    const bool empty_ok = empty_client.download_file(
        1, 1, "@SYS/empty.txt", empty_path.string(), nullptr, &empty_result);
    bool sent_read = false;
    for (const auto& request : empty_server.requests) {
        sent_read = sent_read || request.opcode == kFtpReadFile;
    }
    CHECK(empty_ok && std::filesystem::exists(empty_path) &&
          std::filesystem::file_size(empty_path) == 0,
          "zero-length FTP file is created successfully");
    CHECK(!sent_read, "zero-length FTP file skips ReadFile requests");
    std::filesystem::remove(empty_path, ignored);
}

// 异步 FTP façade（P1）：FFI 调用线程不能阻塞在传输上，必须能起线程、
// 读进度、取消，并且同一时刻只允许一个传输。
static void test_mavlink_ftp_async_download_and_list() {
    fprintf(stderr, "\n[Group] MAVLink FTP async transfer\n");

    FakeFtpServer server;
    const std::string content =
        "UARTV1\nSERIAL1 USART3 TX=10 RX=20 FlowCtrl=2\n";
    server.file.assign(content.begin(), content.end());
    server.dir_entries = {"S208.42 1", "Fuarts.txt\t123", "Dlogs",
                          "Fparams.txt\t42"};
    mg::core::ftp::MavlinkFtpClient client(
        [&](const uint8_t* data, size_t size) {
            return server.send(data, size);
        });
    server.client = &client;

    const auto output_path = ftp_test_path("mgcs_ftp_async_download.txt");
    std::error_code ignored;
    std::filesystem::remove(output_path, ignored);

    CHECK(client.start_download_file(1, 1, "@SYS/uarts.txt",
                                     output_path.string()),
          "async download returns to the caller immediately");
    const auto live = client.transfer_status();
    CHECK(live.task == mg::core::ftp::MavlinkFtpClient::FtpTask::Download,
          "status names the running download task");
    CHECK(client.wait_for_completion(5000),
          "wait_for_completion observes the background finish");
    const auto done = client.transfer_status();
    CHECK(done.finished && done.success && !done.cancelled,
          "async download reports completed success");
    CHECK(done.remote_path == "@SYS/uarts.txt" && done.target_sys == 1 &&
              done.target_comp == 1,
          "async status echoes transfer target and remote path");
    CHECK(done.transferred_bytes == server.file.size() &&
              done.total_bytes == server.file.size(),
          "async status exposes exact received/total progress");
    CHECK(read_binary_file(output_path) == server.file,
          "async download writes the file byte-for-byte");
    CHECK(!client.transfer_active(), "worker is idle after completion");
    std::filesystem::remove(output_path, ignored);

    CHECK(client.start_list_directory(1, 1, "@SYS"),
          "client accepts a new transfer after completion");
    CHECK(client.wait_for_completion(5000), "list transfer finishes");
    const auto listed = client.transfer_status();
    CHECK(listed.task == mg::core::ftp::MavlinkFtpClient::FtpTask::List &&
              listed.finished && listed.success,
          "async list reports completed success");
    bool have_uart = false;
    bool have_dir = false;
    bool have_param = false;
    for (const auto& entry : listed.entries) {
        if (entry.name == "uarts.txt" && !entry.is_directory &&
            entry.size == 123) {
            have_uart = true;
        }
        if (entry.name == "logs" && entry.is_directory) have_dir = true;
        if (entry.name == "params.txt" && entry.size == 42) have_param = true;
    }
    CHECK(have_uart && have_dir && have_param,
          "async list returns parsed file and directory entries");
}

static void test_mavlink_ftp_async_single_session_and_cancel() {
    fprintf(stderr, "\n[Group] MAVLink FTP async cancel\n");

    FakeFtpServer server;
    server.silent = true;  // 不回复，把传输钉在等待状态
    mg::core::ftp::MavlinkFtpClient client(
        [&](const uint8_t* data, size_t size) {
            return server.send(data, size);
        });
    server.client = &client;

    const auto output_path = ftp_test_path("mgcs_ftp_async_cancel.txt");
    std::error_code ignored;
    std::filesystem::remove(output_path, ignored);

    CHECK(client.start_download_file(1, 1, "@SYS/uarts.txt",
                                     output_path.string()),
          "download starts while the server stays silent");
    CHECK(client.transfer_active(), "transfer_active is true while in flight");
    CHECK(!client.start_upload_file(1, 1, output_path.string(), "@SYS/x.txt"),
          "a second transfer is rejected while one is in flight");
    CHECK(!client.wait_for_completion(50),
          "wait_for_completion times out before the server responds");
    CHECK(client.transfer_status().active,
          "status still reports active after the wait times out");

    client.cancel();
    CHECK(client.wait_for_completion(2000),
          "cancel ends the transfer instead of waiting out the timeout");
    const auto cancelled = client.transfer_status();
    CHECK(cancelled.finished && cancelled.cancelled && !cancelled.success,
          "cancelled transfer is reported as cancelled, not failed");
    CHECK(!client.transfer_active(), "worker is idle after cancel");

    const std::string content = "UARTV1\nSERIAL1 USART3 TX=1 RX=2\n";
    server.silent = false;
    server.file.assign(content.begin(), content.end());
    CHECK(client.start_download_file(1, 1, "@SYS/uarts.txt",
                                     output_path.string()),
          "client accepts a new transfer after a cancel");
    CHECK(client.wait_for_completion(5000), "reused transfer finishes");
    CHECK(client.transfer_status().success &&
              read_binary_file(output_path) == server.file,
          "transfer after cancel downloads correctly");
    std::filesystem::remove(output_path, ignored);
}

static void test_mavlink_ftp_async_upload() {
    fprintf(stderr, "\n[Group] MAVLink FTP async upload\n");

    const std::string content =
        "UARTV1\nSERIAL1 USART3 TX=10 RX=20 FlowCtrl=2\n";
    const auto local_path = ftp_test_path("mgcs_ftp_async_upload_src.txt");
    std::error_code ignored;
    {
        std::ofstream out(local_path, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    FakeFtpServer server;
    mg::core::ftp::MavlinkFtpClient client(
        [&](const uint8_t* data, size_t size) {
            return server.send(data, size);
        });
    server.client = &client;

    CHECK(client.start_upload_file(1, 1, local_path.string(), "@SYS/out.txt"),
          "async upload starts");
    CHECK(client.wait_for_completion(5000), "upload transfer finishes");
    const auto status = client.transfer_status();
    CHECK(status.task == mg::core::ftp::MavlinkFtpClient::FtpTask::Upload &&
              status.finished && status.success,
          "async upload reports completed success");
    CHECK(status.transferred_bytes == content.size(),
          "upload reports the bytes written");
    CHECK(std::string(server.uploaded_file.begin(), server.uploaded_file.end()) ==
              content,
          "uploaded bytes reach the server unchanged");
    std::filesystem::remove(local_path, ignored);
}

// 录制接线：链路适配器收到/发出的完整帧必须真的落进 .tlog。
static void test_tlog_recording_wiring() {
    fprintf(stderr, "\n[Group] tlog recording is wired to the adapter\n");

    const char* file = "test_tlog_wiring.tlog";
    mg::core::mavlink::MavlinkAdapter adapter;
    mg::core::settings::MavlinkLogWriter writer;

    std::vector<std::vector<uint8_t>> sent;
    adapter.set_send_callback([&](const uint8_t* data, size_t size) -> bool {
        sent.emplace_back(data, data + size);
        return true;
    });
    adapter.set_log_sink(&writer);
    CHECK(writer.start_recording(file), "adapter recording starts");

    // RX：注入一条 heartbeat，必须进日志。
    mavlink_message_t hb{};
    mavlink_msg_heartbeat_pack(1, 1, &hb, MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA, 0, 0, MAV_STATE_STANDBY);
    uint8_t rbuf[MAVLINK_MAX_PACKET_LEN];
    const uint16_t rlen = mavlink_msg_to_send_buffer(rbuf, &hb);
    adapter.feed_raw(rbuf, rlen);

    // TX：主动发一条 REQUEST_MESSAGE_INTERVAL，也必须进日志。
    adapter.request_message_interval(1, 1, MAVLINK_MSG_ID_ATTITUDE, 1000000);
    writer.stop_recording();
    CHECK(!sent.empty(), "adapter delivered the outgoing frame");

    mg::core::log::FlightLogReplayer replayer;
    CHECK(replayer.open(file), "replay opens the recording");
    replayer.set_speed(0.0);
    int total = 0;
    bool has_hb = false;
    bool has_req = false;
    while (replayer.next_message()) {
        ++total;
        const uint32_t id = replayer.current_message().msgid;
        if (id == MAVLINK_MSG_ID_HEARTBEAT) has_hb = true;
        if (id == MAVLINK_MSG_ID_COMMAND_LONG) has_req = true;
    }
    CHECK(has_hb, "RX heartbeat recorded into tlog");
    CHECK(has_req, "TX request recorded into tlog");
    CHECK(total >= 2, "tlog replay sees both directions");
    CHECK(!replayer.truncated(), "recording is not truncated");

    std::error_code ignored;
    std::filesystem::remove(file, ignored);
}

// ================================================================
int main() {
    fprintf(stderr, "=== New Module Tests ===\n");

    test_camera_ffi();
    test_gimbal_ffi();
    test_video_ffi();
    test_failsafe_ffi();
    test_replayer();
    test_tlog_recording_wiring();
    test_extended_commands();
    test_vehicle_data();
    test_telemetry_freshness();
    test_mavlink_ftp_download();
    test_mavlink_ftp_retry_and_validation();
    test_mavlink_ftp_nak_and_empty_file();
    test_mavlink_ftp_async_download_and_list();
    test_mavlink_ftp_async_single_session_and_cancel();
    test_mavlink_ftp_async_upload();

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
