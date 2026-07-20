/**
 * test_sim_calibration.cc - Calibration tests via MockFlightController + FFI
 */
#include "mock_vehicle_test_env.h"
#include "core/calibration/strategies/compass_calibration.h"
#include "core/calibration/calibration_types.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>

using namespace mg::core::calibration;

static int g_pass = 0;
static int g_fail = 0;

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

    CHECK(env.mock->cal_start_received, "mock received PREFLIGHT_CALIBRATION command");
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

    CHECK(cal.interpret_ack(0, 0, CalibrationStage::Calibrating) == CalibrationStage::Success,
          "ACK accepted -> Success");
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
    test_cal_start_uses_heartbeat_target();
    test_success_history_clears_verification_failure();
    test_statustext_progress();
    test_statustext_success();
    test_statustext_failure();
    test_parse_statustext_edge_cases();
    test_interpret_ack();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
