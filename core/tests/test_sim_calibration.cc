/**
 * test_sim_calibration.cc - Calibration tests via MockFlightController + FFI
 */
#include "mock_vehicle_test_env.h"
#include "core/calibration/strategies/accel_calibration.h"
#include "core/calibration/strategies/baro_calibration.h"
#include "core/calibration/strategies/compass_calibration.h"
#include "core/calibration/strategies/gyro_calibration.h"
#include "core/calibration/strategies/radio_calibration.h"
#include "core/calibration/calibration_types.h"
#include "core/transport/connection_manager.h"
#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <chrono>
#include <memory>
#include <thread>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif
#include "mgcs_test_check.h"

using namespace mg::core::calibration;

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();
constexpr uint16_t kAccelCalVehiclePosCommand = 42429;
constexpr uint32_t kAccelCalVehiclePosSuccess = 16777215;
constexpr uint16_t kStartMagCalCommand = 42424;


struct TestEnv : MockVehicleTestEnv {
    explicit TestEnv(const MockFlightController::Config& cfg = MockFlightController::Config(),
                     bool load_default_params = false)
        : MockVehicleTestEnv(
              cfg,
              load_default_params,
              [](mg::core::mavlink::MavlinkAdapter& mav, mg_core_t* core) {
                  mav.on_statustext([core](const std::string& text, uint8_t severity) {
                      get_active_calibration(core).process_statustext(text, severity);
                  });
                  mav.on_command_ack([core](const mg::core::mavlink::CommandAckInfo& ack) {
                      get_active_calibration(core).process_command_ack(ack.command, ack.result, ack.progress);
                  });
              }) {}

    // Verification reads calibration results back from the FC parameter table,
    // so the cache has to be populated before the calibration finishes.
    void request_params() {
        mg_params_request_all(core);
        for (int i = 0; i < 40; i++) tick(20);
    }
};

