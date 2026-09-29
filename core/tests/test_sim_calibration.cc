/**
 * test_sim_calibration.cc - Calibration tests via MockFlightController + FFI
 */
#include "mock_vehicle_test_env.h"
#include "core/calibration/strategies/compass_calibration.h"
#include "core/calibration/strategies/radio_calibration.h"
#include "core/calibration/calibration_types.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>

using namespace mg::core::calibration;

static int g_pass = 0;
static int g_fail = 0;
constexpr uint16_t kAccelCalVehiclePosCommand = 42429;
constexpr uint32_t kAccelCalVehiclePosSuccess = 16777215;
constexpr uint16_t kStartMagCalCommand = 42424;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

struct TestEnv : MockVehicleTestEnv {
    explicit TestEnv(const MockFlightController::Config& cfg = MockFlightController::Config())
        : MockVehicleTestEnv(
              cfg,
              false,
              [](mg::core::mavlink::MavlinkAdapter& mav, mg_core_t* core) {
                  mav.on_statustext([core](const std::string& text, uint8_t severity) {
                      mg_calibration_process_statustext(core, text.c_str(), severity);
                  });
                  mav.on_command_ack([core](const mg::core::mavlink::CommandAckInfo& ack) {
                      mg_calibration_process_command_ack(core, ack.command, ack.result, ack.progress);
                  });
              }) {}
};

static void test_cal_start_compass() {
    fprintf(stderr, "\n[Group] Cal start compass\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_cal_start(env.core, 2, 0);
    CHECK(result != nullptr, "mg_cal_start returns non-null");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    bool sent_modern_mag_cal_command = false;
    for (const auto& command : env.mock->received_cmds) {
        if (command.command == kStartMagCalCommand) {
            sent_modern_mag_cal_command = true;
            break;
        }
    }
    CHECK(sent_modern_mag_cal_command, "mock received DO_START_MAG_CAL command");
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

    char* result = mg_cal_start(env.core, 0, 0);
    CHECK(result != nullptr, "mg_cal_start returns non-null for sysid 2");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    CHECK(env.mock->cal_start_received, "mock sysid 2 received PREFLIGHT_CALIBRATION command");
}

/// 遥控器校准走 MAV_CMD_PREFLIGHT_CALIBRATION 的 param4=1。
static void test_radio_calibration_ffi_sends_param4() {
    fprintf(stderr, "\n[Group] Radio calibration via FFI\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_cal_start(env.core, 4, 0);
    CHECK(result != nullptr, "mg_cal_start accepts type=4 (radio)");
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
            CalibrationStage::Success,
        "ACK accepted means success");
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
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

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
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_cal_start(env.core, 0, 0);
    CHECK(result != nullptr, "mg_cal_start returns non-null for gyro");
    if (result) mg_free_string(result);

    for (int i = 0; i < 20; i++) env.tick(50);

    char* history = mg_cal_history(env.core, 0);
    std::string json = history ? history : "";
    if (history) mg_free_string(history);

    CHECK(json.find("\"stage\":\"success\"") != std::string::npos,
          "gyro history contains success result");
    CHECK(json.find("\"failure_reason\":\"\"") != std::string::npos,
          "success history has empty failure_reason");
}

static void test_level_calibration_history_has_mode_and_wall_clock_timestamp() {
    fprintf(stderr, "\n[Group] Level calibration audit metadata\n");
    TestEnv env;

    for (int i = 0; i < 15; i++) env.tick(100);

    char* result = mg_calibration_start(env.core, 1, 2);
    CHECK(result != nullptr, "level calibration start returns non-null");
    if (result) mg_free_string(result);
    for (int i = 0; i < 20; i++) env.tick(50);

    char* history = mg_calibration_get_history(env.core, 1);
    std::string json = history ? history : "";
    if (history) mg_free_string(history);

    CHECK(json.find("\"accelerometer_mode\":\"level\"") != std::string::npos,
          "level calibration history keeps its operation type");
    CHECK(json.find("\"timestamp_ms\":1") != std::string::npos,
          "calibration history timestamp uses wall-clock epoch milliseconds");
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
    mg_calibration_process_statustext(env.core, "Compass 1 calibration failed", 6);

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

int main() {
    fprintf(stderr, "=== Calibration Simulation Tests ===\n");
    test_cal_start_compass();
    test_mag_cal_report_completes_calibration();
    test_mag_cal_report_failure_does_not_auto_retry();
    test_mag_cal_progress_reaches_calibration_service();
    test_cal_start_uses_heartbeat_target();
    test_radio_calibration_ffi_sends_param4();
    test_radio_calibration_strategy();
    test_accelerometer_six_face_position_handshake();
    test_success_history_clears_verification_failure();
    test_level_calibration_history_has_mode_and_wall_clock_timestamp();
    test_statustext_progress();
    test_statustext_success();
    test_statustext_failure();
    test_failure_reason_keeps_flight_controller_text();
    test_parse_statustext_edge_cases();
    test_interpret_ack();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
