#define MGCORE_EXPORTS
/**
 * ffi_diagnostics.cc - High-level diagnostics and client snapshots.
 *
 * Aggregates existing backend state into stable FFI contracts for clients,
 * support tooling, and hardware test reports.
 */
#include "ffi_internal.h"
#include "ffi_helpers.h"
#include "json_serializers.h"
#include "mission_json_serializers.h"
#include "param_json_serializers.h"
#include "sensor_health_json_serializers.h"

#include <cstring>
#include <string>

namespace {

std::string last_error_json() {
    const auto error = mg_get_last_error();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("code", static_cast<int>(error));
    jb.field("name", mg_error_string(error));
    jb.end_object();
    return jb.str();
}

std::string connection_state_name(mg_core_t* core) {
    if (!core) return "null_core";
    auto* vehicle = get_active_vehicle(core);
    if (!vehicle) return "no_vehicle";
    return mg::core::transport::ConnectionManager::state_string(vehicle->conn().state());
}

std::string connection_diagnostics_json(mg_core_t* core) {
    auto* vehicle = get_active_vehicle(core);
    auto* mav = get_active_mavlink(core);
    auto* cache = get_active_cache(core);
    const bool connected = is_active_connected(core);
    const uint64_t heartbeat_age = vehicle ? vehicle->conn().heartbeat_age_ms() : 0;
    const bool heartbeat_alive = vehicle ? (vehicle->conn().heartbeat_alive(3000) ||
                                            (connected && heartbeat_age <= 3000)) : false;
    const auto link_quality = mav
        ? mav->link_quality()
        : mg::core::mavlink::LinkQualityInfo{};

    mg::bridge::JsonBuilder heartbeat;
    heartbeat.begin_object();
    heartbeat.field("alive", heartbeat_alive);
    heartbeat.field("age_ms", static_cast<long long>(heartbeat_age));
    heartbeat.end_object();

    mg::bridge::JsonBuilder link;
    link.begin_object();
    link.field("measuring", link_quality.measuring);
    link.field("received", static_cast<int>(link_quality.received));
    link.field("lost", static_cast<int>(link_quality.lost));
    link.field("quality_pct", static_cast<int>(link_quality.quality_pct));
    link.end_object();

    mg::bridge::JsonBuilder target;
    target.begin_object();
    target.field("system_id", static_cast<int>(active_target_sys(core)));
    target.field("component_id", static_cast<int>(active_target_comp(core)));
    if (mav) {
        auto hb = mav->latest_heartbeat();
        target.field("seen", hb.has_value());
        if (hb) {
            target.field("vehicle_type", static_cast<int>(hb->vehicle_type));
            target.field("autopilot_type", static_cast<int>(hb->autopilot_type));
            target.field("flight_mode", std::string(hb->flight_mode));
            target.field("armed", hb->armed);
        }
    } else {
        target.field("seen", false);
    }
    target.end_object();

    mg::bridge::JsonBuilder vehicle_json;
    vehicle_json.begin_object();
    vehicle_json.field("active_vehicle_id", mg_vehicle_get_active(core));
    vehicle_json.field("has_active_vehicle", core->vehicles.active_vehicle() != nullptr);
    if (vehicle) {
        vehicle_json.field("vehicle_id", vehicle->vehicle_id());
    }
    if (cache) {
        vehicle_json.field("vehicle_name", cache->snapshot_vehicle_name());
        vehicle_json.field("board_name", cache->snapshot_board_name());
        vehicle_json.field("product_name", cache->snapshot_product_name());
        vehicle_json.field("usb_board_name", cache->snapshot_usb_board_name());
    }
    vehicle_json.end_object();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("connected", connected);
    jb.field("state", connection_state_name(core));
    jb.field("transport", connected ? "active" : "none");
    jb.field("reconnectable", !connected);
    jb.raw_field("heartbeat", heartbeat.str());
    jb.raw_field("link", link.str());
    jb.raw_field("target", target.str());
    jb.raw_field("vehicle", vehicle_json.str());
    jb.end_object();
    return jb.str();
}

std::string params_operation_json(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    const int received =
        mav ? static_cast<int>(mav->params_download_received()) : 0;
    const int total = mav ? static_cast<int>(mav->param_count()) : 0;
    const bool active = mav && mav->param_download_active();
    const bool cancelled = mav && mav->param_download_cancelled();
    const bool failed = mav && mav->param_download_failed();
    const std::string error = mav ? mav->param_download_error() : std::string();

    mg::bridge::JsonBuilder jb;
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
    jb.field("error", error);
    jb.end_object();
    return jb.str();
}

std::string param_change_history_json(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    const auto records = mav
        ? mav->param_change_history()
        : std::vector<mg::core::mavlink::MavlinkAdapter::ParamChangeRecord>{};

    mg::bridge::JsonBuilder array;
    array.begin_array();
    for (const auto& record : records) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("id", static_cast<unsigned long long>(record.id));
        item.field("name", record.name);
        item.field("requested_value", record.requested_value);
        item.field("actual_value", record.actual_value);
        item.field("type", static_cast<int>(record.type));
        item.field("state", record.state);
        item.field("error", record.error);
        item.field("requested_at_ms", static_cast<long long>(record.requested_at_ms));
        item.field("completed_at_ms", static_cast<long long>(record.completed_at_ms));
        item.end_object();
        array.append_json(item.str());
    }
    array.end_array();
    return array.str();
}