static void test_cal_start_compass() {
    fprintf(stderr, "\n[Group] Cal start compass\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_calibration_start(env.core, 2, 0);
    CHECK(result != nullptr, "mg_calibration_start returns non-null");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    bool sent_modern_mag_cal_command = false;
    bool sent_mag_cal_cancel = false;
    bool requested_progress_stream = false;
    bool requested_report_stream = false;
    for (const auto& command : env.mock->received_cmds) {
        if (command.command == kStartMagCalCommand) {
            sent_modern_mag_cal_command = true;
        } else if (command.command == 42426) {
            sent_mag_cal_cancel = true;
        } else if (command.command == 511 && command.p1 == 191.0f &&
                   command.p2 == 100000.0f) {
            requested_progress_stream = true;
        } else if (command.command == 511 && command.p1 == 192.0f &&
                   command.p2 == 1000000.0f) {
            requested_report_stream = true;
        }
    }
    CHECK(sent_modern_mag_cal_command, "mock received DO_START_MAG_CAL command");
    CHECK(sent_mag_cal_cancel, "start flushes stale MAG_CAL state with DO_CANCEL_MAG_CAL");
    CHECK(requested_progress_stream,
          "start requests MAG_CAL_PROGRESS at 10 Hz");
    CHECK(requested_report_stream,
          "start requests MAG_CAL_REPORT at 1 Hz");
}

static void test_calibration_operation_cancel() {
    fprintf(stderr, "\n[Group] Calibration operation cancellation\n");
    TestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    char* start = mg_calibration_start(env.core, 2, 0);
    CHECK(start && strstr(start, "\"success\":true") != nullptr,
          "calibration starts before operation cancellation");
    if (start) mg_free_string(start);

    char* cancel = mg_operations_cancel(env.core, "calibration");
    CHECK(cancel && strstr(cancel, "\"canceled\":true") != nullptr,
          "calibration can be cancelled through operations");
    if (cancel) mg_free_string(cancel);

    // 取消是异步的：请求受理后、飞控确认前，快照必须显示 cancelling。
    char* pending = mg_operations_get_snapshot(env.core);
    CHECK(pending && strstr(pending, "\"state\":\"cancelling\"") != nullptr,
          "calibration reports cancelling until the vehicle confirms");
    CHECK(pending && strstr(pending, "\"cancellable\":false") != nullptr,
          "a calibration being cancelled cannot be cancelled again");
    if (pending) mg_free_string(pending);

    for (int i = 0; i < 10; ++i) env.tick(20);

    bool sent_mag_cal_cancel = false;
    for (const auto& command : env.mock->received_cmds) {
        if (command.command == 42426) sent_mag_cal_cancel = true;
    }
    CHECK(sent_mag_cal_cancel,
          "magnetometer cancellation sends DO_CANCEL_MAG_CAL to the vehicle");

    char* snapshot = mg_operations_get_snapshot(env.core);
    CHECK(snapshot && strstr(snapshot, "\"id\":\"calibration\"") != nullptr,
          "operations snapshot exposes calibration id");
    CHECK(snapshot && strstr(snapshot, "\"state\":\"cancelled\"") != nullptr,
          "calibration operation reports cancelled state");
    if (snapshot) mg_free_string(snapshot);
}

static void test_cancel_waits_for_the_vehicle_ack() {
    fprintf(stderr, "\n[Group] Calibration cancel waits for the ACK\n");
    TestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    char* start = mg_calibration_start(env.core, 2 /*compass*/, 0);
    CHECK(start && strstr(start, "\"success\":true") != nullptr,
          "compass calibration starts for the cancel test");
    if (start) mg_free_string(start);
    for (int i = 0; i < 5; ++i) env.tick(20);

    char* cancel = mg_calibration_cancel(env.core);
    CHECK(cancel && strstr(cancel, "\"success\":true") != nullptr &&
              strstr(cancel, "\"state\":\"cancelling\"") != nullptr,
          "cancel reports the request as pending, not as already stopped");
    if (cancel) mg_free_string(cancel);

    char* pending = mg_calibration_get_status(env.core);
    CHECK(pending && strstr(pending, "\"active\":true") != nullptr &&
              strstr(pending, "\"stage\":\"cancelling\"") != nullptr,
          "S8: before the ACK the calibration is still active in cancelling state");
    if (pending) mg_free_string(pending);

    // 与取消无关的命令 ACK 不能当作停止确认。
    get_active_calibration(env.core).process_command_ack(42424 /*DO_START_MAG_CAL*/, 0, 0);
    char* still = mg_calibration_get_status(env.core);
    CHECK(still && strstr(still, "\"stage\":\"cancelling\"") != nullptr,
          "an unrelated ACK does not end the cancellation");
    if (still) mg_free_string(still);

    // 飞控对 DO_CANCEL_MAG_CAL 的 ACK 才是真正的停止确认。
    get_active_calibration(env.core).process_command_ack(42426 /*DO_CANCEL_MAG_CAL*/, 0, 0);
    char* done = mg_calibration_get_status(env.core);
    CHECK(done && strstr(done, "\"active\":false") != nullptr &&
              strstr(done, "\"stage\":\"cancelled\"") != nullptr,
          "the vehicle's cancel ACK finalizes the cancellation");
    CHECK(done && strstr(done, "Cancelled by user") != nullptr,
          "the finalized result keeps the user-cancel reason");
    if (done) mg_free_string(done);
}

static void test_cancel_watchdog_finishes_when_vehicle_is_silent() {
    fprintf(stderr, "\n[Group] Calibration watchdog: cancel never confirmed\n");

    mg::core::transport::ConnectionManager conn;
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    conn.wire_for_test(std::make_unique<mg::core::transport::InMemoryLink>(), std::move(mav));
    conn.shared_mavlink()->set_send_callback([](const uint8_t*, size_t) { return true; });
    conn.record_heartbeat();

    CalibrationService svc;
    svc.set_connection(&conn);
    svc.set_target(1, 1);
    svc.set_cancel_timeout_ms(1);  // 极短阈值，避免测试真等 10 秒
    CHECK(svc.start(CalibrationType::Magnetometer), "compass calibration starts");
    CHECK(svc.cancel(), "the cancel request is accepted");
    CHECK(svc.is_active() &&
              svc.get_status().stage == CalibrationStage::Cancelling,
          "the cancel request keeps the calibration active until confirmed");

    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    svc.poll();

    const auto status = svc.get_status();
    CHECK(!status.active, "an unconfirmed cancel is still finalized by the watchdog");
    CHECK(status.stage == CalibrationStage::Cancelled,
          "the cancel watchdog lands on Cancelled, not Failed");
    CHECK(status.user_message.find("did not confirm") != std::string::npos,
          "the watchdog explains that the vehicle never confirmed");
    CHECK(status.failure_reason.empty(),
          "an operator cancel is not reported as a failure");
}

static void test_cancel_link_loss_still_lands_on_cancelled() {
    fprintf(stderr, "\n[Group] Calibration watchdog: link lost during cancel\n");

    mg::core::transport::ConnectionManager conn;
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    conn.wire_for_test(std::make_unique<mg::core::transport::InMemoryLink>(), std::move(mav));
    conn.shared_mavlink()->set_send_callback([](const uint8_t*, size_t) { return true; });
    conn.record_heartbeat();

    CalibrationService svc;
    svc.set_connection(&conn);
    svc.set_target(1, 1);
    CHECK(svc.start(CalibrationType::Magnetometer), "compass calibration starts");
    CHECK(svc.cancel(), "cancel request accepted before the link drops");

    conn.disconnect();
    svc.poll();

    const auto status = svc.get_status();
    CHECK(!status.active && status.stage == CalibrationStage::Cancelled,
          "link loss during cancel ends in Cancelled, not Failed");
    CHECK(status.user_message.find("link lost") != std::string::npos,
          "link loss during cancel records why it stopped waiting");
}

static void test_mag_cal_report_completes_calibration() {
    fprintf(stderr, "\n[Group] MAG_CAL_REPORT completion\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);
    char* start = mg_calibration_start(env.core, 2, 0);
    CHECK(start != nullptr, "magnetometer start returns non-null");
    if (start) mg_free_string(start);
    for (int i = 0; i < 20; i++) env.tick(50);

    mavlink_message_t report{};
    mavlink_msg_mag_cal_report_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &report,
        0, 1, MAG_CAL_SUCCESS, 1,
        8.0f, 20.0f, -15.0f, 10.0f,
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0, 0, 1.0f);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(bytes, &report);
    env.link->inject_incoming(bytes, length);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);
    CHECK(json.find("\"active\":false") != std::string::npos,
          "MAG_CAL_REPORT success finishes calibration");
    CHECK(json.find("\"stage\":\"success\"") != std::string::npos,
          "MAG_CAL_REPORT success sets success stage");
}

