#define MGCORE_EXPORTS
/**
 * ffi_commands.cc - 围栏/Rally 回读与车辆清单
 *
 * 只剩三个导出：mg_fence_download、mg_rally_download、mg_vehicle_list。
 * 早期的大文件按职责拆分后，其余能力（测绘、地形、SITL、固件刷写等）
 * 已在 0.2.x 的清理中删除，见 CHANGELOG。
 */
#include "ffi_internal.h"
#include "core/telemetry/json_builder.h"
#include "core/telemetry/json_serializers.h"
#include "mission_json_serializers.h"
#include "core/mission/mission_types.h"

#include <algorithm>

// cJSON 解析深度限制
#include "cJSON.h"

template <typename Sender>
static char* send_active_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}
// ================================================================
// 任务相关操作：JSON 解析使用 cJSON
// ================================================================

// ================================================================
// Rally Points (MAVLink protocol)
// ================================================================

MGCORE_API char* mg_rally_download(mg_core_t* core) {
    return send_active_request(core, "rally download send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.rally_download(sys, comp);
        });
}


// ================================================================
// Geofence Protocol (MAVLink MISSION_TYPE_FENCE)
// ================================================================

MGCORE_API char* mg_fence_download(mg_core_t* core) {
    return send_active_request(core, "fence download send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.fence_download(sys, comp);
        });
}


// ================================================================
// Multi-vehicle
// ================================================================

MGCORE_API char* mg_vehicle_list(mg_core_t* core) {
    CHECK_CORE(core);
    auto all = core->vehicles.get_all_vehicles_shared();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();
    for (const auto& v : all) {
        auto& info = v->info();
        const auto heartbeat = v->cache().snapshot_heartbeat().data;
        const auto battery = v->cache().snapshot_battery().data;
        const auto gps = v->cache().snapshot_gps().data;
        const auto link_statuses = v->link_manager().statuses();
        size_t healthy_links = 0;
        uint64_t primary_heartbeat_age_ms = 0;
        for (const auto& status : link_statuses) {
            if (!status.comm_lost) ++healthy_links;
            if (status.primary) primary_heartbeat_age_ms = status.heartbeat_age_ms;
        }
        mg::core::telemetry::JsonBuilder obj;
        obj.begin_object();
        obj.field("vehicle_id", info.vehicle_id);
        obj.field("system_id", (int)info.system_id);
        obj.field("name", info.name);
        obj.field("firmware_type", info.firmware_type);
        auto vehicle_type = info.vehicle_type;
        const auto mav = v->shared_mavlink();
        if (mav) {
            const auto hb = mav->latest_heartbeat();
            if (hb.has_value()) {
                mg::core::firmware::FirmwareAdapter adapter;
                adapter.detect(hb->autopilot_type, hb->vehicle_type,
                               ffi_quadplane_enable(mav));
                vehicle_type = adapter.normalized_vehicle_name();
            }
        }
        obj.field("vehicle_type", vehicle_type);
        obj.field("endpoint", info.endpoint);
        obj.field("connected", v->is_connected());
        obj.field("armed", heartbeat.armed);
        obj.field("flight_mode", std::string(heartbeat.flight_mode));
        obj.field("battery_remaining_pct",
                  static_cast<int>(battery.battery_remaining));
        obj.field("gps_fix_type", static_cast<int>(gps.fix_type));
        obj.field("gps_satellites", static_cast<int>(gps.satellites_visible));
        obj.field("link_count", static_cast<int>(link_statuses.size()));
        obj.field("healthy_link_count", static_cast<int>(healthy_links));
        obj.field("communication_lost", v->link_manager().communication_lost());
        obj.field("primary_heartbeat_age_ms",
                  static_cast<int64_t>(primary_heartbeat_age_ms));
        obj.field("heartbeat_fresh",
                  v->is_connected() && v->conn().heartbeat_alive(3000));
        obj.end_object();
        jb.append_json(obj.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}


// ================================================================
// Firmware flash
// ================================================================