std::string mission_operation_json(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    auto progress = mav ? mg::bridge::ffi::present_mission_progress(core, mav->mission_get_progress())
                        : mg::core::mission::MissionTransferProgress{};
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

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("id", "mission");
    jb.field("type", "mission_transfer");
    jb.field("state", state);
    jb.field("progress_pct", pct);
    jb.field("received", static_cast<int>(progress.received));
    jb.field("sent", static_cast<int>(progress.sent));
    jb.field("total", static_cast<int>(progress.total));
    jb.field("cancellable", active);
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

std::string calibration_operation_json(mg_core_t* core) {
    const auto status = get_active_calibration(core).get_status();
    const char* state = "idle";
    if (status.active) {
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

    mg::bridge::JsonBuilder jb;
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
    jb.field("cancellable", status.active);
    jb.field("cancelled", status.stage == mg::core::calibration::CalibrationStage::Cancelled);
    jb.field("message", status.user_message);
    jb.field("error", status.failure_reason);
    jb.end_object();
    return jb.str();
}

std::string log_operation_json(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    const auto status = mav ? mav->log_download_status()
                            : mg::core::mavlink::MavlinkAdapter::LogDownloadStatus{};
    const char* state = "idle";
    if (status.active) {
        state = "downloading";
    } else if (status.completed) {
        state = "complete";
    } else if (status.cancelled) {
        state = "cancelled";
    } else if (!status.error.empty()) {
        state = "failed";
    }
    const int progress = status.total_bytes > 0
        ? static_cast<int>((status.received_bytes * 100) / status.total_bytes)
        : 0;

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("id", "log");
    jb.field("type", "log_download");
    jb.field("state", state);
    jb.field("progress_pct", progress);
    jb.field("log_id", static_cast<int>(status.id));
    jb.field("received_bytes", static_cast<int>(status.received_bytes));
    jb.field("total_bytes", static_cast<int>(status.total_bytes));
    jb.field("path", status.filepath);
    jb.field("cancellable", status.active);
    jb.field("cancelled", status.cancelled);
    jb.field("error", status.error);
    jb.end_object();
    return jb.str();
}

std::string operations_snapshot_json(mg_core_t* core) {
    mg::bridge::JsonBuilder ops;
    ops.begin_array();
    ops.append_json(params_operation_json(core));
    ops.append_json(mission_operation_json(core));
    ops.append_json(calibration_operation_json(core));
    ops.append_json(log_operation_json(core));
    ops.end_array();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("operations", ops.str());
    jb.raw_field("params", params_operation_json(core));
    jb.raw_field("mission", mission_operation_json(core));
    jb.raw_field("calibration", calibration_operation_json(core));
    jb.raw_field("log", log_operation_json(core));
    jb.end_object();
    return jb.str();
}

std::string telemetry_dashboard_json(mg_core_t* core) {
    auto* cache = get_active_cache(core);
    mg::bridge::JsonBuilder telemetry;
    telemetry.begin_object();
    if (cache) {
        telemetry.raw_field("summary", mg::bridge::heartbeat_json(cache->snapshot_heartbeat().data));
        {
            const auto attitude_snap = cache->snapshot_attitude();
            const auto& att = attitude_snap.data;
            const double r2d = 180.0 / 3.141592653589793;
            mg::bridge::JsonBuilder attitude;
            attitude.begin_object();
            attitude.field("roll_deg", att.roll * r2d);
            attitude.field("pitch_deg", att.pitch * r2d);
            attitude.field("yaw_deg", att.yaw * r2d);
            attitude.field("rollspeed_dps", att.rollspeed * r2d);
            attitude.field("pitchspeed_dps", att.pitchspeed * r2d);
            attitude.field("yawspeed_dps", att.yawspeed * r2d);
            attitude.end_object();
            telemetry.raw_field("attitude", attitude.str());
        }
        telemetry.raw_field("gps", cache->snapshot_gps().to_json());
        telemetry.raw_field("global_position", cache->snapshot_global_pos().to_json());
        telemetry.raw_field("battery", cache->snapshot_battery().to_json());
        {
            const auto vfr_snap = cache->snapshot_vfr_hud();
            const auto& vfr = vfr_snap.data;
            mg::bridge::JsonBuilder vfr_hud;
            vfr_hud.begin_object();
            vfr_hud.field("airspeed_m_s", vfr.airspeed);
            vfr_hud.field("groundspeed_m_s", vfr.groundspeed);
            vfr_hud.field("alt_msl_m", vfr.alt);
            vfr_hud.field("climb_m_s", vfr.climb);
            vfr_hud.field("heading_deg", static_cast<double>(vfr.heading));
            vfr_hud.field("throttle_pct", static_cast<double>(vfr.throttle));
            vfr_hud.end_object();
            telemetry.raw_field("vfr_hud", vfr_hud.str());
        }
        telemetry.raw_field("sensor_health", cache->snapshot_sys_status().health_json());
    }
    telemetry.end_object();

    mg::bridge::JsonBuilder validity;
    validity.begin_object();
    if (cache) {
        validity.field("heartbeat", cache->is_fresh(mg::bridge::TelemetryType::kHeartbeat));
        validity.field("attitude", cache->is_fresh(mg::bridge::TelemetryType::kAttitude));
        validity.field("gps", cache->is_fresh(mg::bridge::TelemetryType::kGps));
        validity.field("global_position", cache->is_fresh(mg::bridge::TelemetryType::kGlobalPosition));
        validity.field("battery", cache->is_fresh(mg::bridge::TelemetryType::kBattery));
        validity.field("vfr_hud", cache->is_fresh(mg::bridge::TelemetryType::kVfrHud));
    }
    validity.end_object();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("connection", connection_diagnostics_json(core));
    jb.raw_field("telemetry", telemetry.str());
    jb.raw_field("freshness", cache ? cache->freshness_json() : "{}");
    jb.raw_field("validity", validity.str());
    jb.end_object();
    return jb.str();
}

} // namespace

MGCORE_API char* mg_connection_get_diagnostics(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(connection_diagnostics_json(core));
}

MGCORE_API char* mg_operations_get_snapshot(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(operations_snapshot_json(core));
}

MGCORE_API char* mg_operations_cancel(mg_core_t* core, const char* operation_id) {
    CHECK_CORE(core);
    const std::string id = operation_id ? operation_id : "";
    const bool known =
        id == "params" || id == "mission" || id == "calibration" || id == "log";

    bool canceled = false;
    std::string reason = known ? "no active cancellable operation" : "unknown operation";
    if (id == "params") {
        if (auto* mav = get_active_mavlink(core)) {
            canceled = mav->cancel_param_download();
            if (canceled) reason.clear();
        }
    } else if (id == "mission") {
        if (auto* mav = get_active_mavlink(core)) {
            canceled = mav->mission_cancel();
            if (canceled) reason.clear();
        }
    } else if (id == "calibration") {
        auto& calibration = get_active_calibration(core);
        if (calibration.is_active()) {
            char* result = mg_calibration_cancel(core);
            const bool request_ok =
                result && std::strstr(result, "\"success\":true") != nullptr;
            if (result) mg_free_string(result);
            canceled = !calibration.is_active();
            reason = canceled
                ? std::string()
                : (request_ok ? "calibration cancel is still pending"
                              : "failed to cancel calibration");
        }
    } else if (id == "log") {
        if (auto* mav = get_active_mavlink(core)) {
            if (mav->log_download_active()) {
                canceled = mav->log_download_cancel();
                reason = canceled ? std::string()
                                  : "failed to cancel log download";
            }
        }
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", known);
    jb.field("operation_id", id);
    jb.field("canceled", canceled);
    jb.field("reason", reason);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_telemetry_get_dashboard_snapshot(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(telemetry_dashboard_json(core));
}

MGCORE_API char* mg_system_get_diagnostics(mg_core_t* core) {
    CHECK_CORE(core);

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("connection", connection_diagnostics_json(core));
    jb.raw_field("operations", operations_snapshot_json(core));
    jb.raw_field("dashboard", telemetry_dashboard_json(core));
    jb.raw_field("parameter_history", param_change_history_json(core));
    jb.raw_field("last_error", last_error_json());
    jb.end_object();
    return to_cstr(jb.str());
}