static void test_mag_cal_report_failure_does_not_auto_retry() {
    fprintf(stderr, "\n[Group] MAG_CAL_REPORT failure\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);
    char* start = mg_calibration_start(env.core, 2, 0);
    CHECK(start != nullptr, "magnetometer start returns non-null before failure");
    if (start) mg_free_string(start);
    for (int i = 0; i < 20; i++) env.tick(50);

    mavlink_message_t report{};
    mavlink_msg_mag_cal_report_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &report,
        0, 1, MAG_CAL_FAILED_RESIDUALS_HIGH, 0,
        80.0f, 0.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0, 0, 1.0f);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(bytes, &report);
    env.link->inject_incoming(bytes, length);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);
    CHECK(json.find("\"active\":false") != std::string::npos,
          "MAG_CAL_REPORT failure ends calibration");
    CHECK(json.find("\"stage\":\"failed\"") != std::string::npos,
          "MAG_CAL_REPORT failure sets failed stage");
    CHECK(json.find("\"retry_count\":0") != std::string::npos,
          "magnetometer failure does not retry automatically");
}

static void test_mag_cal_progress_reaches_calibration_service() {
    fprintf(stderr, "\n[Group] MAG_CAL_PROGRESS routing\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);
    char* start = mg_calibration_start(env.core, 2, 0);
    CHECK(start != nullptr, "magnetometer start returns non-null before progress");
    if (start) mg_free_string(start);
    for (int i = 0; i < 20; i++) env.tick(50);

    uint8_t completion_mask[10] = {};
    mavlink_message_t progress{};
    mavlink_msg_mag_cal_progress_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &progress,
        0, 1, MAG_CAL_RUNNING_STEP_ONE, 1, 63, completion_mask,
        0.0f, 0.0f, 1.0f);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(bytes, &progress);
    env.link->inject_incoming(bytes, length);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);
    CHECK(json.find("\"progress\":63") != std::string::npos,
          "MAG_CAL_PROGRESS reaches the calibration service");
    CHECK(json.find("\"compass_progress\":[{\"compass_id\":0,\"completion_pct\":63}]")
              != std::string::npos,
          "per-compass progress is reported for the UI");

    // 第二个罗盘的进度必须单独记一条，而不是覆盖第一个。
    mavlink_message_t progress2{};
    mavlink_msg_mag_cal_progress_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &progress2,
        1, 1, MAG_CAL_RUNNING_STEP_ONE, 1, 12, completion_mask,
        0.0f, 0.0f, 1.0f);
    const uint16_t length2 = mavlink_msg_to_send_buffer(bytes, &progress2);
    env.link->inject_incoming(bytes, length2);

    char* status2 = mg_calibration_get_status(env.core);
    std::string json2 = status2 ? status2 : "";
    if (status2) mg_free_string(status2);
    CHECK(
        json2.find("\"compass_id\":0,\"completion_pct\":63") !=
                std::string::npos &&
            json2.find("\"compass_id\":1,\"completion_pct\":12") !=
                std::string::npos,
        "each compass keeps its own progress entry");
    CHECK(json2.find("\"progress\":63") != std::string::npos,
          "aggregate progress stays at the max across compasses");
}

static void test_cal_start_uses_heartbeat_target() {
    fprintf(stderr, "\n[Group] Cal start targets heartbeat sysid\n");
    MockFlightController::Config cfg;
    cfg.system_id = 2;
    cfg.component_id = 1;
    TestEnv env(cfg);

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_calibration_start(env.core, 0, 0);
    CHECK(result != nullptr, "mg_calibration_start returns non-null for sysid 2");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    CHECK(env.mock->cal_start_received, "mock sysid 2 received PREFLIGHT_CALIBRATION command");
}

