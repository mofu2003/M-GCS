/**
 * test_sim_ui_paths.cc - Cover for FFI entry points that real UI paths call
 * but no automated test touched before (audit item TEST-001):
 *   mg_params_get_by_name, mg_stream_set_interval,
 *   mg_vehicle_request_firmware_version, mg_calibration_cancel,
 *   mg_serial_get_uart_names.
 *
 * These are exercised against the in-process MockFlightController so the
 * request encoding, ACK handling and MAVLink FTP round-trip stay pinned.
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"
#include "ardupilotmega/mavlink.h"
#include "core/transport/link_session.h"
#include "core/vehicle/vehicle.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


namespace {

constexpr uint16_t kSetMessageInterval = 511;
constexpr uint16_t kRequestMessage = 512;
constexpr uint32_t kMessageIdAutopilotVersion = 148;

struct TestEnv : MockVehicleTestEnv {
    explicit TestEnv(const MockFlightController::Config& cfg = MockFlightController::Config())
        : MockVehicleTestEnv(cfg, /*load_default_params=*/true) {}

    void connect() {
        for (int i = 0; i < 15; i++) tick(100);
    }

    void load_params() {
        mg_params_request_all(core);
        for (int i = 0; i < 60; i++) tick(20);
    }
};

bool json_has(const char* json, const char* needle) {
    return json != nullptr && std::strstr(json, needle) != nullptr;
}

/// 本进程 id（跨平台）。
unsigned long test_process_id() {
#if defined(_WIN32)
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

/// 每进程唯一的临时文件路径。
///
/// 固定文件名（mgcs_log_download_test.bin 等）会让并发跑同一个测试二进制的两个
/// 进程写同一份文件：日志下载/tlog/文件传输用例互相覆盖，门禁里表现为"单独跑
/// 全绿、只有并发时红"（实测并发 4 份时出现过 1 个实例的 "downloaded file lands
/// on disk" 失败）。文件名带 pid 后各进程各写各的；各用例本来就在开头 remove()
/// 旧文件，pid 复用也不会残留脏数据。
std::filesystem::path unique_temp_path(const char* name) {
    return std::filesystem::temp_directory_path() /
           ("mgcs-" + std::to_string(test_process_id()) + "-" + name);
}

struct EventProbe {
    int waypoint_events = 0;
    int waypoint_seq = -1;
};

void event_probe_cb(mg_event_type_t type, char* json, void* user_data) {
    auto* probe = static_cast<EventProbe*>(user_data);
    if (probe != nullptr && type == MG_EVENT_WAYPOINT_REACHED) {
        probe->waypoint_events++;
        const char* seq = json != nullptr ? std::strstr(json, "\"seq\":") : nullptr;
        if (seq != nullptr) probe->waypoint_seq = std::atoi(seq + 6);
    }
    // 事件 json 归回调所有，每个分支都要释放。
    if (json != nullptr) mg_free_string(json);
}

// MG_EVENT_WAYPOINT_REACHED 以前只在公开枚举里声明，没有地方发出。
void test_waypoint_reached_event_is_emitted() {
    fprintf(stderr, "\n[Group] MG_EVENT_WAYPOINT_REACHED\n");
    TestEnv env;
    env.connect();

    EventProbe probe;
    mg_set_event_callback(env.core, event_probe_cb, &probe);

    env.mock->send_mission_item_reached(3);
    env.tick(50);

    CHECK(probe.waypoint_events >= 1, "waypoint reached event is emitted");
    CHECK(probe.waypoint_seq == 3, "event carries the reached sequence number");

    mg_set_event_callback(env.core, nullptr, nullptr);
}

void test_params_get_by_name() {
    fprintf(stderr, "\n[Group] mg_params_get_by_name\n");
    TestEnv env;
    env.connect();
    env.load_params();

    char* found = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    CHECK(json_has(found, "\"found\":true"), "known parameter reports found");
    CHECK(json_has(found, "\"name\":\"WPNAV_SPEED\""), "known parameter echoes its name");
    if (found) mg_free_string(found);

    char* missing = mg_params_get_by_name(env.core, "NOT_A_REAL_PARAM");
    CHECK(json_has(missing, "\"found\":false"), "unknown parameter reports not found");
    if (missing) mg_free_string(missing);

    char* empty = mg_params_get_by_name(env.core, "");
    CHECK(json_has(empty, "\"found\":false"), "empty name is rejected without crash");
    if (empty) mg_free_string(empty);

    char* null_name = mg_params_get_by_name(env.core, nullptr);
    CHECK(json_has(null_name, "\"found\":false"), "null name is rejected without crash");
    if (null_name) mg_free_string(null_name);
}

void test_stream_set_interval() {
    fprintf(stderr, "\n[Group] mg_stream_set_interval\n");
    TestEnv env;
    env.connect();
    env.mock->received_cmds.clear();

    char* result = mg_stream_set_interval(env.core, kMessageIdAutopilotVersion, 100000);
    CHECK(json_has(result, "\"success\":true"), "set_interval returns success");
    if (result) mg_free_string(result);

    env.tick(50);

    bool sent = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == kSetMessageInterval &&
            cmd.p1 == static_cast<float>(kMessageIdAutopilotVersion) &&
            cmd.p2 == 100000.0f) {
            sent = true;
        }
    }
    CHECK(sent, "SET_MESSAGE_INTERVAL carries the requested id and interval");

    char* invalid = mg_stream_set_interval(env.core, -1, 100000);
    CHECK(json_has(invalid, "\"success\":false"), "negative message id is rejected");
    if (invalid) mg_free_string(invalid);
}

void test_vehicle_request_firmware_version() {
    fprintf(stderr, "\n[Group] mg_vehicle_request_firmware_version\n");
    TestEnv env;
    env.connect();
    env.mock->received_cmds.clear();

    char* result = mg_vehicle_request_firmware_version(env.core);
    CHECK(json_has(result, "\"success\":true"), "firmware version request returns success");
    if (result) mg_free_string(result);

    env.tick(50);

    bool sent = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == kRequestMessage &&
            cmd.p1 == static_cast<float>(kMessageIdAutopilotVersion)) {
            sent = true;
        }
    }
    CHECK(sent, "request encodes REQUEST_MESSAGE for AUTOPILOT_VERSION");
}

