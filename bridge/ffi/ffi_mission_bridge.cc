#include "ffi_mission_bridge.h"

#include "ffi_vehicle_session.h"
#include "common/mavlink.h"

namespace mg::bridge::ffi {

namespace {

bool send_home_position_to_vehicle(mg_core_t* core) {
    auto* mav = resolve_vehicle_session(core).mavlink;
    auto hb = mav ? mav->latest_heartbeat() : std::nullopt;
    return hb.has_value() && hb->autopilot_type == MAV_AUTOPILOT_ARDUPILOTMEGA;
}

bool looks_like_ardupilot_home_item(const mg::core::mission::MissionItem& item) {
    return item.seq == 0 &&
           item.command == MAV_CMD_NAV_WAYPOINT &&
           item.frame == MAV_FRAME_GLOBAL;
}

mg::core::mission::MissionItem build_ardupilot_home_item(mg_core_t* core) {
    mg::core::mission::MissionItem home{};
    home.seq = 0;
    home.frame = MAV_FRAME_GLOBAL;
    home.command = MAV_CMD_NAV_WAYPOINT;
    home.current = 0;
    home.autocontinue = 1;
    home.mission_type = MAV_MISSION_TYPE_MISSION;

    auto* mav = resolve_vehicle_session(core).mavlink;
    if (!mav) return home;

    const auto hp = mav->latest_home_position();
    if (hp && (hp->latitude != 0 || hp->longitude != 0 || hp->altitude != 0)) {
        home.x = hp->latitude;
        home.y = hp->longitude;
        home.z = static_cast<float>(hp->altitude / 1000.0);
        return home;
    }

    const auto gp = mav->latest_global_position();
    if (gp) {
        home.x = gp->lat;
        home.y = gp->lon;
        home.z = static_cast<float>(gp->alt / 1000.0);
    }
    return home;
}

} // namespace

mg::core::mission::MissionPlan present_mission_plan(
    mg_core_t* core,
    const mg::core::mission::MissionPlan& raw_plan) {
    if (!send_home_position_to_vehicle(core) ||
        raw_plan.items.empty() ||
        !looks_like_ardupilot_home_item(raw_plan.items.front())) {
        return raw_plan;
    }

    auto plan = raw_plan;
    plan.items.erase(plan.items.begin());
    for (size_t i = 0; i < plan.items.size(); ++i) {
        plan.items[i].seq = static_cast<uint16_t>(i);
    }
    plan.current_index =
        raw_plan.current_index > 0 ? static_cast<uint16_t>(raw_plan.current_index - 1) : 0;
    return plan;
}

mg::core::mission::MissionPlan prepare_mission_upload(
    mg_core_t* core,
    const mg::core::mission::MissionPlan& client_plan) {
    if (!send_home_position_to_vehicle(core) || client_plan.items.empty()) {
        return client_plan;
    }

    auto plan = client_plan;
    if (!looks_like_ardupilot_home_item(plan.items.front())) {
        plan.items.insert(plan.items.begin(), build_ardupilot_home_item(core));
    }

    for (size_t i = 0; i < plan.items.size(); ++i) {
        plan.items[i].seq = static_cast<uint16_t>(i);
    }
    plan.current_index = static_cast<uint16_t>(client_plan.current_index + 1);
    return plan;
}

mg::core::mission::MissionTransferProgress present_mission_progress(
    mg_core_t* core,
    const mg::core::mission::MissionTransferProgress& raw_progress) {
    auto progress = raw_progress;
    if (send_home_position_to_vehicle(core) && progress.total > 0) {
        progress.total -= 1;
        if (progress.received > 0) progress.received -= 1;
        if (progress.sent > 0) progress.sent -= 1;
    }
    return progress;
}

int present_mission_current_index(
    mg_core_t* core,
    const mg::core::mission::MissionPlan& raw_plan) {
    const auto plan = present_mission_plan(core, raw_plan);
    return static_cast<int>(plan.current_index);
}

int prepare_mission_current_index(mg_core_t* core, int client_visible_index) {
    return send_home_position_to_vehicle(core) ? client_visible_index + 1 : client_visible_index;
}

} // namespace mg::bridge::ffi