/// 遥控器校准走 MAV_CMD_PREFLIGHT_CALIBRATION 的 param4=1。
static void test_radio_calibration_ffi_sends_param4() {
    fprintf(stderr, "\n[Group] Radio calibration via FFI\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_calibration_start(env.core, 4, 0);
    CHECK(result != nullptr, "mg_calibration_start accepts type=4 (radio)");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    bool sent_radio_cal = false;
    for (const auto& command : env.mock->received_cmds) {
        if (command.command == 241 && command.p4 == 1.0f) {
            sent_radio_cal = true;
            break;
        }
    }
    CHECK(sent_radio_cal, "MAV_CMD_PREFLIGHT_CALIBRATION param4=1 sent");
}

/// 策略层的纯逻辑：类型名、指令号、ACK 与 STATUSTEXT 解析。
static void test_radio_calibration_strategy() {
    fprintf(stderr, "\n[Group] Radio calibration strategy\n");
    RadioCalibration strategy;

    CHECK(strategy.type() == CalibrationType::Radio, "type is Radio");
    CHECK(std::strcmp(strategy.type_name(), "radio") == 0, "type name is radio");
    CHECK(strategy.mav_command() == 241, "mav_command is 241");
    CHECK(
        strategy.interpret_ack(0, 0, CalibrationStage::Calibrating) ==
            CalibrationStage::Calibrating,
        "ACK accepted means calibration started");
    CHECK(
        strategy.interpret_ack(5, 0, CalibrationStage::Calibrating) ==
            CalibrationStage::Calibrating,
        "ACK in-progress keeps calibrating");
    CHECK(
        strategy.interpret_ack(4, 0, CalibrationStage::Calibrating) ==
            CalibrationStage::Failed,
        "ACK failed means failed");
    CHECK(
        strategy
                .parse_statustext(
                    "RC calibration complete", 6, CalibrationStage::Calibrating)
                .new_stage == CalibrationStage::Success,
        "statustext complete means success");
    CHECK(
        strategy
                .parse_statustext(
                    "Radio calibration failed", 6, CalibrationStage::Calibrating)
                .new_stage == CalibrationStage::Failed,
        "statustext failed means failed");
    CHECK(
        !strategy
             .parse_statustext(
                 "EKF variance", 6, CalibrationStage::Calibrating)
             .is_calibration_text,
        "unrelated statustext is ignored");
}

static void test_accelerometer_six_face_position_handshake() {
    fprintf(stderr, "\n[Group] Accelerometer six-face handshake\n");
    TestEnv env(MockFlightController::Config(), /*load_default_params=*/true);

    for (int i = 0; i < 15; i++) env.tick(100);
    env.request_params();

    char* start = mg_calibration_start(env.core, 1, 1);
    CHECK(start != nullptr, "six-face start returns non-null");
    if (start) mg_free_string(start);
    for (int i = 0; i < 20; i++) env.tick(50);

    char* before_request = mg_calibration_get_status(env.core);
    std::string before_json = before_request ? before_request : "";
    if (before_request) mg_free_string(before_request);
    CHECK(before_json.find("\"active\":true") != std::string::npos,
          "start ACK keeps six-face calibration active");
    CHECK(before_json.find("\"stage\":\"success\"") == std::string::npos,
          "start ACK is not treated as calibration success");

    mavlink_message_t request{};
    mavlink_msg_command_long_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &request,
        255, 0, kAccelCalVehiclePosCommand, 0,
        1.0f, 0, 0, 0, 0, 0, 0);
    uint8_t bytes[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(bytes, &request);
    env.link->inject_incoming(bytes, length);

    char* requested = mg_calibration_get_status(env.core);
    std::string requested_json = requested ? requested : "";
    if (requested) mg_free_string(requested);
    CHECK(requested_json.find("\"stage\":\"waiting_orientation\"") != std::string::npos,
          "vehicle position request advances to waiting orientation");
    CHECK(requested_json.find("\"current_step\":1") != std::string::npos,
          "vehicle position request selects the requested face");

    char* confirm = mg_calibration_accel_confirm(env.core, 1);
    CHECK(confirm != nullptr, "position confirmation returns non-null");
    if (confirm) mg_free_string(confirm);
    for (int i = 0; i < 10; i++) env.tick(50);

    bool sent_requested_position = false;
    for (const auto& command : env.mock->received_cmds) {
        if (command.command == kAccelCalVehiclePosCommand &&
            command.p1 == 1.0f) {
            sent_requested_position = true;
        }
    }
    CHECK(sent_requested_position,
          "ground station confirms the exact position requested by the vehicle");

    mavlink_message_t completed{};
    mavlink_msg_command_long_pack(
        1, MAV_COMP_ID_AUTOPILOT1, &completed,
        255, 0, kAccelCalVehiclePosCommand, 0,
        static_cast<float>(kAccelCalVehiclePosSuccess), 0, 0, 0, 0, 0, 0);
    const uint16_t completed_length = mavlink_msg_to_send_buffer(bytes, &completed);
    env.link->inject_incoming(bytes, completed_length);

    char* completed_status = mg_calibration_get_status(env.core);
    std::string completed_json = completed_status ? completed_status : "";
    if (completed_status) mg_free_string(completed_status);
    CHECK(completed_json.find("\"active\":false") != std::string::npos,
          "flight controller success notification finishes six-face calibration");
    CHECK(completed_json.find("\"stage\":\"success\"") != std::string::npos,
          "flight controller success notification sets calibration success");
}

static void test_success_history_clears_verification_failure() {
    fprintf(stderr, "\n[Group] Success history clears verification failure\n");
    TestEnv env(MockFlightController::Config(), /*load_default_params=*/true);

    for (int i = 0; i < 15; i++) env.tick(100);
    env.request_params();

    char* result = mg_calibration_start(env.core, 0, 0);
    CHECK(result != nullptr, "mg_calibration_start returns non-null for gyro");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);

    CHECK(json.find("\"stage\":\"success\"") != std::string::npos,
          "gyro history contains success result");
    CHECK(json.find("\"failure_reason\":\"\"") != std::string::npos,
          "success history has empty failure_reason");
}