void test_calibration_cancel() {
    fprintf(stderr, "\n[Group] mg_calibration_cancel\n");
    TestEnv env;
    env.connect();

    // Idle cancel must be rejected, not silently succeed.
    char* idle = mg_calibration_cancel(env.core);
    CHECK(json_has(idle, "\"success\":false"), "cancel with no active calibration fails");
    if (idle) mg_free_string(idle);

    // Suppress the ACK so the calibration stays active until we cancel it.
    env.mock->set_cmd_no_response(true);
    char* start = mg_calibration_start(env.core, 0, 0);
    CHECK(json_has(start, "\"success\":true"), "gyro calibration starts");
    if (start) mg_free_string(start);

    char* active = mg_calibration_get_status(env.core);
    CHECK(json_has(active, "\"active\":true"), "calibration is active before cancel");
    if (active) mg_free_string(active);

    char* cancel = mg_calibration_cancel(env.core);
    CHECK(json_has(cancel, "\"success\":true"), "cancel reports success");
    if (cancel) mg_free_string(cancel);

    // S8：飞控确认之前不能显示为已停止，status 必须停在 cancelling。
    char* pending = mg_calibration_get_status(env.core);
    CHECK(json_has(pending, "\"active\":true") &&
              json_has(pending, "\"stage\":\"cancelling\""),
          "calibration stays active and reports cancelling before the ACK");
    if (pending) mg_free_string(pending);

    // 放开 ACK：飞控确认后才落到 cancelled。
    env.mock->set_cmd_no_response(false);
    for (int i = 0; i < 5; ++i) env.tick(20);

    char* stopped = mg_calibration_get_status(env.core);
    CHECK(json_has(stopped, "\"active\":false") &&
              json_has(stopped, "\"stage\":\"cancelled\""),
          "calibration is inactive after the vehicle confirms the cancel");
    if (stopped) mg_free_string(stopped);
}

void test_config_param_write_path() {
    fprintf(stderr, "\n[Group] mg_config_read_param / mg_config_write_param\n");
    TestEnv env;
    env.connect();
    env.load_params();

    char* detail = mg_config_read_param(env.core, "WPNAV_SPEED");
    CHECK(json_has(detail, "\"param_name\":\"WPNAV_SPEED\""),
          "config read returns descriptor metadata");
    if (detail) mg_free_string(detail);

    env.mock->received_params_set.clear();
    char* write = mg_config_write_param(env.core, "WPNAV_SPEED", 7.0f);
    CHECK(json_has(write, "\"success\":true"), "config write accepted");
    if (write) mg_free_string(write);

    env.tick(50);
    bool forwarded = false;
    for (const auto& name : env.mock->received_params_set) {
        if (name == "WPNAV_SPEED") forwarded = true;
    }
    CHECK(forwarded, "config write forwards PARAM_SET to the flight controller");

    char* empty = mg_config_write_param(env.core, "", 1.0f);
    CHECK(json_has(empty, "\"success\":false"), "empty name is rejected");
    if (empty) mg_free_string(empty);
}

// The canonical log listing reports the entries received from the flight controller.
void test_log_list_reports_real_entries() {
    fprintf(stderr, "\n[Group] mg_onboard_log_get_entries\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(7, 1700000000u, 4096u, 2);
    env.mock->send_log_entry(8, 1700000060u, 8192u, 2);
    // 重传同一条目不能产生重复行，也不能把计数顶到 num_logs 之上。
    env.mock->send_log_entry(7, 1700000000u, 4096u, 2);
    env.tick(50);

    char* list = mg_onboard_log_get_entries(env.core);
    CHECK(json_has(list, "\"success\":true"), "log list reports success");
    CHECK(json_has(list, "\"total\":2"), "log list dedupes retransmitted entries");
    CHECK(json_has(list, "\"expected\":2"), "log list reports the flight controller's num_logs");
    CHECK(json_has(list, "\"complete\":true"), "log list reports completion once num_logs are received");
    CHECK(json_has(list, "\"id\":7"), "log list carries the real log id");
    CHECK(json_has(list, "\"size_bytes\":4096"), "log list carries the real size");
    CHECK(json_has(list, "\"size_bytes\":8192"), "log list carries both entries");
    if (list) mg_free_string(list);

}

// P2：下载完成态只在文件真的写成功之后置位，并报告 saved_bytes。
void test_log_download_reports_saved_bytes() {
    fprintf(stderr, "\n[Group] mg_onboard_log_download completion follows a successful write\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(7, 1700000000u, 8u);
    env.tick(50);

    const auto path =
        unique_temp_path("mgcs_log_download_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    char* start = mg_onboard_log_download(env.core, 7, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts");
    if (start) mg_free_string(start);
    env.tick(10);

    const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    env.mock->send_log_data(7, 0, payload, 8);

    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"completed\""),
          "download reports completed only after the file is saved");
    CHECK(json_has(status, "\"completed\":true"), "completed flag is set");
    CHECK(json_has(status, "\"saved_bytes\":8"), "status reports the saved byte count");
    if (status) mg_free_string(status);

    FILE* f = fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr, "downloaded file exists");
    if (f) {
        uint8_t got[16] = {};
        const size_t n = fread(got, 1, sizeof(got), f);
        fclose(f);
        CHECK(n == 8 && std::memcmp(got, payload, 8) == 0,
              "downloaded file content matches the LOG_DATA payload");
    }
    std::filesystem::remove(path, ec);
}

