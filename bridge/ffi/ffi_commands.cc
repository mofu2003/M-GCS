#define MGCORE_EXPORTS
/**
 * ffi_commands.cc - Mission, Survey, Rally, Log, Terrain, Camera, Video,
 *                   Vehicle, Firmware, SITL, Preflight
 *
 * 任务上传/下载、Rally、围栏协议、测量规划、地形查询、日志、SITL 等。
 *
 * 从早期大文件中按职责拆分而来：
 *   飞行命令      -> ffi_flight_commands.cc
 *   参数文件读写  -> ffi_param_file.cc
 *
 * v0.18：任务 JSON 解析统一使用 cJSON。
 */
#include "ffi_internal.h"
#include "json_builder.h"
#include "json_serializers.h"
#include "mission_json_serializers.h"
#include "core/mission/mission_types.h"

#include <algorithm>

// cJSON 解析深度限制
#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

template <typename Sender>
static char* send_active_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}

template <typename Getter>
static char* export_connected_plan_json(mg_core_t* core, Getter&& getter) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");
    return to_cstr(mg::bridge::mission_plan_json(getter(*mav)));
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

MGCORE_API char* mg_rally_clear(mg_core_t* core) {
    return send_active_request(core, "rally clear send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.rally_clear(sys, comp);
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

MGCORE_API char* mg_fence_clear(mg_core_t* core) {
    return send_active_request(core, "fence clear send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.fence_clear(sys, comp);
        });
}

MGCORE_API char* mg_fence_enable(mg_core_t* core, int enable) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");
    // MAV_CMD_DO_FENCE_ENABLE (4010): param1 = 1 enable, 0 disable
    bool ok_send = mav->send_command_long(
        active_target_sys(core), active_target_comp(core),
        4010, 0,  // MAV_CMD_DO_FENCE_ENABLE
        static_cast<float>(enable),
        0, 0, 0, 0, 0, 0);
    if (ok_send) {
        // Also update local config
        auto& gf = get_active_geofence(core);
        auto cfg = gf.get_config();
        cfg.enabled = (enable != 0);
        gf.set_config(cfg);
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "fence enable send failed");
}

// ================================================================
// Multi-vehicle
// ================================================================

MGCORE_API char* mg_vehicle_list(mg_core_t* core) {
    CHECK_CORE(core);
    auto all = core->vehicles.get_all_vehicles_shared();
    mg::bridge::JsonBuilder jb;
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
        mg::bridge::JsonBuilder obj;
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
        obj.end_object();
        jb.append_json(obj.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_vehicle_count(mg_core_t* core) {
    CHECK_CORE(core);
    mg::bridge::JsonBuilder jb;
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
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("flashing", false);
    jb.field("progress", 0);
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// ffi_api.h aliases
// ================================================================