// Regression: before the fix, verification only failed on Poor quality, so a
// failed parameter readback (empty param cache) still reported success.
static void test_verification_readback_failure_is_reported_failed() {
    fprintf(stderr, "\n[Group] Verification readback failure\n");
    TestEnv env;  // no params loaded and none requested -> empty param cache

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_calibration_start(env.core, 0, 0);
    CHECK(result != nullptr, "gyro start returns non-null without params");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);

    CHECK(json.find("\"active\":false") != std::string::npos,
          "readback failure ends calibration");
    CHECK(json.find("\"stage\":\"failed\"") != std::string::npos,
          "parameter readback failure is not reported as success");

    char* status_json = mg_calibration_get_status(env.core);
    std::string history_json = status_json ? status_json : "";
    if (status_json) mg_free_string(status_json);
    CHECK(history_json.find("\"failure_reason\":\"\"") == std::string::npos,
          "failed calibration history carries a failure reason");
}

// 回归：历史落盘现在推迟到 fire_event_safe 的锁外路径，必须验证真的写进了磁盘，
// 而不是只改了内存里的标记。
static void test_history_is_flushed_to_disk_outside_the_lock() {
    fprintf(stderr, "\n[Group] Calibration history flush\n");
    TestEnv env;
    for (int i = 0; i < 15; i++) env.tick(100);

    // 路径必须与核心在 MGCORE_TEST_BUILD 下使用的 history_storage_path() 一致
    // （core/src/calibration/calibration_service.cc 的测试构建分支），测试读的是
    // 核心实际写入的那份文件。每进程唯一路径避免并发测试互相覆盖。
    const auto pid_str = std::to_string(
#if defined(_WIN32)
        ::_getpid()
#else
        ::getpid()
#endif
    );
    const auto path = std::filesystem::temp_directory_path() /
                      ("M-GCS-test-" + pid_str) / "calibration-history.v1";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    char* result = mg_calibration_start(env.core, 0, 0);
    CHECK(result != nullptr, "gyro start returns non-null for flush test");
    if (result) mg_free_string(result);
    for (int i = 0; i < 20; i++) env.tick(50);

    std::ifstream input(path);
    CHECK(input.is_open(), "calibration history file is written to disk");
    const std::string content((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    CHECK(content.find("MGCAL1 0 ") != std::string::npos,
          "gyro record persisted with MGCAL1 format");
    CHECK(content.find("Failed to read calibration parameters") != std::string::npos,
          "failure reason persisted verbatim");

    // MGCAL1 <type> <stage> <quality> <timestamp_ms> ...：时间戳是审计要素，
    // 必须是 wall-clock epoch 毫秒（13 位），不能退化成 steady_clock 的相对值。
    {
        const auto at = content.find("MGCAL1 ");
        CHECK(at != std::string::npos, "MGCAL1 line is present");
        std::istringstream line(content.substr(at));
        std::string version;
        int type = 0, stage = 0, quality = 0;
        long long stamp = 0;
        line >> version >> type >> stage >> quality >> stamp;
        CHECK(stamp > 1000000000000LL,
              "persisted calibration timestamp is wall-clock epoch milliseconds");
        const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        CHECK(stamp <= now && now - stamp < 60000,
              "persisted timestamp is close to the current wall-clock time");
    }
}

static void test_statustext_progress() {
    fprintf(stderr, "\n[Group] STATUSTEXT progress\n");
    CompassCalibration cal;

    {
        StatusParseResult r = cal.parse_statustext("compass: 50%", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "'compass: 50%' is calibration text");
        CHECK(r.progress == 50, "progress parsed as 50");
        CHECK(r.new_stage == CalibrationStage::Calibrating, "stage stays Calibrating");
    }

    {
        StatusParseResult r = cal.parse_statustext("Mag (0): 75%", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "'Mag (0): 75%' is calibration text");
        CHECK(r.progress == 75, "progress parsed as 75");
    }
}

static void test_statustext_success() {
    fprintf(stderr, "\n[Group] STATUSTEXT success\n");
    CompassCalibration cal;

    {
        StatusParseResult r = cal.parse_statustext("Compass calibrated", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "'Compass calibrated' is calibration text");
        CHECK(r.stage_changed, "stage changed");
        CHECK(r.new_stage == CalibrationStage::Success, "stage -> Success");
        CHECK(r.progress == 100, "progress = 100 on success");
    }

    {
        StatusParseResult r = cal.parse_statustext("Calibration successful", 6, CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Success, "'Calibration successful' -> Success");
    }
}

static void test_statustext_failure() {
    fprintf(stderr, "\n[Group] STATUSTEXT failure\n");
    CompassCalibration cal;

    {
        StatusParseResult r = cal.parse_statustext("compass calibration failed", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "'compass calibration failed' is calibration text");
        CHECK(r.stage_changed, "stage changed on failure");
        CHECK(r.new_stage == CalibrationStage::Failed, "stage -> Failed");
    }

    {
        StatusParseResult r = cal.parse_statustext("Compass calibration Failed", 6, CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Failed, "'Failed' detected");
    }
}

/// 飞控报失败时，失败原因必须是飞控原话，不能被通用文案顶掉。
static void test_failure_reason_keeps_flight_controller_text() {
    fprintf(stderr, "\n[Group] failure reason keeps FC text\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);
    char* start = mg_calibration_start(env.core, 2, 0);
    if (start) mg_free_string(start);
    for (int i = 0; i < 20; i++) env.tick(50);

    // 注入一条飞控的失败文本。
    get_active_calibration(env.core).process_statustext("Compass 1 calibration failed", 6);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);
    CHECK(json.find("Compass 1 calibration failed") != std::string::npos,
          "failure reason keeps the flight controller's own text");
    CHECK(json.find("Move away from magnetic interference") == std::string::npos,
          "generic guidance does not overwrite the real reason");
}

static void test_parse_statustext_edge_cases() {
    fprintf(stderr, "\n[Group] parse_statustext edge cases\n");
    CompassCalibration cal;

    {
        StatusParseResult r = cal.parse_statustext("Compass 50% complete", 6, CalibrationStage::Calibrating);
        CHECK(r.progress == 50, "'Compass 50% complete' -> progress=50");
    }

    {
        StatusParseResult r = cal.parse_statustext("Compass calibrated", 6, CalibrationStage::Calibrating);
        CHECK(r.new_stage != CalibrationStage::Failed, "'Compass calibrated' is NOT failure");
        CHECK(r.new_stage == CalibrationStage::Success, "'Compass calibrated' IS success");
    }

    {
        StatusParseResult r = cal.parse_statustext("compass calibration failed", 6, CalibrationStage::Calibrating);
        CHECK(r.new_stage != CalibrationStage::Success, "'compass calibration failed' is NOT success");
        CHECK(r.new_stage == CalibrationStage::Failed, "'compass calibration failed' IS failure");
    }

    {
        StatusParseResult r = cal.parse_statustext(
            "Compass calibrated but failed due to timeout", 6, CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Failed,
              "'calibrated but failed' -> Failed (not success)");
    }

    {
        StatusParseResult r = cal.parse_statustext("EKF2 IMU0 is using GPS", 6, CalibrationStage::Calibrating);
        CHECK(!r.is_calibration_text, "unrelated text is not calibration text");
    }

    {
        StatusParseResult r = cal.parse_statustext(
            "Please rotate the vehicle around all axes", 6, CalibrationStage::WaitingOrientation);
        CHECK(r.is_calibration_text, "rotation prompt is calibration text");
    }
}

static void test_interpret_ack() {
    fprintf(stderr, "\n[Group] ACK interpretation\n");
    CompassCalibration cal;

    CHECK(cal.interpret_ack(0, 0, CalibrationStage::Calibrating) == CalibrationStage::Calibrating,
          "compass ACK accepted only confirms start; completion comes from progress/status text");
    CHECK(cal.interpret_ack(5, 50, CalibrationStage::WaitingForStart) == CalibrationStage::Calibrating,
          "ACK in_progress -> Calibrating");
    CHECK(cal.interpret_ack(5, 75, CalibrationStage::Calibrating) == CalibrationStage::Calibrating,
          "ACK in_progress while calibrating -> stays Calibrating");
    CHECK(cal.interpret_ack(4, 0, CalibrationStage::Calibrating) == CalibrationStage::Failed,
          "ACK failed -> Failed");
    CHECK(cal.interpret_ack(1, 0, CalibrationStage::Calibrating) == CalibrationStage::Failed,
          "ACK rejected -> Failed");
}


// ================================================================
// 策略层结果语义：ACK 只确认受理，STATUSTEXT 必须整句匹配完成短语
// ================================================================

/// 旧实现各写各的文本判定：gyro/baro/accel 用裸子串（飞控报的
/// "Gyro calibration unsuccessful" / "Barometer calibration incomplete" 都会命中
/// 成功分支），compass 先判成功再判失败且失败词表更窄，radio 既用裸子串话题词又最宽。
/// 现在五种策略共用 statustext_match.h 的两条谓词。这里把语义钉住：
/// 失败词必须先判（"unsuccessful" 含 "successful"），完成必须是整句。
static void test_calibration_strategy_result_semantics() {
    fprintf(stderr, "\n[Group] Calibration strategy result semantics\n");

    {
        GyroCalibration gyro;
        CHECK(gyro.interpret_ack(0, 0, CalibrationStage::Calibrating) ==
                  CalibrationStage::Calibrating,
              "gyro ACK accepted only confirms the command was accepted");
        StatusParseResult r = gyro.parse_statustext(
            "Gyro calibration unsuccessful", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "gyro failure text is calibration text");
        CHECK(r.new_stage == CalibrationStage::Failed,
              "gyro 'calibration unsuccessful' is not read as success");
        r = gyro.parse_statustext("Gyro calibration successful", 6,
                                  CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Success,
              "gyro completion text still means success");
    }

    {
        BaroCalibration baro;
        CHECK(baro.interpret_ack(0, 0, CalibrationStage::Calibrating) ==
                  CalibrationStage::Calibrating,
              "baro ACK accepted only confirms the command was accepted");
        StatusParseResult r = baro.parse_statustext(
            "Barometer calibration incomplete", 6, CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Failed,
              "baro 'calibration incomplete' is not read as success");
        r = baro.parse_statustext("Barometer 1 calibration complete", 6,
                                  CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Success,
              "ArduPilot baro completion text still means success");
    }

    {
        AccelCalibration accel;
        CHECK(accel.interpret_ack(0, 0, CalibrationStage::Calibrating) ==
                  CalibrationStage::Calibrating,
              "accel ACK accepted only confirms the command was accepted");
        StatusParseResult r = accel.parse_statustext(
            "Accelerometer calibration unsuccessful", 6,
            CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Failed,
              "accel 'calibration unsuccessful' is not read as success");
        r = accel.parse_statustext("Accelerometer calibration successful", 6,
                                   CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Success,
              "accel completion text still means success");
        // 六面校准的姿态提示不能被当成失败（"Place vehicle ... and press any key."）。
        r = accel.parse_statustext("Place vehicle level and press any key.", 6,
                                   CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::WaitingOrientation,
              "accel orientation prompt still moves to waiting orientation");
    }

    {
        // 罗盘：失败必须先判。"Compass calibration unsuccessful" 含 "successful"，
        // 旧的"先成功后失败 + 窄失败词表"两个分支都不命中，校准会一直挂着。
        CompassCalibration compass;
        CHECK(compass.interpret_ack(0, 0, CalibrationStage::Calibrating) ==
                  CalibrationStage::Calibrating,
              "compass ACK accepted only confirms the command was accepted");
        StatusParseResult r = compass.parse_statustext(
            "Compass calibration unsuccessful", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "compass 'unsuccessful' is calibration text");
        CHECK(r.new_stage == CalibrationStage::Failed,
              "compass 'calibration unsuccessful' is not read as success");
        r = compass.parse_statustext("Compass calibration incomplete", 6,
                                     CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Failed,
              "compass 'calibration incomplete' is not read as success");
        r = compass.parse_statustext("Compass calibrated", 6,
                                     CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Success,
              "compass 'Compass calibrated' still means success");
    }

    {
        // 遥控：旧的裸子串（"success"/"complete"）会把飞控报的失败读成成功。
        RadioCalibration radio;
        CHECK(radio.interpret_ack(0, 0, CalibrationStage::Calibrating) ==
                  CalibrationStage::Calibrating,
              "radio ACK accepted only confirms the command was accepted");
        StatusParseResult r = radio.parse_statustext(
            "RC calibration unsuccessful", 6, CalibrationStage::Calibrating);
        CHECK(r.is_calibration_text, "radio 'unsuccessful' is calibration text");
        CHECK(r.new_stage == CalibrationStage::Failed,
              "radio 'calibration unsuccessful' is not read as success");
        r = radio.parse_statustext("Radio calibration incomplete", 6,
                                   CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Failed,
              "radio 'calibration incomplete' is not read as success");
        r = radio.parse_statustext("RC calibration complete", 6,
                                   CalibrationStage::Calibrating);
        CHECK(r.new_stage == CalibrationStage::Success,
              "radio completion text still means success");
    }
}

/// 端到端：飞控用 STATUSTEXT 报失败时，即使参数缓存里能读回旧的陀螺偏移
/// （旧实现会据此判成功），也必须落 Failed。
static void test_calibration_failure_text_beats_parameter_readback() {
    fprintf(stderr, "\n[Group] Failure text beats a readable parameter cache\n");
    TestEnv env(MockFlightController::Config(), /*load_default_params=*/true);
    for (int i = 0; i < 15; i++) env.tick(100);
    env.request_params();

    // 飞控不回 ACK：结果完全由 STATUSTEXT 决定。ArduPilot 的陀螺校准 ACK 是
    // 同步完成信号（服务层另行补偿），这里单独验证文本路径。
    env.mock->set_cmd_no_response(true);

    char* start = mg_calibration_start(env.core, 0 /*gyro*/, 0);
    CHECK(start && strstr(start, "\"success\":true") != nullptr,
          "gyro calibration starts for the failure-text test");
    if (start) mg_free_string(start);
    for (int i = 0; i < 10; i++) env.tick(50);

    env.mock->send_statustext("Gyro calibration unsuccessful", 6);
    for (int i = 0; i < 10; i++) env.tick(50);

    char* status = mg_calibration_get_status(env.core);
    std::string json = status ? status : "";
    if (status) mg_free_string(status);
    // 失败文本走失败路径（陀螺会按重试策略再发一次），绝不能落成功。
    CHECK(json.find("\"stage\":\"success\"") == std::string::npos,
          "the failure text is not turned into a success");
    CHECK(json.find("\"failure_reason\":\"Gyroscope calibration failed\"") !=
              std::string::npos,
          "the flight controller's failure text is kept as the reason");
}



static void test_non_magnetometer_cancel_reaches_vehicle() {
    fprintf(stderr, "\n[Group] Cancel reaches the vehicle for every type\n");
    TestEnv env;
    for (int i = 0; i < 15; ++i) env.tick(100);

    // 让飞控不回应，校准才会停在"进行中"，否则陀螺 ACCEPTED 直接算成功。
    env.mock->set_cmd_no_response(true);

    char* start = mg_calibration_start(env.core, 0 /*gyro*/, 0);
    CHECK(start && strstr(start, "\"success\":true") != nullptr, "gyro calibration starts");
    if (start) mg_free_string(start);

    env.mock->received_cmds.clear();

    char* cancel = mg_calibration_cancel(env.core);
    CHECK(cancel && strstr(cancel, "\"success\":true") != nullptr,
          "gyro cancellation reports success");
    if (cancel) mg_free_string(cancel);

    for (int i = 0; i < 10; ++i) env.tick(20);

    bool sent_cancel = false;
    for (const auto& command : env.mock->received_cmds) {
        if (command.command == 241 && command.p1 == 0.0f && command.p2 == 0.0f &&
            command.p3 == 0.0f && command.p4 == 0.0f && command.x == 0 &&
            command.y == 0 && command.z == 0) {
            sent_cancel = true;
        }
    }
    CHECK(sent_cancel,
          "gyro cancellation sends an all-zero PREFLIGHT_CALIBRATION to the vehicle");
}

static void test_calibration_fails_when_link_is_lost() {
    fprintf(stderr, "\n[Group] Calibration watchdog: link loss\n");

    mg::core::transport::ConnectionManager conn;
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    conn.wire_for_test(std::make_unique<mg::core::transport::InMemoryLink>(), std::move(mav));
    // 本组只验证看门狗，不关心链路对端：wire_for_test 装好之后再把发送置为成功。
    conn.shared_mavlink()->set_send_callback([](const uint8_t*, size_t) { return true; });
    conn.record_heartbeat();  // WaitingHeartbeat -> Connected
    CHECK(conn.is_connected(), "test link starts connected");

    CalibrationService svc;
    svc.set_connection(&conn);
    svc.set_target(1, 1);
    CHECK(svc.start(CalibrationType::Gyroscope), "calibration starts on a connected link");
    CHECK(svc.is_active(), "calibration is active before the link drops");

    conn.disconnect();
    svc.poll();

    const auto status = svc.get_status();
    CHECK(!status.active, "link loss clears the active flag");
    CHECK(status.stage == CalibrationStage::Failed, "link loss moves the stage to Failed");
    CHECK(status.failure_reason.find("link lost") != std::string::npos,
          "link loss reports the link as the failure reason");
}

static void test_calibration_stall_timeout_fails() {
    fprintf(stderr, "\n[Group] Calibration watchdog: sampling stall\n");

    mg::core::transport::ConnectionManager conn;
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    conn.wire_for_test(std::make_unique<mg::core::transport::InMemoryLink>(), std::move(mav));
    // 同上：只验证看门狗，发送直接置为成功。
    conn.shared_mavlink()->set_send_callback([](const uint8_t*, size_t) { return true; });
    conn.record_heartbeat();

    CalibrationService svc;
    svc.set_connection(&conn);
    svc.set_target(1, 1);
    svc.set_stall_timeout_ms(1);  // 用极短阈值，避免测试真的等 30 秒
    CHECK(svc.start(CalibrationType::Gyroscope), "calibration starts before the stall test");

    // 每次 poll 都会判定停滞并触发一次重试，重试额度用完后应稳定停在 Failed。
    for (int i = 0; i < 20 && svc.get_status().active; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        svc.poll();
    }

    const auto status = svc.get_status();
    CHECK(!status.active, "sampling stall stops the calibration");
    CHECK(status.stage == CalibrationStage::Failed, "sampling stall ends in Failed");
    CHECK(status.failure_reason.find("timed out") != std::string::npos,
          "sampling stall reports a timeout reason");
}



int main() {
    fprintf(stderr, "=== Calibration Simulation Tests ===\n");
    test_cal_start_compass();
    test_calibration_operation_cancel();
    test_cancel_waits_for_the_vehicle_ack();
    test_cancel_watchdog_finishes_when_vehicle_is_silent();
    test_cancel_link_loss_still_lands_on_cancelled();
    test_mag_cal_report_completes_calibration();
    test_mag_cal_report_failure_does_not_auto_retry();
    test_mag_cal_progress_reaches_calibration_service();
    test_cal_start_uses_heartbeat_target();
    test_radio_calibration_ffi_sends_param4();
    test_radio_calibration_strategy();
    test_accelerometer_six_face_position_handshake();
    test_success_history_clears_verification_failure();
    test_verification_readback_failure_is_reported_failed();
    test_history_is_flushed_to_disk_outside_the_lock();
    test_statustext_progress();
    test_statustext_success();
    test_statustext_failure();
    test_failure_reason_keeps_flight_controller_text();
    test_parse_statustext_edge_cases();
    test_interpret_ack();
    test_calibration_strategy_result_semantics();
    test_calibration_failure_text_beats_parameter_readback();
    test_non_magnetometer_cancel_reaches_vehicle();
    test_calibration_fails_when_link_is_lost();
    test_calibration_stall_timeout_fails();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