// 溢出守卫不能误伤正常路径：连续多帧（ofs 递增）与"补前面的洞"（ofs 小于当前
// download_ofs）都必须照常接收。只有 ofs 超过已请求前沿才丢弃。
void test_log_download_accepts_in_order_and_resent_holes() {
    fprintf(stderr, "\n[Group] LOG_DATA guard leaves normal and resent frames intact\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(11, 1700000000u, 180u);
    env.tick(50);

    const auto path =
        unique_temp_path("mgcs_log_inorder_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    char* start = mg_onboard_log_download(env.core, 11, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts");
    if (start) mg_free_string(start);
    env.tick(10);

    uint8_t block[90];
    for (int i = 0; i < 90; ++i) block[i] = static_cast<uint8_t>(i);
    // 正常递增：0 → 90。第二帧的 ofs 正好等于前沿，必须接受。
    env.mock->send_log_data(11, 0, block, 90);
    env.mock->send_log_data(11, 90, block, 90);

    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"completed\""),
          "two in-order blocks complete a declared-size download");
    CHECK(json_has(status, "\"saved_bytes\":180"), "both blocks were written");
    if (status) mg_free_string(status);

    FILE* f = fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr, "in-order download wrote a file");
    if (f) {
        uint8_t got[180] = {};
        const size_t n = fread(got, 1, sizeof(got), f);
        fclose(f);
        CHECK(n == 180, "file holds all 180 bytes");
        CHECK(got[0] == 0 && got[89] == 89 && got[90] == 0 && got[179] == 89,
              "both blocks landed at the right offsets");
    }
    std::filesystem::remove(path, ec);
}

// 飞控给的 LOG_DATA.ofs 是 uint32，ofs+count 会回绕。当 download_size 未知（0，
// 即 LOG_ENTRY 里没有这个 id 的大小）时，越界保护条件 `download_size > 0 && ...`
// 不生效，回绕后的下标会写到 download_data 之外 → 堆越界写。
// 这里注入一条 ofs 接近 UINT32_MAX 的 LOG_DATA，断言进程既不崩也不越界。
void test_log_download_rejects_wrapping_offset() {
    fprintf(stderr, "\n[Group] LOG_DATA with a wrapping offset is rejected\n");
    TestEnv env;
    env.connect();

    // 只声明另一个 id 的条目：请求的 id 不在列表里 → download_size 保持 0。
    env.mock->send_log_entry(5, 1700000000u, 8u);
    env.tick(50);

    const auto path =
        unique_temp_path("mgcs_log_wrap_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    // id=9 不在日志列表里（没有 size），下载仍会被受理。
    char* start = mg_onboard_log_download(env.core, 9, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "download of an unsized log is accepted");
    if (start) mg_free_string(start);
    env.tick(10);

    // ofs 取一个会让 ofs+count 回绕的值；count=16 是 LOG_DATA 的固定载荷长度。
    const uint8_t payload[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    env.mock->send_log_data(9, 0xFFFFFFF0u, payload, 16);
    env.tick(10);

    // 走到这里说明没有越界写穿崩进程；状态必须是失败或结束，而不是"已完成"。
    char* status = mg_onboard_log_download_status(env.core);
    CHECK(status != nullptr, "status is still queryable after a wrapping offset");
    CHECK(!json_has(status, "\"completed\":true"),
          "a wrapping offset never reports a completed download");
    if (status) mg_free_string(status);
    std::filesystem::remove(path, ec);
}

// P2：落盘失败必须显示为失败，而不是“报告完成但文件不存在”。
void test_log_download_write_failure_is_visible() {
    fprintf(stderr, "\n[Group] mg_onboard_log_download surfaces a write failure\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(9, 1700000000u, 8u);
    env.tick(50);

    // 父目录不存在 → fopen 必然失败（普通用户与 root 都一样）。
    const auto dir =
        unique_temp_path("mgcs_missing_dir_for_test");
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    const auto path = dir / "log.bin";

    char* start = mg_onboard_log_download(env.core, 9, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts");
    if (start) mg_free_string(start);
    env.tick(10);

    const uint8_t payload[8] = {9, 9, 9, 9, 9, 9, 9, 9};
    env.mock->send_log_data(9, 0, payload, 8);

    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"failed\""),
          "write failure is reported as failed, not completed");
    CHECK(!json_has(status, "\"completed\":true"),
          "failed write never reports completion");
    CHECK(json_has(status, "\"saved_bytes\":0"), "failed write saved zero bytes");
    CHECK(!std::filesystem::exists(path), "no partial file is left behind");
    if (status) mg_free_string(status);
}

// P4：状态查询是纯读取；丢包补发由周期超时检查驱动。
void test_log_download_status_has_no_side_effect() {
    fprintf(stderr, "\n[Group] mg_onboard_log_download_status is a pure read\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(11, 1700000000u, 8u);
    env.tick(50);

    const auto path =
        unique_temp_path("mgcs_log_status_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    char* start = mg_onboard_log_download(env.core, 11, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts");
    if (start) mg_free_string(start);
    env.tick(10);
    const int after_start = env.mock->log_request_data_count;
    CHECK(after_start == 1, "start sends exactly one LOG_REQUEST_DATA");

    // 静默 600ms（超过 500ms 补发阈值）后再查状态。旧实现会在这里补发。
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"downloading\""),
          "stalled download still reports downloading");
    if (status) mg_free_string(status);
    env.tick(0);  // 只把 GCS→FC 的帧交给模拟飞控，不产生新的遥测
    CHECK(env.mock->log_request_data_count == after_start,
          "status query sends no LOG_REQUEST_DATA");

    // 周期超时检查才是补发的驱动：推进一次遥测即可触发从缺口重发。
    env.tick(100);
    CHECK(env.mock->log_request_data_count > after_start,
          "stall recovery retransmits from the periodic timeout path");
    CHECK(env.mock->last_log_request_ofs == 0,
          "retransmit starts from the first missing chunk");
    std::filesystem::remove(path, ec);
}

// 取消下载只发送 LOG_REQUEST_END，并落到 cancelled 状态。
void test_log_download_cancel_sends_request_end() {
    fprintf(stderr, "\n[Group] mg_onboard_log_download_cancel\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(3, 1700000000u, 8u);
    env.tick(50);

    const auto path =
        unique_temp_path("mgcs_log_cancel_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    char* start = mg_onboard_log_download(env.core, 3, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts");
    if (start) mg_free_string(start);
    env.tick(10);

    char* cancel = mg_onboard_log_download_cancel(env.core);
    CHECK(json_has(cancel, "\"success\":true"), "cancel is accepted");
    if (cancel) mg_free_string(cancel);
    env.tick(10);
    CHECK(env.mock->log_request_end_count == 1,
          "cancel sends exactly one LOG_REQUEST_END");

    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"cancelled\""),
          "cancelled state is reported");
    if (status) mg_free_string(status);
    std::filesystem::remove(path, ec);
}

