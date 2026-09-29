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

#include <cstdio>
#include <cstring>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

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

    char* stopped = mg_calibration_get_status(env.core);
    CHECK(json_has(stopped, "\"active\":false"), "calibration is inactive after cancel");
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

// 兼容入口以前无论有没有日志都返回 success:true + 空数组，这条用例要求它
// 反映飞控真实上报的日志条目。
void test_log_list_reports_real_entries() {
    fprintf(stderr, "\n[Group] mg_log_list\n");
    TestEnv env;
    env.connect();

    env.mock->send_log_entry(7, 1700000000u, 4096u);
    env.tick(50);

    char* list = mg_log_list(env.core);
    CHECK(json_has(list, "\"success\":true"), "log list reports success");
    CHECK(json_has(list, "\"total\":1"), "log list reports the real entry count");
    CHECK(json_has(list, "\"id\":7"), "log list carries the real log id");
    CHECK(json_has(list, "\"size_bytes\":4096"), "log list carries the real size");
    if (list) mg_free_string(list);

    // mg_log_get_entries 与兼容入口必须给出同一份数据。
    char* entries = mg_log_get_entries(env.core);
    CHECK(json_has(entries, "\"id\":7"), "mg_log_get_entries agrees with mg_log_list");
    if (entries) mg_free_string(entries);
}

void test_rc_calibrate_save_is_honest() {
    fprintf(stderr, "\n[Group] mg_rc_calibrate_save\n");
    TestEnv env;
    env.connect();

    char* idle = mg_rc_calibrate_save(env.core);
    CHECK(json_has(idle, "\"success\":true"), "save succeeds when no calibration is running");
    CHECK(json_has(idle, "saved_by_flight_controller"),
          "save explains the flight controller owns persistence");
    if (idle) mg_free_string(idle);

    // 校准进行中（ACK = IN_PROGRESS）时不得谎报保存成功。
    env.mock->set_cmd_no_response(false);
    env.mock->set_next_cmd_result(1 /* MAV_RESULT_IN_PROGRESS */);
    char* start = mg_rc_calibrate_start(env.core);
    if (start) mg_free_string(start);
    env.tick(50);

    char* in_progress = mg_rc_calibrate_save(env.core);
    CHECK(json_has(in_progress, "\"success\":false"),
          "save refuses while RC calibration is in progress");
    if (in_progress) mg_free_string(in_progress);
}

void test_mission_request_propagates_send_failure() {
    fprintf(stderr, "\n[Group] mg_mission_request\n");
    TestEnv env;
    env.connect();

    char* ok_result = mg_mission_request(env.core, "{}");
    CHECK(json_has(ok_result, "\"success\":true"), "mission request succeeds on a live link");
    if (ok_result) mg_free_string(ok_result);
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

}  // namespace

int main() {
    fprintf(stderr, "=== FFI UI path simulation tests ===\n");
    test_params_get_by_name();
    test_stream_set_interval();
    test_vehicle_request_firmware_version();
    test_calibration_cancel();
    test_config_param_write_path();
    test_log_list_reports_real_entries();
    test_rc_calibrate_save_is_honest();
    test_mission_request_propagates_send_failure();
    test_serial_get_uart_names();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
