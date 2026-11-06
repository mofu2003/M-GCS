#define MGCORE_EXPORTS
/**
 * ffi_diagnostics.cc - High-level diagnostics and client snapshots.
 *
 * Aggregates existing backend state into stable FFI contracts for clients,
 * support tooling, and hardware test reports.
 */
#include "ffi_internal.h"
#include "ffi_helpers.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/param_json_serializers.h"
#include "core/telemetry/sensor_health_json_serializers.h"
#include "ffi_operations_internal.h"

#include <string>

namespace {

using mg::bridge::operations_detail::operations_snapshot_json;

std::string last_error_json() {
    const auto error = mg_get_last_error();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("code", static_cast<int>(error));
    jb.field("name", ffi_error_name(error));
    jb.field("message", mg_error_string(error));
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
    auto mav = shared_active_mavlink(core);
    auto* cache = get_active_cache(core);
    const bool connected = is_active_connected(core);
    const uint64_t heartbeat_age = vehicle ? vehicle->conn().heartbeat_age_ms() : 0;
    const bool heartbeat_alive = vehicle ? (vehicle->conn().heartbeat_alive(3000) ||
                                            (connected && heartbeat_age <= 3000)) : false;
    const auto link_quality = mav
        ? mav->link_quality()
        : mg::core::mavlink::LinkQualityInfo{};

    mg::core::telemetry::JsonBuilder heartbeat;
    heartbeat.begin_object();
    heartbeat.field("alive", heartbeat_alive);
    heartbeat.field("age_ms", static_cast<long long>(heartbeat_age));
    heartbeat.end_object();

    mg::core::telemetry::JsonBuilder link;
    link.begin_object();
    link.field("measuring", link_quality.measuring);
    link.field("received", static_cast<int>(link_quality.received));
    link.field("lost", static_cast<int>(link_quality.lost));
    link.field("quality_pct", static_cast<int>(link_quality.quality_pct));
    link.end_object();

    mg::core::telemetry::JsonBuilder target;
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

    mg::core::telemetry::JsonBuilder vehicle_json;
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

    mg::core::telemetry::JsonBuilder jb;
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

std::string param_change_history_json(mg_core_t* core) {
    auto mav = shared_active_mavlink(core);
    const auto records = mav
        ? mav->param_change_history()
        : std::vector<mg::core::mavlink::MavlinkAdapter::ParamChangeRecord>{};

    mg::core::telemetry::JsonBuilder array;
    array.begin_array();
    for (const auto& record : records) {
        mg::core::telemetry::JsonBuilder item;
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

std::string telemetry_dashboard_json(mg_core_t* core) {
    auto* cache = get_active_cache(core);
    mg::core::telemetry::JsonBuilder telemetry;
    telemetry.begin_object();
    if (cache) {
        telemetry.raw_field("summary", mg::core::telemetry::heartbeat_json(cache->snapshot_heartbeat().data));
        {
            const auto attitude_snap = cache->snapshot_attitude();
            const auto& att = attitude_snap.data;
            const double r2d = 180.0 / 3.141592653589793;
            mg::core::telemetry::JsonBuilder attitude;
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
            const auto rc_snap = cache->snapshot_rc();
            const bool rc_valid = cache->is_fresh(
                mg::core::telemetry::TelemetryType::kRcChannels, 500) &&
                rc_snap.data.chancount > 0;
            telemetry.raw_field(
                "rc_channels",
                mg::core::telemetry::rc_channels_json(rc_snap.data, rc_valid));
        }
        {
            const auto servo_snap = cache->snapshot_servo();
            const bool servo_valid = cache->is_fresh(
                mg::core::telemetry::TelemetryType::kServoOutput, 500);
            telemetry.raw_field(
                "servo_output",
                mg::core::telemetry::servo_output_json(servo_snap.data, servo_valid));
        }
        {
            const auto vfr_snap = cache->snapshot_vfr_hud();
            const auto& vfr = vfr_snap.data;
            mg::core::telemetry::JsonBuilder vfr_hud;
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

    mg::core::telemetry::JsonBuilder validity;
    validity.begin_object();
    if (cache) {
        validity.field("heartbeat", cache->is_fresh(mg::core::telemetry::TelemetryType::kHeartbeat));
        validity.field("attitude", cache->is_fresh(mg::core::telemetry::TelemetryType::kAttitude));
        validity.field("gps", cache->is_fresh(mg::core::telemetry::TelemetryType::kGps));
        validity.field("global_position", cache->is_fresh(mg::core::telemetry::TelemetryType::kGlobalPosition));
        validity.field("battery", cache->is_fresh(mg::core::telemetry::TelemetryType::kBattery));
        validity.field("vfr_hud", cache->is_fresh(mg::core::telemetry::TelemetryType::kVfrHud));
    }
    validity.end_object();

    mg::core::telemetry::JsonBuilder jb;
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

MGCORE_API char* mg_telemetry_get_dashboard_snapshot(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(telemetry_dashboard_json(core));
}

MGCORE_API char* mg_system_get_diagnostics(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::telemetry::JsonBuilder jb;
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