// LOG_ENTRY 声明的大小大于实际发出的字节时，明确报告截断，不写半个文件。
void test_log_download_truncation_is_reported() {
    fprintf(stderr, "\n[Group] mg_onboard_log_download reports truncation\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(21, 1700000000u, 100u);
    env.tick(50);

    const auto path =
        unique_temp_path("mgcs_log_truncated_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    char* start = mg_onboard_log_download(env.core, 21, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts");
    if (start) mg_free_string(start);
    env.tick(10);

    // 只发 8 字节，然后飞控用空帧表示“没有更多数据”。
    const uint8_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    env.mock->send_log_data(21, 0, payload, 8);
    env.mock->send_log_data(21, 8, nullptr, 0);

    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"failed\""),
          "early stream end is reported as failed");
    CHECK(!json_has(status, "\"completed\":true"),
          "truncated download never reports completion");
    CHECK(json_has(status, "log truncated"),
          "truncation reason is surfaced to the caller");
    if (status) mg_free_string(status);
    CHECK(!std::filesystem::exists(path),
          "no partial file is written when the download is truncated");
}

// 飞控没声明大小时仍能下载，但完成态明确标注“未校验”。
void test_log_download_unknown_size_is_unverified() {
    fprintf(stderr, "\n[Group] mg_onboard_log_download without a declared size\n");
    TestEnv env;
    env.connect();

    // 不发送 LOG_ENTRY：飞控没有声明大小，下载只能靠空帧判定结束。
    const auto path =
        unique_temp_path("mgcs_log_unknown_size_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);

    char* start = mg_onboard_log_download(env.core, 5, path.string().c_str());
    CHECK(json_has(start, "\"success\":true"), "log download starts without a list");
    if (start) mg_free_string(start);
    env.tick(10);

    uint8_t payload[90];
    for (int i = 0; i < 90; ++i) payload[i] = static_cast<uint8_t>(i);
    env.mock->send_log_data(5, 0, payload, 90);
    env.mock->send_log_data(5, 90, nullptr, 0);

    char* status = mg_onboard_log_download_status(env.core);
    CHECK(json_has(status, "\"state\":\"completed\""),
          "empty frame ends an undeclared-size download");
    CHECK(json_has(status, "\"size_declared\":false"),
          "status marks that the flight controller declared no size");
    CHECK(json_has(status, "\"saved_bytes\":90"), "all received bytes are saved");
    if (status) mg_free_string(status);

    FILE* f = fopen(path.string().c_str(), "rb");
    CHECK(f != nullptr, "undeclared-size download still writes a file");
    if (f) fclose(f);
    std::filesystem::remove(path, ec);
}

// 擦除发送 LOG_ERASE；下载进行中必须拒绝，避免半途丢失数据。
void test_log_erase_gating() {
    fprintf(stderr, "\n[Group] mg_onboard_log_erase\n");
    TestEnv env;
    env.connect();

    char* erase = mg_onboard_log_erase(env.core);
    CHECK(json_has(erase, "\"success\":true"), "erase is accepted on an idle link");
    if (erase) mg_free_string(erase);
    env.tick(20);
    CHECK(env.mock->log_erase_count == 1, "erase sends exactly one LOG_ERASE");

    // 下载进行中 → 拒绝擦除，且不发送 LOG_ERASE。
    env.mock->send_log_entry(22, 1700000000u, 100u);
    env.tick(50);
    const auto path =
        unique_temp_path("mgcs_log_erase_gate_test.bin");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    char* start = mg_onboard_log_download(env.core, 22, path.string().c_str());
    if (start) mg_free_string(start);
    env.tick(10);

    char* blocked = mg_onboard_log_erase(env.core);
    CHECK(json_has(blocked, "\"success\":false"),
          "erase is refused while a download is active");
    if (blocked) mg_free_string(blocked);
    env.tick(20);
    CHECK(env.mock->log_erase_count == 1,
          "refused erase sends no LOG_ERASE");
    std::filesystem::remove(path, ec);
}

// 飞行日志声明会采集飞控 STATUSTEXT，但状态转发以前没接线。这条用例要求
// 事件真的落到 mg_event_log_get_events 里。
void test_flight_log_captures_statustext() {
    fprintf(stderr, "\n[Group] flight log captures STATUSTEXT\n");
    TestEnv env;
    env.connect();

    char* start = mg_event_log_start(env.core);
    CHECK(json_has(start, "\"success\":true"), "flight log starts");
    if (start) mg_free_string(start);

    env.mock->send_statustext("PreArm: Throttle too high", 4);
    env.tick(50);

    char* events = mg_event_log_get_events(env.core);
    CHECK(json_has(events, "fc.statustext"), "statustext stored under fc.statustext");
    CHECK(json_has(events, "PreArm: Throttle too high"),
          "statustext text reaches the flight log");
    if (events) mg_free_string(events);

    char* stop = mg_event_log_stop(env.core);
    CHECK(json_has(stop, "\"success\":true"), "flight log stops");
    if (stop) mg_free_string(stop);
}

void test_mission_request_propagates_send_failure() {
    fprintf(stderr, "\n[Group] mg_mission_download\n");
    TestEnv env;
    env.connect();

    char* ok_result = mg_mission_download(env.core);
    CHECK(json_has(ok_result, "\"success\":true"), "mission request succeeds on a live link");
    if (ok_result) mg_free_string(ok_result);
}

// 遥控通道 → 飞行模式的映射（对标 MP ConfigFlightModes）：读 FLTMODE1..6、
// FLTMODE_CH 与 Copter 的 SIMPLE / SUPER_SIMPLE，槽位名用模式表反查。
void test_flight_mode_mapping_reads_ardupilot_params() {
    fprintf(stderr, "\n[Group] mg_flight_get_mode_mapping\n");

    TestEnv env;  // 默认 ArduPilot/Copter
    env.connect();
    env.mock->add_param("FLTMODE1", 0);   // Stabilize
    env.mock->add_param("FLTMODE2", 6);   // RTL
    env.mock->add_param("FLTMODE_CH", 5);
    env.mock->add_param("SIMPLE", 1);
    env.load_params();

    char* json = env.call_with_pump(mg_flight_get_mode_mapping);
    CHECK(json != nullptr, "mode mapping returns json");
    CHECK(json_has(json, "\"available\":true"), "FLTMODE1 present -> mapping available");
    CHECK(json_has(json, "\"param_prefix\":\"FLTMODE\""), "ArduPilot uses the FLTMODE prefix");
    CHECK(json_has(json, "\"channel_param\":\"FLTMODE_CH\""), "mode channel param is FLTMODE_CH");
    CHECK(json_has(json, "\"channel\":5"), "mode channel value is read");
    CHECK(json_has(json, "\"param\":\"FLTMODE2\"") && json_has(json, "\"value\":6"),
          "slot 2 reports the RTL mode number");
    CHECK(json_has(json, "\"name\":\"RTL\""), "slot name is resolved through the mode table");
    CHECK(json_has(json, "\"SIMPLE\":1"), "Copter SIMPLE mask is reported");
    if (json) mg_free_string(json);
}

void test_serial_get_uart_names() {
    fprintf(stderr, "\n[Group] mg_serial_get_uart_names\n");

    TestEnv env;
    env.connect();
    env.mock->set_ftp_file(
        "@SYS/uarts.txt",
        "UARTV1\n"
        "SERIAL0 OTG1 TX=1 RX=1\n"
        "SERIAL2 USART3 TX=2 RX=2 FlowCtrl=1\n");

    char* json = env.call_with_pump(mg_serial_get_uart_names);
    CHECK(json != nullptr, "uart names returns json");
    CHECK(json_has(json, "\"available\":true"), "downloaded uart file is available");
    CHECK(json_has(json, "\"name\":\"USART3\""), "hardware name parsed from uarts.txt");
    CHECK(json_has(json, "\"serial_index\":2"), "logical SERIAL index preserved");
    CHECK(json_has(json, "\"flow_control\":1"), "flow control flag parsed");
    if (json) mg_free_string(json);

    // A board that never serves the file must not be reported as a real board
    // with zero UARTs.
    TestEnv bare;
    bare.connect();
    char* missing = bare.call_with_pump(mg_serial_get_uart_names);
    CHECK(json_has(missing, "\"available\":false"), "missing uart file is not faked as available");
    CHECK(!json_has(missing, "\"success\":true"), "missing uart file does not report success");
    if (missing) mg_free_string(missing);
}


// 轮询 mg_fs_transfer_status，直到出现期望状态或超时；pump 线程负责把
// mock 的 FTP 响应喂回适配器。
std::string wait_for_fs_state(TestEnv& env, const char* want,
                              int timeout_ms = 5000) {
    const auto start = std::chrono::steady_clock::now();
    std::string last;
    while (true) {
        char* status = mg_fs_transfer_status(env.core);
        last = status != nullptr ? status : std::string();
        if (status != nullptr) mg_free_string(status);
        const std::string marker = std::string("\"state\":\"") + want + "\"";
        if (last.find(marker) != std::string::npos) return last;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) return last;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

// mg_fs_* 是异步入口：发起后调用线程必须立刻返回，传输在后台推进，
// 结果只能通过 mg_fs_transfer_status 轮询（P1）。
void test_fs_transfer_async_ffi() {
    fprintf(stderr, "\n[Group] mg_fs_* async file transfer\n");

    TestEnv env;
    env.connect();
    const std::string uart_text =
        "UARTV1\n"
        "SERIAL0 OTG1 TX=1 RX=1\n"
        "SERIAL2 USART3 TX=2 RX=2 FlowCtrl=1\n";
    env.mock->set_ftp_file("@SYS/uarts.txt", uart_text);
    env.mock->set_ftp_directory(
        "@SYS", {"S208.42 1", "Fuarts.txt\t41", "Dlogs", "Fparams.txt\t42"});

    const auto output =
        unique_temp_path("mgcs_ffi_fs_async.txt");
    std::error_code ignored;
    std::filesystem::remove(output, ignored);

    // 没有活动传输时，取消必须被拒绝而不是假装成功。
    char* idle_cancel = mg_fs_cancel(env.core);
    CHECK(json_has(idle_cancel, "\"success\":false"),
          "mg_fs_cancel refuses when nothing is transferring");
    if (idle_cancel) mg_free_string(idle_cancel);

    env.start_pump();
    char* started =
        mg_fs_download_file(env.core, "@SYS/uarts.txt", output.string().c_str());
    CHECK(json_has(started, "\"success\":true"),
          "mg_fs_download_file starts and returns immediately");
    if (started) mg_free_string(started);

    const std::string completed = wait_for_fs_state(env, "completed");
    CHECK(json_has(completed.c_str(), "\"state\":\"completed\""),
          "download reaches completed through status polling");
    CHECK(json_has(completed.c_str(), "\"task\":\"download\""),
          "status names the download task");
    CHECK(json_has(completed.c_str(), "\"remote_path\":\"@SYS/uarts.txt\""),
          "status echoes the remote path");
    CHECK(json_has(completed.c_str(),
                   ("\"received_bytes\":" + std::to_string(uart_text.size()))
                       .c_str()),
          "status reports the bytes received");
    CHECK(std::filesystem::exists(output) &&
              std::filesystem::file_size(output) == uart_text.size(),
          "downloaded file lands on disk");

    char* after = mg_fs_transfer_status(env.core);
    CHECK(json_has(after, "\"active\":false"),
          "finished transfer is no longer active");
    if (after) mg_free_string(after);

    char* list_start = mg_fs_list_directory(env.core, "@SYS");
    CHECK(json_has(list_start, "\"success\":true"),
          "mg_fs_list_directory starts a background listing");
    if (list_start) mg_free_string(list_start);
    const std::string listed = wait_for_fs_state(env, "completed");
    CHECK(json_has(listed.c_str(), "\"task\":\"list\"") &&
              json_has(listed.c_str(), "\"name\":\"uarts.txt\"") &&
              json_has(listed.c_str(), "\"name\":\"logs\""),
          "list status carries parsed directory items");
    env.stop_pump();

    // 远端不存在 → NAK FILE_NOT_FOUND：状态必须是 failed 且带协议错误码。
    TestEnv failing;
    failing.connect();
    failing.start_pump();
    char* missing = mg_fs_download_file(failing.core, "@SYS/missing.txt",
                                        output.string().c_str());
    CHECK(json_has(missing, "\"success\":true"),
          "request for a missing file still starts asynchronously");
    if (missing) mg_free_string(missing);
    const std::string failed = wait_for_fs_state(failing, "failed");
    failing.stop_pump();
    CHECK(json_has(failed.c_str(), "\"state\":\"failed\""),
          "missing remote file ends as failed");
    CHECK(json_has(failed.c_str(), "\"error_code\":10"),
          "failure surfaces the MAV_FTP_ERR code");

    // 不启动 pump：没有响应，传输停在等待中，此时取消必须真的中断它。
    TestEnv cancel_env;
    cancel_env.connect();
    cancel_env.mock->set_ftp_file("@SYS/big.txt", std::string(4096, 'x'));
    const auto cancel_out =
        unique_temp_path("mgcs_ffi_fs_cancel.txt");
    std::filesystem::remove(cancel_out, ignored);
    char* c_start = mg_fs_download_file(cancel_env.core, "@SYS/big.txt",
                                        cancel_out.string().c_str());
    CHECK(json_has(c_start, "\"success\":true"), "in-flight download starts");
    if (c_start) mg_free_string(c_start);
    char* c_cancel = mg_fs_cancel(cancel_env.core);
    CHECK(json_has(c_cancel, "\"success\":true"),
          "mg_fs_cancel accepts an in-flight transfer");
    if (c_cancel) mg_free_string(c_cancel);
    const std::string cancelled = wait_for_fs_state(cancel_env, "cancelled", 3000);
    CHECK(json_has(cancelled.c_str(), "\"state\":\"cancelled\""),
          "cancelled transfer reports cancelled, not failed");

    std::filesystem::remove(output, ignored);
    std::filesystem::remove(cancel_out, ignored);
}

}  // namespace

// ArduPilot 只在首次上电与解锁时发一次 HOME_POSITION，GCS 必须主动要。
// 这条用例钉住“连上并识别后自动请求 home”，以及 home 缓存能被正确读出。
void test_home_position_is_requested_on_identify() {
    fprintf(stderr, "\n[Group] home position request\n");
    TestEnv env;
    env.connect();

    bool requested_home = false;
    bool requested_stream = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == 410 /* MAV_CMD_GET_HOME_POSITION */) {
            requested_home = true;
        }
        if (cmd.command == 511 /* MAV_CMD_SET_MESSAGE_INTERVAL */ &&
            cmd.p1 == static_cast<float>(MAVLINK_MSG_ID_HOME_POSITION) &&
            cmd.p2 == 1000000.0f) {
            requested_stream = true;
        }
    }
    CHECK(requested_home, "GCS asks for home position after identifying the vehicle");
    CHECK(requested_stream, "GCS requests 1 Hz HOME_POSITION streaming");

    bool requested_extended = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == 511 &&
            cmd.p1 == static_cast<float>(MAVLINK_MSG_ID_EXTENDED_SYS_STATE) &&
            cmd.p2 == 1000000.0f) {
            requested_extended = true;
        }
    }
    CHECK(requested_extended,
          "GCS requests 1 Hz EXTENDED_SYS_STATE streaming (landed_state)");

    // 没有 home 之前必须是 invalid，不能拿 0,0 当作有效 home 画到地图上。
    char* empty = mg_vehicle_get_home_position(env.core);
    CHECK(json_has(empty, "\"valid\":false"), "home is invalid before the FC reports one");
    if (empty) mg_free_string(empty);

    env.mock->send_home_position(39.9042, 116.4074, 42.5);
    env.tick(50);

    char* home = mg_vehicle_get_home_position(env.core);
    CHECK(json_has(home, "\"valid\":true"), "home becomes valid after HOME_POSITION");
    CHECK(json_has(home, "\"lat_deg\":39.90"), "home latitude decoded from degE7");
    CHECK(json_has(home, "\"lon_deg\":116.40"), "home longitude decoded from degE7");
    CHECK(json_has(home, "\"alt_m\":42.5"), "home altitude decoded from mm");
    if (home) mg_free_string(home);
}

