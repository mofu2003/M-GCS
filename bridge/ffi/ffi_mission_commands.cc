#define MGCORE_EXPORTS
/**
 * Mission protocol FFI entrypoints.
 *
 * Keeps mission transfer commands near mission presentation/upload shaping
 * instead of leaving them in the historical catch-all command module.
 */
#include "ffi_internal.h"
#include "core/telemetry/json_builder.h"
#include "core/telemetry/json_serializers.h"
#include "mission_json_serializers.h"

#include <algorithm>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

namespace {

template <typename Sender>
char* send_active_mission_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    mg::core::platform::LockGuard lk(core->mission_mtx);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}

} // namespace

MGCORE_API char* mg_mission_download(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto mav = shared_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "disconnected");
    return mav->mission_download(active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, "send failed");
}


MGCORE_API char* mg_mission_clear(mg_core_t* core) {
    return send_active_mission_request(core, "send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.mission_clear(sys, comp);
        });
}





MGCORE_API char* mg_mission_get_execution_progress(mg_core_t* core) {
    CHECK_CORE(core);
    auto mav = shared_active_mavlink(core);
    auto raw_plan = mav ? mav->mission_get_plan() : mg::core::mission::MissionPlan{};
    auto plan = ffi_present_mission_plan(core, raw_plan);
    const auto progress = mav
        ? mav->mission_get_execution_progress()
        : mg::core::mavlink::MavlinkAdapter::MissionExecutionProgress{};

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("valid", progress.valid);
    jb.field("current_index",
             mg::bridge::ffi::present_mission_current_index(core, raw_plan));
    jb.field("current_seq", static_cast<int>(progress.current_seq));
    jb.field("total", static_cast<int>(plan.items.size()));
    jb.field("reached_count", static_cast<int>(progress.reached_count));
    jb.field("last_reached_seq", static_cast<int>(progress.last_reached_seq));
    // reached_end：航段飞完（最后一项到达）；
    // finished：任务完成，按 QGC FlyViewMissionCompleteDialog 的定义——
    // 曾经在任务模式下解锁飞行，之后上锁。
    jb.field("reached_end", progress.reached_end);
    jb.field("finished", get_active_flight_modes(core).mission_fly_completed());
    // 中止：解锁飞行中离开过任务模式，而且最后一项没到达。带上"未到达"这一条是为了
    // 区分正常收尾——飞控跑完任务后自己也会切 RTL/降落，那时 reached_end 已经是 true。
    jb.field("aborted",
             !progress.reached_end && progress.valid &&
                 get_active_flight_modes(core).mission_mode_exited_while_armed());
    jb.end_object();
    return to_cstr(jb.str());
}
