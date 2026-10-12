#define MGCORE_EXPORTS
/**
 * ffi_calibration.cc - 传感器校准 FFI 入口
 *
 * 管理陀螺仪、加速度计、罗盘、气压计、遥控器校准流程。
 *
 * mg_calibration_start, mg_calibration_cancel, mg_calibration_get_status,
 * mg_calibration_get_history, mg_calibration_get_retry_info,
 * mg_calibration_accel_confirm, mg_calibration_start_airspeed
 *
 * 取消是异步的：mg_calibration_cancel 只负责把取消命令下发并把状态置为
 * cancelling，等飞控 COMMAND_ACK（或超时/链路断开）才落到 cancelled。
 * 在飞控确认之前，状态查询的 active 仍为 true（safety.md S8）。
 *
 * 所有调用都通过 active vehicle 的 CalibrationService 路由。
 */
#include "ffi_internal.h"
#include "ffi_helpers.h"

using namespace mg::bridge::ffi_helpers;

namespace {

/// 当前飞机是否为 PX4。校准命令里有几条是 ArduPilot 专有的
/// （DO_CANCEL_MAG_CAL / ACCELCAL_VEHICLE_POS），PX4 会直接忽略。
bool active_is_px4(mg_core_t* core) {
    auto mav = shared_active_mavlink(core);
    if (!mav) return false;
    const auto heartbeat = mav->latest_heartbeat();
    return heartbeat.has_value() && heartbeat->autopilot_type == MAV_AUTOPILOT_PX4;
}

}  // namespace

MGCORE_API char* mg_calibration_start(mg_core_t* core, int type, int accel_mode) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto& cal = get_active_calibration(core);
    if (cal.is_active()) return fail(MG_ERROR_CALIBRATION, "calibration already in progress");
    if (type < 0 || type > 4) return fail(MG_ERROR_INVALID_PARAM, "invalid calibration type (0=gyro, 1=accel, 2=compass, 3=baro, 4=radio)");
    if (type == 1 && accel_mode != 1 && accel_mode != 2 && accel_mode != 4) return fail(MG_ERROR_INVALID_PARAM, "invalid accel mode (1=full, 2=trim, 4=simple)");

    auto cal_type = static_cast<mg::core::calibration::CalibrationType>(type);
    auto mode = static_cast<mg::core::calibration::AccelCalMode>(accel_mode);

    cal.set_connection(&get_active_conn(core));
    cal.set_target(active_target_sys(core), active_target_comp(core));
    bool started = cal.start(cal_type, mode);
    if (started) return ok();
    return fail(MG_ERROR_CALIBRATION, "failed to start calibration");
}