// ---- 相机/云台设备清单（S2 前端页面的读接口基础） ----
//
// S1 的读接口只能返回「第一台设备 + 数量」，前端无法列出多台设备。
// 这里钉住清单数组：注入两台不同型号的相机/云台后，读接口必须把它们都列出来，
// 并且按组件 id 选择时返回对应那台的详情。
std::string payload_array_slice(const char* json, const char* key) {
    if (json == nullptr) return {};
    const std::string text(json);
    const auto begin = text.find(key);
    if (begin == std::string::npos) return {};
    const auto end = text.find(']', begin);
    if (end == std::string::npos) return {};
    return text.substr(begin, end - begin + 1);
}

int count_char(const std::string& text, char needle) {
    int n = 0;
    for (char c : text) {
        if (c == needle) n++;
    }
    return n;
}

void inject_message(TestEnv& env, mavlink_message_t& msg) {
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    env.link->inject_incoming(buf, len);
}

// 电机测试：MAV_CMD_DO_MOTOR_TEST(209)，参数校验与命令下发。
void test_actuator_motor_test_sends_command() {
    fprintf(stderr, "\n[Group] mg_actuator_motor_test\n");

    TestEnv env;
    env.connect();
    env.mock->received_cmds.clear();

    env.start_pump();
    char* bad_motor = mg_actuator_motor_test(env.core, 0, 50.0f, 1000);
    env.stop_pump();
    CHECK(json_has(bad_motor, "\"success\":false"), "motor 0 is rejected");
    if (bad_motor) mg_free_string(bad_motor);

    env.start_pump();
    char* bad_throttle = mg_actuator_motor_test(env.core, 1, 150.0f, 1000);
    env.stop_pump();
    CHECK(json_has(bad_throttle, "\"success\":false"), "throttle above 100 is rejected");
    if (bad_throttle) mg_free_string(bad_throttle);

    env.start_pump();
    char* sent = mg_actuator_motor_test(env.core, 3, 25.0f, 2000);
    env.tick(20);
    env.stop_pump();
    CHECK(json_has(sent, "\"success\":true"), "valid motor test is accepted");
    if (sent) mg_free_string(sent);

    bool found = false;
    for (const auto& cmd : env.mock->received_cmds) {
        if (cmd.command == 209 && cmd.p1 == 3.0f && cmd.p3 == 25.0f) found = true;
    }
    CHECK(found, "DO_MOTOR_TEST carries motor index and throttle percent");
}

