#define MGCORE_EXPORTS
/**
 * Mission protocol FFI entrypoints.
 *
 * Keeps mission transfer commands near mission presentation/upload shaping
 * instead of leaving them in the historical catch-all command module.
 */
#include "ffi_internal.h"
#include "json_builder.h"
#include "json_serializers.h"
#include "mission_json_serializers.h"
#include "mission_json_parser.h"

#include <algorithm>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace {

template <typename Sender>
char* send_active_mission_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    mg::core::platform::LockGuard lk(core->mission_mtx);
    auto* mav = get_active_mavlink(core);
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}

} // namespace

MGCORE_API char* mg_mission_download(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return mav->mission_download(active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_mission_upload(mg_core_t* core, const char* j) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    mg::core::platform::LockGuard lk(core->mission_mtx);
    if (!j || j[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "empty mission data");

    cJSON* root = cJSON_Parse(j);
    if (!root) return fail(MG_ERROR_INVALID_PARAM, "invalid JSON");
    if (!cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "expected JSON array of mission items");
    }

    mg::core::mission::MissionPlan plan;
    const int array_size = cJSON_GetArraySize(root);
    for (int i = 0; i < array_size; i++) {
        cJSON* item_json = cJSON_GetArrayItem(root, i);
        if (!cJSON_IsObject(item_json)) continue;

        mg::core::mission::MissionItem item{};
        if (mg::bridge::mission_parser::parse_mission_item(item_json, item)) {
            plan.items.push_back(item);
        }
    }
    cJSON_Delete(root);

    if (plan.items.empty()) return fail(MG_ERROR_INVALID_PARAM, "no valid mission items found in JSON");

    std::sort(plan.items.begin(), plan.items.end(),
        [](const auto& a, const auto& b) { return a.seq < b.seq; });
    for (size_t i = 0; i < plan.items.size(); i++) {
        plan.items[i].seq = static_cast<uint16_t>(i);
    }
    plan = ffi_prepare_mission_upload(core, plan);

    auto* mav = get_active_mavlink(core);
    if (mav && mav->mission_upload(active_target_sys(core), active_target_comp(core), plan)) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_mission_clear(mg_core_t* core) {
    return send_active_mission_request(core, "send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.mission_clear(sys, comp);
        });
}

MGCORE_API char* mg_mission_set_current(mg_core_t* core, int seq) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (seq < 0) return fail(MG_ERROR_INVALID_PARAM, "seq must be non-negative");
    seq = mg::bridge::ffi::prepare_mission_current_index(core, seq);
    return send_active_mission_request(core, "send failed",
        [seq](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.mission_set_current(sys, comp, static_cast<uint16_t>(seq));
        });
}

MGCORE_API char* mg_mission_get_plan(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    auto plan = mav ? ffi_present_mission_plan(core, mav->mission_get_plan()) : mg::core::mission::MissionPlan{};
    return to_cstr(mg::bridge::mission_plan_json(plan));
}

MGCORE_API char* mg_mission_get_progress(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    auto prog = mav ? mg::bridge::ffi::present_mission_progress(core, mav->mission_get_progress())
                    : mg::core::mission::MissionTransferProgress{};
    return to_cstr(mg::bridge::mission_progress_json(prog));
}

MGCORE_API char* mg_mission_get_current(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    auto raw_plan = mav ? mav->mission_get_plan() : mg::core::mission::MissionPlan{};
    auto plan = ffi_present_mission_plan(core, raw_plan);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("index", mg::bridge::ffi::present_mission_current_index(core, raw_plan));
    jb.field("count", (int)plan.items.size());
    jb.end_object();
    return to_cstr(jb.str());
}