MGCORE_API char* mg_calibration_cancel(mg_core_t* core) {
    CHECK_CORE(core);
    auto& cal = get_active_calibration(core);
    if (!cal.is_active()) return fail(MG_ERROR_INVALID_STATE, "no calibration in progress");
    // QGC 的 PX4 控制器同样不发取消命令，并注明固件不支持
    // （SensorsComponentController.cc:397-399 "The firmware doesn't allow us to
    // cancel calibration. The best we can do is wait"）。与其让界面显示"已取消"
    // 而飞控仍在采样，不如明确告知不支持（S8）。
    if (active_is_px4(core)) {
        return fail(MG_ERROR_NOT_SUPPORTED,
                    "PX4 firmware does not support cancelling calibration; wait for the "
                    "flight controller to finish");
    }
    const auto status = cal.get_status();
    auto mav = shared_active_mavlink(core);

    // 先进入 Cancelling，再发取消命令：链路读线程可能在 send 返回后、状态改
    // 动前就把 COMMAND_ACK 递进来，先置状态才不会漏掉那条确认。
    if (!cal.cancel()) {
        return fail(MG_ERROR_INVALID_STATE, "no calibration in progress");
    }

    bool command_sent = true;
    if (mav && status.current_type == mg::core::calibration::CalibrationType::Accelerometer) {
        command_sent = mav->send_command_long(
            active_target_sys(core), active_target_comp(core),
            42429, 0, 16777216.0f, 0, 0, 0, 0, 0, 0);
    } else if (mav && status.current_type == mg::core::calibration::CalibrationType::Magnetometer) {
        command_sent = mav->send_command_long(
            active_target_sys(core), active_target_comp(core),
            42426, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        // Do not leave the explicit 10 Hz calibration streams running after
        // cancellation.
        mav->request_message_interval(
            active_target_sys(core), active_target_comp(core), 191, -1);
        mav->request_message_interval(
            active_target_sys(core), active_target_comp(core), 192, -1);
    } else if (mav) {
        // 陀螺/气压/遥控没有专用取消命令。QGC 用全 0 的 PREFLIGHT_CALIBRATION
        // 让飞控停止这些校准（Vehicle.cc:2481-2491），这里保持一致：
        // 只改本地状态而不通知飞控，会让飞控继续校准而界面显示"已取消"。
        command_sent = mav->send_command_long(
            active_target_sys(core), active_target_comp(core),
            241, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (!command_sent) {
        // 取消命令没发出去，就不会有飞控确认。立刻收尾为 Cancelled 并写明原因，
        // 不让界面停在“取消中”等一次永远不会到来的 ACK。
        cal.finish_cancel(
            "Cancelled by user (cancel command could not be sent to the flight controller)");
        return fail(MG_ERROR_SEND_FAILED,
                    "calibration cancelled locally, but the cancel command was not sent");
    }

    // success 只表示“取消请求已下发并受理”。真正停止要等飞控 COMMAND_ACK；
    // 在此之前 state 为 cancelling，界面保持轮询（S8）。
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("state", "cancelling");
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_calibration_get_status(mg_core_t* core) {
    CHECK_CORE(core);
    auto& cal = get_active_calibration(core);
    // 界面按固定节奏轮询状态，这里顺带驱动看门狗：链路断开或采样停滞时把校准
    // 判定为失败并给出原因，避免操作者一直停在"校准中"。
    cal.poll();
    return to_cstr(cal_status_json(cal.get_status()));
}

MGCORE_API char* mg_calibration_get_history(mg_core_t* core, int type) {
    CHECK_CORE(core);
    if (type < 0 || type > 4) return to_cstr("[]");
    auto& cal = get_active_calibration(core);
    cal.poll();
    auto history = cal.get_history(static_cast<mg::core::calibration::CalibrationType>(type));
    return to_cstr(cal_history_json(history));
}

MGCORE_API char* mg_calibration_get_retry_info(mg_core_t* core) {
    CHECK_CORE(core);
    auto& cal = get_active_calibration(core);
    cal.poll();
    auto status = cal.get_status();
    mg::core::calibration::RetryInfo info;
    info.retry_count = status.retry_count;
    info.max_retries = status.max_retries;
    info.last_failure = status.failure_reason;
    return to_cstr(cal_retry_json(info));
}

MGCORE_API char* mg_calibration_accel_confirm(mg_core_t* core, int position) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (position < 1 || position > 6) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid accelerometer position");
    }
    if (active_is_px4(core)) {
        // ACCELCAL_VEHICLE_POS 是 ArduPilot 专有的六面确认流程，PX4 不处理。
        return fail(MG_ERROR_NOT_SUPPORTED,
                    "PX4 does not use the six-face accelerometer confirmation flow");
    }
    const auto status = get_active_calibration(core).get_status();
    if (!status.active ||
        status.current_type != mg::core::calibration::CalibrationType::Accelerometer) {
        return fail(MG_ERROR_INVALID_STATE, "accelerometer calibration is not active");
    }
    if (status.current_step != position) {
        return fail(MG_ERROR_INVALID_STATE, "flight controller is not requesting this accelerometer position");
    }
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    const bool sent = mav->send_command_long(
        active_target_sys(core), active_target_comp(core),
        42429, 0, static_cast<float>(position), 0, 0, 0, 0, 0, 0);
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "failed to confirm accelerometer position");
}

MGCORE_API char* mg_calibration_start_airspeed(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (get_active_calibration(core).is_active()) {
        return fail(MG_ERROR_CALIBRATION, "calibration already in progress");
    }
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    const bool sent = mav->send_command_long(
        active_target_sys(core), active_target_comp(core),
        241, 0, 0, 0, 0, 0, 0, 2, 0);
    return sent ? ok() : fail(MG_ERROR_SEND_FAILED, "failed to start airspeed calibration");
}
