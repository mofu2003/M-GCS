#define MGCORE_EXPORTS
/**
 * ffi_calibration.cc - 传感器校准 FFI 入口
 *
 * 管理陀螺仪、加速度计、罗盘、气压计校准流程。
 *
 * mg_calibration_start, mg_calibration_cancel, mg_calibration_get_status,
 * mg_calibration_get_history, mg_calibration_get_retry_info,
 * mg_calibration_process_statustext, mg_calibration_process_command_ack
 *
 * 所有调用都通过 active vehicle 的 CalibrationService 路由。
 */
#include "ffi_internal.h"
#include "ffi_helpers.h"

using namespace mg::bridge::ffi_helpers;

MGCORE_API char* mg_calibration_start(mg_core_t* core, int type, int accel_mode) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto& cal = get_active_calibration(core);
    if (cal.is_active()) return fail(MG_ERROR_CALIBRATION, "calibration already in progress");
    if (type < 0 || type > 3) return fail(MG_ERROR_INVALID_PARAM, "invalid calibration type (0=gyro, 1=accel, 2=compass, 3=baro)");
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
    cal.cancel();
    return ok();
}

MGCORE_API char* mg_calibration_get_status(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(cal_status_json(get_active_calibration(core).get_status()));
}

MGCORE_API char* mg_calibration_get_history(mg_core_t* core, int type) {
    CHECK_CORE(core);
    if (type < 0 || type > 3) return to_cstr("[]");
    auto history = get_active_calibration(core).get_history(static_cast<mg::core::calibration::CalibrationType>(type));
    return to_cstr(cal_history_json(history));
}

MGCORE_API char* mg_calibration_get_retry_info(mg_core_t* core) {
    CHECK_CORE(core);
    auto status = get_active_calibration(core).get_status();
    mg::core::calibration::RetryInfo info;
    info.retry_count = status.retry_count;
    info.max_retries = status.max_retries;
    info.last_failure = status.failure_reason;
    return to_cstr(cal_retry_json(info));
}

void mg_calibration_process_statustext(mg_core_t* core, const char* text, uint8_t severity) {
    if (!core || !text) return;
    get_active_calibration(core).process_statustext(std::string(text), severity);
}

void mg_calibration_process_command_ack(mg_core_t* core, uint16_t command, uint8_t result, uint8_t progress) {
    if (!core) return;
    get_active_calibration(core).process_command_ack(command, result, progress);
}

// ================================================================
// 统一校准别名：与 ffi_api.h 中的公开声明保持一致。
// ================================================================
MGCORE_API char* mg_cal_start(mg_core_t* core, int type, int mode) {
    return mg_calibration_start(core, type, mode);
}

MGCORE_API char* mg_cal_cancel(mg_core_t* core) {
    return mg_calibration_cancel(core);
}

MGCORE_API char* mg_cal_status(mg_core_t* core) {
    return mg_calibration_get_status(core);
}

MGCORE_API char* mg_cal_history(mg_core_t* core, int type) {
    return mg_calibration_get_history(core, type);
}

MGCORE_API char* mg_cal_retry_info(mg_core_t* core) {
    return mg_calibration_get_retry_info(core);
}
