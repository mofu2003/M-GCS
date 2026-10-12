#define MGCORE_EXPORTS
/**
 * ffi_vehicle.cc - Section 2: Vehicle Health + Firmware
 *
 * 传感器健康、飞控固件状态、固件版本请求。
 */
#include "ffi_internal.h"
#include "ffi_response.h"
#include "core/telemetry/json_serializers.h"
#include "core/telemetry/sensor_health_json_serializers.h"
#include "core/firmware/firmware_adapter.h"
#include "core/firmware/firmware_plugin.h"

namespace {

const char* firmware_profile(mg::core::firmware::FirmwareKind kind) {
    switch (kind) {
        case mg::core::firmware::FirmwareKind::ArduPilot:
            return "ardupilot";
        case mg::core::firmware::FirmwareKind::PX4:
            return "px4";
        default:
            return "unknown";
    }
}

void capability_field(mg::core::telemetry::JsonBuilder& jb,
                      const char* name,
                      uint32_t flags,
                      mg::core::firmware::FirmwareCapability capability) {
    jb.field(name, (flags & static_cast<uint32_t>(capability)) != 0);
}

std::string capability_json(uint32_t flags, bool has_param_remapping) {
    mg::core::telemetry::JsonBuilder caps;
    caps.begin_object();
    capability_field(caps, "guided_mode", flags,
                     mg::core::firmware::CapGuidedMode);
    capability_field(caps, "pause", flags, mg::core::firmware::CapPause);
    capability_field(caps, "takeoff", flags, mg::core::firmware::CapTakeoff);
    capability_field(caps, "orbit", flags, mg::core::firmware::CapOrbit);
    capability_field(caps, "smart_rtl", flags,
                     mg::core::firmware::CapSmartRTL);
    capability_field(caps, "set_home", flags, mg::core::firmware::CapSetHome);
    capability_field(caps, "motor_test", flags,
                     mg::core::firmware::CapMotorTest);
    capability_field(caps, "calibration", flags,
                     mg::core::firmware::CapCalibration);
    capability_field(caps, "roi", flags, mg::core::firmware::CapRoi);
    capability_field(caps, "change_heading", flags,
                     mg::core::firmware::CapChangeHeading);
    caps.field("param_remapping", has_param_remapping);
    caps.end_object();
    return caps.str();
}

}  // namespace

// ================================================================
// Vehicle / Sensor getters (all follow the same pattern)
// ================================================================


MGCORE_API char* mg_sensors_get_health(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"present":0,"enabled":0,"health":0})");
    auto snap = cache->snapshot_sys_status();
    const auto hb = cache->snapshot_heartbeat().data;
    return to_cstr(mg::core::telemetry::sensor_health_json(snap.data, hb.autopilot_type));
}

MGCORE_API char* mg_vehicle_get_firmware(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"autopilot_type":0,"vehicle_type":0})");
    auto snap = cache->snapshot_heartbeat();
    return to_cstr(mg::core::telemetry::firmware_json(snap.data));
}
MGCORE_API char* mg_sensors_get_statustext(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) return to_cstr(R"({"severity":7,"text":"","severity_label":"debug"})");
    auto snap = cache->snapshot_statustext();
    return to_cstr(mg::core::telemetry::statustext_json(snap.data));
}


MGCORE_API char* mg_vehicle_request_firmware_version(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    if (mav->request_autopilot_version(active_target_sys(core), active_target_comp(core))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_firmware_get_type(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    if (!cache) {
        mg::core::telemetry::JsonBuilder empty;
        empty.begin_object();
        empty.field("schema_version", 1);
        empty.field("firmware", "Unknown");
        empty.field("profile", "unknown");
        empty.field("vehicle_type", "Unknown");
        empty.field("autopilot_type", 0);
        empty.field("vehicle_type_id", 0);
        empty.field("capability_flags", 0);
        empty.field("minimum_takeoff_altitude_m", 0.0);
        empty.raw_field("capabilities", capability_json(0, false));
        empty.end_object();
        return mg::bridge::ffi::response::ok(empty.str());
    }
    auto hb_snap = cache->snapshot_heartbeat();
    const auto& hb = hb_snap.data;
    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(hb.autopilot_type, hb.vehicle_type);
    auto plugin =
        mg::core::firmware::create_firmware_plugin(adapter.firmware_kind());
    const uint32_t flags =
        plugin ? plugin->capabilities(adapter.vehicle_kind()) : 0;
    const bool has_param_remapping = plugin && plugin->has_param_remapping();
    const float minimum_takeoff_altitude_m =
        plugin ? plugin->minimum_takeoff_altitude(adapter.vehicle_kind()) : 0.0f;

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("schema_version", 1);
    jb.field("firmware", adapter.firmware_name());
    jb.field("profile", firmware_profile(adapter.firmware_kind()));
    jb.field("vehicle_type", adapter.normalized_vehicle_name());
    jb.field("autopilot_type", (int)hb.autopilot_type);
    jb.field("vehicle_type_id", (int)hb.vehicle_type);
    jb.field("capability_flags", static_cast<unsigned>(flags));
    jb.field("minimum_takeoff_altitude_m",
             static_cast<double>(minimum_takeoff_altitude_m));
    jb.raw_field("capabilities", capability_json(flags, has_param_remapping));
    jb.end_object();
    return mg::bridge::ffi::response::ok(jb.str());
}

MGCORE_API char* mg_health_get_all(mg_core_t* core) {
    CHECK_CORE(core);
    auto* cache = get_active_cache(core);
    auto ss_snap = cache->snapshot_sys_status();
    auto hb_snap = cache->snapshot_heartbeat();
    const auto& ss = ss_snap.data;
    const auto& hb = hb_snap.data;
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("connected", is_active_connected(core));
    jb.field("system_id", (int)hb.system_id);
    jb.field("armed", hb.armed);
    jb.field("flight_mode", std::string(hb.flight_mode));
    jb.field("sensors_present", (int64_t)ss.onboard_control_sensors_present);
    jb.field("sensors_enabled", (int64_t)ss.onboard_control_sensors_enabled);
    jb.field("sensors_health", (int64_t)ss.onboard_control_sensors_health);
    jb.field("voltage_battery", (int)ss.battery_voltage);
    jb.field("current_battery", (int)ss.battery_current);
    jb.field("battery_remaining", (int)ss.battery_remaining);
    jb.end_object();
    return to_cstr(jb.str());
}
