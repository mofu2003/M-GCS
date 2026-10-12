/* Long-running operation snapshots and cancellation entry points. */
#define MGCORE_EXPORTS

#include "ffi_internal.h"
#include "ffi_helpers.h"
#include "core/telemetry/json_serializers.h"
#include "mission_json_serializers.h"
#include "ffi_operations_internal.h"

#include <cstring>
#include <string>

namespace mg::bridge::operations_detail {

std::string params_operation_json(mg_core_t* core) {
    auto* vehicle = get_active_vehicle(core);
    const auto mav = vehicle ? vehicle->operation_mavlink("params") : nullptr;
    const int received =
        mav ? static_cast<int>(mav->params_download_received()) : 0;
    const int total = mav ? static_cast<int>(mav->param_count()) : 0;
    const bool active = mav && mav->param_download_active();
    const bool cancelled = mav && mav->param_download_cancelled();
    const bool failed = mav && mav->param_download_failed();
    const std::string error = mav ? mav->param_download_error() : std::string();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("id", "params");
    jb.field("type", "parameter_sync");
    jb.field(
        "state",
        active ? "running"
               : cancelled ? "cancelled"
                           : failed ? "failed"
                                    : (total > 0 ? "complete" : "idle"));
    jb.field("progress_pct", total > 0 ? (received * 100) / total : 0);
    jb.field("received", received);
    jb.field("total", total);
    jb.field("cancellable", active);
    jb.field("cancelled", cancelled);
    jb.field("retry_count",
             mav ? mav->param_download_retry_count() : 0);
    jb.field("missing_count",
             mav ? mav->param_download_missing_count() : 0);
    jb.field("failed_count",
             mav ? mav->param_download_failed_count() : 0);
    jb.field("error", error);
    jb.end_object();
    return jb.str();
}

std::string transfer_operation_json(
    const char* id,
    const char* type,
    const mg::core::mission::MissionTransferProgress& progress,
    int retry_count) {
    const bool active =
        progress.state == mg::core::mission::MissionState::Downloading ||
        progress.state == mg::core::mission::MissionState::Uploading ||
        progress.state == mg::core::mission::MissionState::WaitingAck;

    int pct = 0;
    if (progress.total > 0) {
        if (progress.received > 0) pct = (progress.received * 100) / progress.total;
        else if (progress.sent > 0) pct = (progress.sent * 100) / progress.total;
    }

    const char* state = "idle";
    switch (progress.state) {
        case mg::core::mission::MissionState::Idle: state = "idle"; break;
        case mg::core::mission::MissionState::Downloading: state = "downloading"; break;
        case mg::core::mission::MissionState::Uploading: state = "uploading"; break;
        case mg::core::mission::MissionState::WaitingAck: state = "waiting_ack"; break;
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("id", id);
    jb.field("type", type);
    jb.field("state", state);
    jb.field("progress_pct", pct);
    jb.field("received", static_cast<int>(progress.received));
    jb.field("sent", static_cast<int>(progress.sent));
    jb.field("total", static_cast<int>(progress.total));
    jb.field("cancellable", active);
    jb.field("retry_count", retry_count);
    jb.field("result", static_cast<int>(progress.result));
    jb.field(
        "result_text",
        mg::core::mission::mission_result_message(progress.result));
    jb.field(
        "cancelled",
        progress.result == MAV_MISSION_OPERATION_CANCELLED);
    jb.field("error", progress.error);
    jb.end_object();
    return jb.str();
}

std::string mission_operation_json(mg_core_t* core) {
    auto* vehicle = get_active_vehicle(core);
    const auto mav = vehicle ? vehicle->operation_mavlink("mission") : nullptr;
    const auto progress = mav
        ? mg::bridge::ffi::present_mission_progress(core, mav->mission_get_progress())
        : mg::core::mission::MissionTransferProgress{};
    return transfer_operation_json(
        "mission", "mission_transfer", progress,
        mav ? mav->mission_transfer_retry_count() : 0);
}

std::string fence_operation_json(mg_core_t* core) {
    auto* vehicle = get_active_vehicle(core);
    const auto mav = vehicle ? vehicle->operation_mavlink("fence") : nullptr;
    const auto progress = mav ? mav->fence_get_progress()
                              : mg::core::mission::MissionTransferProgress{};
    return transfer_operation_json(
        "fence", "fence_transfer", progress,
        mav ? mav->fence_transfer_retry_count() : 0);
}

std::string rally_operation_json(mg_core_t* core) {
    auto* vehicle = get_active_vehicle(core);
    const auto mav = vehicle ? vehicle->operation_mavlink("rally") : nullptr;
    const auto progress = mav ? mav->rally_get_progress()
                              : mg::core::mission::MissionTransferProgress{};
    return transfer_operation_json(
        "rally", "rally_transfer", progress,
        mav ? mav->rally_transfer_retry_count() : 0);
}

std::string calibration_operation_json(mg_core_t* core) {
    const auto status = get_active_calibration(core).get_status();
    const bool cancelling =
        status.stage == mg::core::calibration::CalibrationStage::Cancelling;
    const char* state = "idle";
    if (cancelling) {
        // 取消命令已下发、飞控尚未确认：仍在进行中，但不允许再点一次取消。
        state = "cancelling";
    } else if (status.active) {
        state = "running";
    } else {
        switch (status.stage) {
            case mg::core::calibration::CalibrationStage::Cancelled:
                state = "cancelled";
                break;
            case mg::core::calibration::CalibrationStage::Failed:
                state = "failed";
                break;
            case mg::core::calibration::CalibrationStage::Success:
                state = "complete";
                break;
            default:
                state = "idle";
                break;
        }
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("id", "calibration");
    jb.field("type", "calibration");
    jb.field("state", state);
    jb.field(
        "calibration_type",
        mg::core::calibration::CalibrationValidator::type_string(status.current_type));
    jb.field("stage", mg::core::calibration::CalibrationValidator::stage_string(status.stage));
    jb.field("progress_pct", static_cast<int>(status.progress));
    jb.field("current_step", static_cast<int>(status.current_step));
    jb.field("total_steps", static_cast<int>(status.total_steps));
    jb.field("cancellable", status.active && !cancelling);
    jb.field("cancelled", status.stage == mg::core::calibration::CalibrationStage::Cancelled);
    jb.field("message", status.user_message);
    jb.field("error", status.failure_reason);
    jb.end_object();
    return jb.str();
}

std::string log_operation_json(mg_core_t* core) {
    auto* vehicle = get_active_vehicle(core);
    const auto mav = vehicle ? vehicle->operation_mavlink("log") : nullptr;
    const auto status = mav ? mav->log_download_status()
                            : mg::core::mavlink::MavlinkAdapter::LogDownloadStatus{};
    const char* state = "idle";
    if (status.active) {
        state = "downloading";
    } else if (status.saving) {
        state = "saving";
    } else if (status.completed) {
        state = "complete";
    } else if (status.cancelled) {
        state = "cancelled";
    } else if (!status.error.empty()) {
        state = "failed";
    }
    // 64 位中间量：received_bytes 可超过 42 MB，直接乘 100 会溢出 uint32。
    const int progress = status.total_bytes > 0
        ? static_cast<int>((static_cast<uint64_t>(status.received_bytes) * 100) /
                           status.total_bytes)
        : 0;

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("id", "log");
    jb.field("type", "log_download");
    jb.field("state", state);
    jb.field("progress_pct", progress);
    jb.field("log_id", static_cast<int>(status.id));
    jb.field("received_bytes", static_cast<unsigned long long>(status.received_bytes));
    jb.field("total_bytes", static_cast<unsigned long long>(status.total_bytes));
    jb.field("size_declared", status.size_declared);
    jb.field("saved_bytes", static_cast<unsigned long long>(status.saved_bytes));
    jb.field("path", status.filepath);
    jb.field("cancellable", status.active);
    jb.field("cancelled", status.cancelled);
    jb.field("error", status.error);
    jb.end_object();
    return jb.str();
}

std::string operations_snapshot_json(mg_core_t* core) {
    mg::core::telemetry::JsonBuilder ops;
    ops.begin_array();
    ops.append_json(params_operation_json(core));
    ops.append_json(mission_operation_json(core));
    ops.append_json(fence_operation_json(core));
    ops.append_json(rally_operation_json(core));
    ops.append_json(calibration_operation_json(core));
    ops.append_json(log_operation_json(core));
    ops.end_array();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("operations", ops.str());
    jb.raw_field("params", params_operation_json(core));
    jb.raw_field("mission", mission_operation_json(core));
    jb.raw_field("fence", fence_operation_json(core));
    jb.raw_field("rally", rally_operation_json(core));
    jb.raw_field("calibration", calibration_operation_json(core));
    jb.raw_field("log", log_operation_json(core));
    jb.end_object();
    return jb.str();
}

} // namespace mg::bridge::operations_detail

MGCORE_API char* mg_operations_get_snapshot(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(
        mg::bridge::operations_detail::operations_snapshot_json(core));
}

MGCORE_API char* mg_operations_get_snapshot_for_vehicle(
    mg_core_t* core, int vehicle_id) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    mg::bridge::ffi::VehicleTargetScope scope(core, vehicle);
    return to_cstr(
        mg::bridge::operations_detail::operations_snapshot_json(core));
}

MGCORE_API char* mg_operations_cancel(mg_core_t* core, const char* operation_id) {
    CHECK_CORE(core);
    const std::string id = operation_id ? operation_id : "";
    const bool known =
        id == "params" || id == "mission" || id == "fence" ||
        id == "rally" || id == "calibration" || id == "log";

    bool canceled = false;
    std::string reason = known ? "no active cancellable operation" : "unknown operation";
    if (id == "params") {
        if (auto* vehicle = get_active_vehicle(core)) {
            canceled = vehicle->cancel_operation("params");
            if (canceled) reason.clear();
        }
    } else if (id == "mission") {
        if (auto* vehicle = get_active_vehicle(core)) {
            canceled = vehicle->cancel_operation("mission");
            if (canceled) reason.clear();
        }
    } else if (id == "fence") {
        if (auto* vehicle = get_active_vehicle(core)) {
            canceled = vehicle->cancel_operation("fence");
            if (canceled) reason.clear();
        }
    } else if (id == "rally") {
        if (auto* vehicle = get_active_vehicle(core)) {
            canceled = vehicle->cancel_operation("rally");
            if (canceled) reason.clear();
        }
    } else if (id == "calibration") {
        auto& calibration = get_active_calibration(core);
        if (!calibration.is_active()) {
            reason = "no active cancellable operation";
        } else {
            char* result = mg_calibration_cancel(core);
            const bool request_ok =
                result && std::strstr(result, "\"success\":true") != nullptr;
            if (result) mg_free_string(result);
            // 校准取消是异步的：请求被受理即视为已下发（快照的 state 会短暂
            // 显示 cancelling），等飞控 COMMAND_ACK 才真正落到 cancelled。
            canceled = request_ok;
            reason = request_ok ? std::string() : "failed to cancel calibration";
        }
    } else if (id == "log") {
        if (auto* vehicle = get_active_vehicle(core)) {
            canceled = vehicle->cancel_operation("log");
            if (canceled) reason.clear();
        }
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", known);
    jb.field("operation_id", id);
    jb.field("canceled", canceled);
    jb.field("reason", reason);
    if (!known) {
        // F4：success:false 必须带统一 error 对象。reason 保留（界面直接显示原文），
        // error 供调用方按错误码分支。
        write_error(jb, MG_ERROR_INVALID_PARAM, reason);
    }
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_operations_cancel_for_vehicle(
    mg_core_t* core, int vehicle_id, const char* operation_id) {
    CHECK_CORE(core);
    if (vehicle_id <= 0) return fail(MG_ERROR_INVALID_PARAM, "invalid vehicle");
    auto vehicle = core->vehicles.get_vehicle_shared(vehicle_id);
    if (!vehicle) return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    mg::bridge::ffi::VehicleTargetScope scope(core, vehicle);
    return mg_operations_cancel(core, operation_id);
}
