#define MGCORE_EXPORTS
/**
 * ffi_commands.cc - Mission, Survey, Rally, Log, Terrain, Camera, Video,
 *                   Vehicle, Firmware, SITL, Preflight
 *
 * 任务上传/下载、Rally、围栏协议、测量规划、地形查询、日志、SITL 等。
 *
 * 从早期大文件中按职责拆分而来：
 *   飞行命令      -> ffi_flight_commands.cc
 *
 * Mission JSON parsing uses cJSON consistently across the bridge.
 */
#include "ffi_internal.h"
#include "core/telemetry/json_builder.h"
#include "core/telemetry/json_serializers.h"
#include "mission_json_serializers.h"
#include "core/mission/mission_types.h"

#include <algorithm>

// cJSON 解析深度限制
#define CJSON_NESTING_LIMIT 10
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

template <typename Getter>
static char* export_connected_plan_json(mg_core_t* core, Getter&& getter) {
    CHECK_CORE(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");
    return to_cstr(mg::bridge::mission_plan_json(
        getter(*mav), ffi_param_profile(core)));
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

MGCORE_API char* mg_rally_get_plan(mg_core_t* core) {
    return export_connected_plan_json(core, [](auto& mav) {
        return mav.rally_get_plan();
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

MGCORE_API char* mg_fence_get_plan(mg_core_t* core) {
    return export_connected_plan_json(core, [](auto& mav) {
        return mav.fence_get_plan();
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
        obj.field("vehicle_type", info.vehicle_type);
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

MGCORE_API char* mg_vehicle_count(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("count", static_cast<int>(core->vehicles.vehicle_count()));
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// Firmware flash
// ================================================================

MGCORE_API char* mg_firmware_flash(mg_core_t* core, const char* firmware_path) {
    CHECK_CORE(core);
    (void)firmware_path;
    return fail(MG_ERROR_NOT_SUPPORTED, "firmware flash not yet implemented");
}

MGCORE_API char* mg_firmware_flash_status(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("flashing", false);
    jb.field("progress", 0);
    jb.end_object();
    return to_cstr(jb.str());
}