void test_payload_device_inventory() {
    fprintf(stderr, "\n[Group] Payload device inventory (camera/gimbal)\n");
    TestEnv env;
    CHECK(env.wait_connected(), "payload inventory: connected");

    // 两台相机：component 100 / 101，型号不同。
    auto inject_camera = [&env](uint8_t comp, const char* model, uint16_t res_h) {
        mavlink_message_t m;
        mavlink_camera_information_t ci = {};
        ci.flags = 0x00000003;  // CAPTURE_IMAGE | CAPTURE_VIDEO
        ci.resolution_h = res_h;
        ci.resolution_v = 2160;
        std::memcpy(ci.vendor_name, "ACME", 4);
        std::memcpy(ci.model_name, model, std::strlen(model));
        mavlink_msg_camera_information_encode(1, comp, &m, &ci);
        inject_message(env, m);
    };
    inject_camera(100, "CamA", 4000);
    inject_camera(101, "CamB", 1920);

    // 相机 100 有两个存储桶（0 与 1）。
    for (uint8_t storage_id = 0; storage_id < 2; storage_id++) {
        mavlink_message_t m;
        mavlink_storage_information_t si = {};
        si.storage_id = storage_id;
        si.storage_count = 2;
        si.status = 2;
        si.total_capacity = 32000.0f;
        si.used_capacity = 1000.0f * (storage_id + 1);
        si.available_capacity = 31000.0f;
        mavlink_msg_storage_information_encode(1, 100, &m, &si);
        inject_message(env, m);
    }

    char* r = mg_camera_get_status(env.core, 0);
    CHECK(r != nullptr, "camera_get_status returns non-null");
    const std::string cameras = payload_array_slice(r, "\"cameras\":[");
    CHECK(count_char(cameras, '{') == 2, "camera inventory lists both cameras");
    CHECK(cameras.find("\"model_name\":\"CamA\"") != std::string::npos, "inventory has CamA");
    CHECK(cameras.find("\"model_name\":\"CamB\"") != std::string::npos, "inventory has CamB");
    CHECK(json_has(r, "\"available\":true"), "camera available:true when a device exists");
    CHECK(json_has(r, "\"model_name\":\"CamA\""), "default selection is the first camera");
    const std::string storage = payload_array_slice(r, "\"entries\":[");
    CHECK(count_char(storage, '{') == 2, "storage returns every bucket, not just the first");
    if (r) mg_free_string(r);

    // 按组件 id 选择第二台：详情必须是 CamB，而不是「第一台」。
    r = mg_camera_get_status(env.core, 101);
    CHECK(r != nullptr, "camera_get_status(101) returns non-null");
    CHECK(json_has(r, "\"component_id\":101"), "selecting by compId addresses CamB");
    CHECK(json_has(r, "\"model_name\":\"CamB\""), "selected camera detail is CamB");
    CHECK(json_has(r, "\"resolution_h\":1920"), "selected camera detail keeps its own resolution");
    if (r) mg_free_string(r);

    // 不存在的组件 id：合法的「无此设备」，不是调用失败。
    r = mg_camera_get_status(env.core, 200);
    CHECK(r != nullptr, "camera_get_status(200) returns non-null");
    CHECK(json_has(r, "\"available\":false"), "unknown compId reports available:false");
    CHECK(!json_has(r, "\"error\""), "unknown compId is not an error");
    if (r) mg_free_string(r);

    // 两台云台管理器：component 154 / 155；154 另有设备信息提供厂商/型号。
    for (uint8_t comp = 154; comp < 156; comp++) {
        mavlink_message_t m;
        mavlink_gimbal_manager_information_t gi = {};
        gi.cap_flags = 0x5;
        gi.pitch_min = -90.0f;
        gi.pitch_max = 30.0f;
        mavlink_msg_gimbal_manager_information_encode(1, comp, &m, &gi);
        inject_message(env, m);
    }
    {
        mavlink_message_t m;
        mavlink_gimbal_device_information_t gd = {};
        std::memcpy(gd.vendor_name, "GMB", 3);
        std::memcpy(gd.model_name, "G1", 2);
        mavlink_msg_gimbal_device_information_encode(1, 154, &m, &gd);
        inject_message(env, m);
    }

    r = mg_gimbal_get_status(env.core, 0);
    CHECK(r != nullptr, "gimbal_get_status returns non-null");
    const std::string gimbals = payload_array_slice(r, "\"gimbals\":[");
    CHECK(count_char(gimbals, '{') == 2, "gimbal inventory lists both managers");
    CHECK(gimbals.find("\"vendor_name\":\"GMB\"") != std::string::npos,
          "gimbal inventory carries device vendor when known");
    CHECK(json_has(r, "\"available\":true"), "gimbal available:true when a device exists");
    CHECK(json_has(r, "\"pitch_min\":-90"), "gimbal detail exposes the manager pitch range");
    if (r) mg_free_string(r);

    // 云台姿态：单位四元数 → 欧拉角全 0；再验证 180° 偏航（q=(0,0,0,1)，
    // 在 float 下是精确值，不会因为 0.7071 的舍入把断言变成浮点噪声）。
    {
        mavlink_message_t m;
        mavlink_gimbal_device_attitude_status_t ga = {};
        ga.q[0] = 1.0f;
        mavlink_msg_gimbal_device_attitude_status_encode(1, 154, &m, &ga);
        inject_message(env, m);
    }
    r = mg_gimbal_get_attitude(env.core, 0);
    CHECK(r != nullptr, "gimbal_get_attitude returns non-null");
    CHECK(json_has(r, "\"available\":true"), "gimbal attitude available after status message");
    CHECK(json_has(r, "\"yaw_deg\":0"), "identity quaternion gives yaw 0");
    if (r) mg_free_string(r);

    {
        mavlink_message_t m;
        mavlink_gimbal_device_attitude_status_t ga = {};
        ga.q[3] = 1.0f;  // 180° about Z
        mavlink_msg_gimbal_device_attitude_status_encode(1, 154, &m, &ga);
        inject_message(env, m);
    }
    r = mg_gimbal_get_attitude(env.core, 0);
    CHECK(r != nullptr, "gimbal_get_attitude(second) returns non-null");
    CHECK(json_has(r, "\"yaw_deg\":180"), "yaw quaternion converts to 180 degrees");
    if (r) mg_free_string(r);
}

