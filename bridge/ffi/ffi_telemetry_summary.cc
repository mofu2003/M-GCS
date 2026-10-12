#define MGCORE_EXPORTS
/**
 * ffi_telemetry_summary.cc - Bridge API response normalized wrappers
 *
 * 遥测响应包装器：从缓存读取数据，并统一输出单位
 * （伏特、安培、度、米、米每秒等）。
 *
 * 所有函数优先读取当前活动飞控的 mg::core::telemetry::TelemetryCache；
 * 只有在未选中任何飞控时，才退回 fallback_vehicle 的缓存。
 *
 * 单位归一化规则：
 *   电压：V，而不是 mV
 *   电流：A，而不是 cA
 *   经纬度：度，而不是 degE7
 *   高度：米，而不是毫米
 *   速度：m/s
 *   航向：度，而不是 centidegrees
 *   电量：百分比（0-100，未知时为 -1）
 */
#include "ffi_response.h"
#include "core/telemetry/json_serializers.h"




// ================================================================
// 1. Connection state
// ================================================================

// ================================================================
// 2. Vehicle summary
// ================================================================
MGCORE_API char* mg_vehicle_get_summary(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"system_id":0,"component_id":0,"vehicle_type":0,"autopilot_type":0,"armed":false,"base_mode":0,"custom_mode":0,"system_status":0,"flight_mode":"","heartbeat_fresh":false,"heartbeat_age_ms":-1})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto hb_snap = cache.snapshot_heartbeat();
            const auto& hb = hb_snap.data;
            data.field("system_id",      (int)hb.system_id);
            data.field("component_id",   (int)hb.component_id);
            data.field("vehicle_type",   (int)hb.vehicle_type);
            data.field("autopilot_type", (int)hb.autopilot_type);
            data.field("armed",          hb.armed);
            data.field("base_mode",      (int)hb.base_mode);
            data.field("custom_mode",    (int32_t)hb.custom_mode);
            data.field("system_status",  (int)hb.system_status);
            data.field("flight_mode",    std::string(hb.flight_mode));
            write_heartbeat_freshness(data, &cache);
            const auto vehicle_name = cache.snapshot_vehicle_name();
            if (!vehicle_name.empty()) data.field("vehicle_name", vehicle_name);
            const auto board_name = cache.snapshot_board_name();
            if (!board_name.empty()) data.field("board_name", board_name);
            const auto product_name = cache.snapshot_product_name();
            if (!product_name.empty()) data.field("product_name", product_name);
            const auto usb_board_name = cache.snapshot_usb_board_name();
            if (!usb_board_name.empty()) data.field("usb_board_name", usb_board_name);
        });
}
