#define MGCORE_EXPORTS
/**
 * ffi_mission_management.cc - Mission management and flight mode control FFI
 *
 * 任务管理：增删改查航点、重排序、飞行模式控制。
 *
 * NOTE: Each FFI mutation function (add, update, remove, move) uses a local
 * MissionManager with a read-modify-upload cycle. If the caller
 * invokes multiple mission mutations concurrently, a lost-update race occurs.
 * The caller MUST serialize mission mutations (e.g., await each call before
 * issuing the next).
 */
#include "ffi_internal.h"
#include "json_serializers.h"
#include "core/mission/mission_manager.h"
#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

// Safe helper: get mission plan from mavlink (null-safe)
static mg::core::mission::MissionPlan safe_get_plan(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    return mav ? ffi_present_mission_plan(core, mav->mission_get_plan()) : mg::core::mission::MissionPlan{};
}

// Safe helper: upload mission plan (null-safe)
static bool safe_upload_plan(mg_core_t* core, const mg::core::mission::MissionPlan& plan) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return false;
    return mav->mission_upload(active_target_sys(core), active_target_comp(core),
                               ffi_prepare_mission_upload(core, plan));
}

template <typename Fn>
static char* mutate_mission_plan(mg_core_t* core, const char* invalid_reason, Fn&& fn) {
    CHECK_CORE(core);
    mg::core::platform::LockGuard lk(core->mission_mtx);

    mg::core::mission::MissionManager mgr;
    mgr.set_plan(safe_get_plan(core));
    if (!fn(mgr)) {
        return fail(MG_ERROR_INVALID_PARAM, invalid_reason);
    }

    const auto new_plan = mgr.get_plan();
    if (!safe_upload_plan(core, new_plan)) {
        return fail(MG_ERROR_SEND_FAILED, "mission upload failed");
    }
    return nullptr;
}

// ================================================================
// Mission Management FFI
// ================================================================