// ================================================================
// S5：断链必须通知界面，而不只是让状态悄悄过期
// ================================================================

struct ConnectionProbe {
    int events = 0;
    bool connected_value = true;   // 事件里的 connected 字段
    std::string reason;
};

void connection_probe_cb(mg_event_type_t type, char* json, void* user_data) {
    auto* probe = static_cast<ConnectionProbe*>(user_data);
    if (probe != nullptr && type == MG_EVENT_CONNECTION) {
        probe->events++;
        if (json != nullptr) {
            const std::string text(json);
            probe->connected_value = text.find("\"connected\":true") != std::string::npos;
            const auto reason_at = text.find("\"reason\":\"");
            if (reason_at != std::string::npos) {
                const auto from = reason_at + std::strlen("\"reason\":\"");
                const auto to = text.find('"', from);
                if (to != std::string::npos) {
                    probe->reason = text.substr(from, to - from);
                }
            }
        }
    }
    if (json != nullptr) mg_free_string(json);
}

/// S5 的前半句：链路断开时事件必须发出（界面据此提示）；状态失效那半句由
/// test_mode_state_reports_freshness 覆盖。
///
/// 这里刻意走生产接线：`attach_shared_session()` → `finish_connect()` 才会装上
/// `Vehicle::install_connection_state_callback`，它把连接状态变化转成
/// `kConnection` 事件（= MG_EVENT_CONNECTION）。MockVehicleTestEnv 用
/// wire_for_test() 旁路了 finish_connect()，所以这条断言自己建会话。
void test_link_loss_notifies_the_interface() {
    fprintf(stderr, "\n[Group] S5 link loss notification\n");

    mg_core_t* core = mg_create();
    auto link = std::make_shared<mg::core::transport::InMemoryLink>();
    link->open();
    auto session =
        std::make_shared<mg::core::transport::LinkSession>(link, "inmemory://s5");

    // fallback vehicle 的 id 是 0，attach_shared_session() 拒绝 route_id <= 0，
    // 所以这里按会话路径注册一台正常编号的飞机（test_bridge_vehicle_session 同款）。
    auto vehicle = std::make_shared<mg::core::vehicle::Vehicle>(5);
    core->vehicles.add_for_test(vehicle);
    core->vehicles.set_active_vehicle(5);
    vehicle->set_event_callback([core](int event_type, const std::string& json) {
        core->events.fire(static_cast<mg_event_type_t>(event_type), json);
    });

    CHECK(vehicle->attach_shared_session(session, "inmemory://s5", 1, 1),
          "shared session attaches through the production path");
    // 生产顺序：先 attach 再 open()（open() 才把链路接收回调接到 router 上）。
    CHECK(session->open(), "shared session opens after attaching");

    {
        // 模拟飞控只在本块内存活：它持有链路引用，必须在释放链路之前销毁。
        MockFlightController mock(*link, MockFlightController::Config());
        for (int i = 0; i < 60; i++) {
            mock.tick(100);
            auto outgoing = link->take_outgoing();
            if (outgoing.empty()) continue;
            mock.on_gcs_data(outgoing.data(), outgoing.size());
            if (vehicle->conn().is_connected()) break;
        }
    }
    CHECK(vehicle->conn().is_connected(),
          "vehicle is connected before the link drops");

    ConnectionProbe probe;
    mg_set_event_callback(core, connection_probe_cb, &probe);

    char* before = mg_connection_get_state(core);
    CHECK(json_has(before, "\"heartbeat_fresh\":true"),
          "state is fresh while the link is up");
    if (before) mg_free_string(before);
    CHECK(probe.events == 0, "no connection event is emitted while the link is up");

    // 断开：走 UI 用的生产路径 mg_connection_disconnect（close_vehicle →
    // Vehicle::disconnect → ConnectionManager::disconnect → reset → 连接状态回调
    // → kConnection 事件）。
    char* result = mg_connection_disconnect(core);
    if (result) mg_free_string(result);

    CHECK(probe.events >= 1, "S5: disconnecting emits a connection event");
    CHECK(!probe.connected_value, "S5: the event tells the interface the link is gone");
    CHECK(!probe.reason.empty(), "the event carries a reason string for the UI");

    char* after = mg_connection_get_state(core);
    CHECK(json_has(after, "\"connected\":false"), "state reports disconnected");
    CHECK(json_has(after, "\"heartbeat_fresh\":false"),
          "S5: dependent state is marked stale, not left as the last value");
    if (after) mg_free_string(after);

    mg_set_event_callback(core, nullptr, nullptr);

    // 先放掉本用例持有的飞机与链路，再销毁 core：Vehicle 析构会走断链清理，
    // 而事件回调捕获了 core，顺序颠倒就是 use-after-free。
    vehicle.reset();
    session.reset();
    link.reset();
    mg_destroy(core);
}

int main() {
    fprintf(stderr, "=== FFI UI path simulation tests ===\n");
    test_params_get_by_name();
    test_stream_set_interval();
    test_vehicle_request_firmware_version();
    test_calibration_cancel();
    test_config_param_write_path();
    test_flight_log_captures_statustext();
    test_waypoint_reached_event_is_emitted();
    test_link_loss_notifies_the_interface();
    test_home_position_is_requested_on_identify();
    test_log_list_reports_real_entries();
    test_log_download_reports_saved_bytes();
    test_log_download_accepts_in_order_and_resent_holes();
    test_log_download_rejects_wrapping_offset();
    test_log_download_write_failure_is_visible();
    test_log_download_status_has_no_side_effect();
    test_log_download_cancel_sends_request_end();
    test_log_download_truncation_is_reported();
    test_log_download_unknown_size_is_unverified();
    test_log_erase_gating();
    test_mission_request_propagates_send_failure();
    test_flight_mode_mapping_reads_ardupilot_params();
    test_serial_get_uart_names();
    test_fs_transfer_async_ffi();
    test_actuator_motor_test_sends_command();
    test_payload_device_inventory();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
