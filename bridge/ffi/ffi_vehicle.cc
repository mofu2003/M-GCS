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


MGCORE_API char* mg_vehicle_request_firmware_version(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    if (mav->request_autopilot_version(active_target_sys(core), active_target_comp(core))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}