MGCORE_API char* mg_mission_add_item(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    cJSON* root = cJSON_Parse(json);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");

    mg::core::mission::MissionItem item{};
    item.frame = static_cast<uint8_t>(mg::core::mission::CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(mg::core::mission::NavCommand::NavWaypoint);
    item.autocontinue = 1;

    cJSON* lat = cJSON_GetObjectItemCaseSensitive(root, "lat");
    cJSON* lon = cJSON_GetObjectItemCaseSensitive(root, "lon");
    cJSON* alt = cJSON_GetObjectItemCaseSensitive(root, "alt");
    cJSON* cmd = cJSON_GetObjectItemCaseSensitive(root, "command");
    cJSON* hold = cJSON_GetObjectItemCaseSensitive(root, "hold_time");
    cJSON* frame = cJSON_GetObjectItemCaseSensitive(root, "frame");

    if (lat && cJSON_IsNumber(lat)) item.x = static_cast<int32_t>(lat->valuedouble * 1e7);
    if (lon && cJSON_IsNumber(lon)) item.y = static_cast<int32_t>(lon->valuedouble * 1e7);
    if (alt && cJSON_IsNumber(alt)) item.z = static_cast<float>(alt->valuedouble);
    if (cmd && cJSON_IsNumber(cmd)) item.command = static_cast<uint16_t>(cmd->valueint);
    if (hold && cJSON_IsNumber(hold)) item.param1 = static_cast<float>(hold->valuedouble);
    if (frame && cJSON_IsNumber(frame)) item.frame = static_cast<uint8_t>(frame->valueint);

    cJSON_Delete(root);

    char* error = mutate_mission_plan(core, "failed to add item", [&](mg::core::mission::MissionManager& mgr) {
        mgr.add_item(item);
        return true;
    });
    if (error) return error;

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("index", -1);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_update_item(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    cJSON* root = cJSON_Parse(json);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");

    cJSON* idx = cJSON_GetObjectItemCaseSensitive(root, "index");
    if (!idx || !cJSON_IsNumber(idx)) {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "missing index");
    }
    int index = idx->valueint;

    mg::core::mission::MissionItem item{};
    item.frame = static_cast<uint8_t>(mg::core::mission::CoordFrame::GlobalRelAlt);
    item.command = static_cast<uint16_t>(mg::core::mission::NavCommand::NavWaypoint);
    item.autocontinue = 1;
    item.seq = static_cast<uint16_t>(index);

    cJSON* lat = cJSON_GetObjectItemCaseSensitive(root, "lat");
    cJSON* lon = cJSON_GetObjectItemCaseSensitive(root, "lon");
    cJSON* alt = cJSON_GetObjectItemCaseSensitive(root, "alt");
    cJSON* cmd = cJSON_GetObjectItemCaseSensitive(root, "command");

    if (lat && cJSON_IsNumber(lat)) item.x = static_cast<int32_t>(lat->valuedouble * 1e7);
    if (lon && cJSON_IsNumber(lon)) item.y = static_cast<int32_t>(lon->valuedouble * 1e7);
    if (alt && cJSON_IsNumber(alt)) item.z = static_cast<float>(alt->valuedouble);
    if (cmd && cJSON_IsNumber(cmd)) item.command = static_cast<uint16_t>(cmd->valueint);

    cJSON_Delete(root);

    char* error = mutate_mission_plan(core, "failed to update item", [&](mg::core::mission::MissionManager& mgr) {
        mgr.update_item(index, item);
        return true;
    });
    return error ? error : ok();
}

MGCORE_API char* mg_mission_remove_item(mg_core_t* core, int index) {
    CHECK_CORE(core);
    if (index < 0) return fail(MG_ERROR_INVALID_PARAM, "invalid index");

    char* error = mutate_mission_plan(core, "failed to remove item", [&](mg::core::mission::MissionManager& mgr) {
        mgr.remove_item(static_cast<size_t>(index));
        return true;
    });
    return error ? error : ok();
}

MGCORE_API char* mg_mission_move_item(mg_core_t* core, int from_index, int to_index) {
    CHECK_CORE(core);
    if (from_index < 0 || to_index < 0) return fail(MG_ERROR_INVALID_PARAM, "invalid index");

    char* error = mutate_mission_plan(core, "failed to move item", [&](mg::core::mission::MissionManager& mgr) {
        mgr.move_item(static_cast<size_t>(from_index), static_cast<size_t>(to_index));
        return true;
    });
    return error ? error : ok();
}

MGCORE_API char* mg_mission_validate(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::mission::MissionManager mgr;
    mgr.set_plan(safe_get_plan(core));

    auto result = mgr.validate();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid", result.valid);

    mg::bridge::JsonBuilder warnings_arr;
    warnings_arr.begin_array();
    for (const auto& w : result.warnings) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("msg", w);
        item.end_object();
        warnings_arr.append_json(item.str());
    }
    warnings_arr.end_array();
    jb.raw_field("warnings", warnings_arr.str());

    mg::bridge::JsonBuilder errors_arr;
    errors_arr.begin_array();
    for (const auto& e : result.errors) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("msg", e);
        item.end_object();
        errors_arr.append_json(item.str());
    }
    errors_arr.end_array();
    jb.raw_field("errors", errors_arr.str());

    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_get_stats(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::mission::MissionManager mgr;
    mgr.set_plan(safe_get_plan(core));

    auto stats = mgr.calculate_stats();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("total_distance_m", stats.total_distance_m);
    jb.field("total_altitude_change_m", stats.total_altitude_change_m);
    jb.field("estimated_time_s", stats.estimated_time_s);
    jb.field("min_altitude_m", stats.min_altitude_m);
    jb.field("max_altitude_m", stats.max_altitude_m);
    jb.field("waypoint_count", static_cast<int>(stats.waypoint_count));
    jb.field("total_item_count", static_cast<int>(stats.total_item_count));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_export_json(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::mission::MissionManager mgr;
    mgr.set_plan(safe_get_plan(core));

    return to_cstr(mgr.export_json());
}

MGCORE_API char* mg_mission_import_json(mg_core_t* core, const char* json) {
    CHECK_CORE(core);
    if (!json || json[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null json");

    char* error = mutate_mission_plan(core, "failed to parse JSON", [&](mg::core::mission::MissionManager& mgr) {
        return mgr.import_json(json);
    });
    if (error) return error;

    return ok();
}

MGCORE_API char* mg_mission_export_csv(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::mission::MissionManager mgr;
    mgr.set_plan(safe_get_plan(core));

    return to_cstr(mgr.export_csv());
}

MGCORE_API char* mg_mission_import_csv(mg_core_t* core, const char* csv) {
    CHECK_CORE(core);
    if (!csv || csv[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null csv");

    char* error = mutate_mission_plan(core, "failed to parse CSV", [&](mg::core::mission::MissionManager& mgr) {
        return mgr.import_csv(csv);
    });
    if (error) return error;

    return ok();
}

MGCORE_API char* mg_mission_get_templates(mg_core_t* core) {
    CHECK_CORE(core);

    auto templates = mg::core::mission::MissionManager::get_templates();

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& t : templates) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", t.name);
        item.field("description", t.description);
        item.field("item_count", static_cast<int>(t.items.size()));
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}
