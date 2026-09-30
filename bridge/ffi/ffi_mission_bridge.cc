#include "ffi_mission_bridge.h"

#include "ffi_vehicle_session.h"
#include "ardupilotmega/mavlink.h"

#include <cmath>

namespace mg::bridge::ffi {

bool mission_uses_home_item(mg_core_t* core) {
    auto* mav = resolve_vehicle_session(core).mavlink;
    auto hb = mav ? mav->latest_heartbeat() : std::nullopt;
    return hb.has_value() && hb->autopilot_type == MAV_AUTOPILOT_ARDUPILOTMEGA;
}

namespace {

bool looks_like_ardupilot_home_item(const mg::core::mission::MissionItem& item) {
    return item.seq == 0 &&
           item.command == MAV_CMD_NAV_WAYPOINT &&
           item.frame == MAV_FRAME_GLOBAL;
}

mg::core::mission::MissionItem build_ardupilot_home_item(
    mg_core_t* core,
    const mg::core::mission::cad::MissionHome* planned_home) {
    mg::core::mission::MissionItem home{};
    home.seq = 0;
    home.frame = MAV_FRAME_GLOBAL;
    home.command = MAV_CMD_NAV_WAYPOINT;
    home.current = 0;
    home.autocontinue = 1;
    home.mission_type = MAV_MISSION_TYPE_MISSION;

    if (planned_home && planned_home->set &&
        std::isfinite(planned_home->latitude) &&
        std::isfinite(planned_home->longitude)) {
        home.x = static_cast<int32_t>(std::llround(planned_home->latitude * 1e7));
        home.y = static_cast<int32_t>(std::llround(planned_home->longitude * 1e7));
        home.z = planned_home->altitude_m;
        return home;
    }

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
    const mg::core::mission::MissionPlan& raw_plan,
    mg::core::mission::cad::MissionHome* home,
    bool* home_present) {
    if (home_present) *home_present = false;
    if (!mission_uses_home_item(core) ||
        raw_plan.items.empty() ||
        !looks_like_ardupilot_home_item(raw_plan.items.front())) {
        return raw_plan;
    }

    if (home_present) *home_present = true;
    const auto& raw_home = raw_plan.items.front();
    if (home && (raw_home.x != 0 || raw_home.y != 0)) {
        home->set = true;
        home->latitude = static_cast<double>(raw_home.x) / 1e7;
        home->longitude = static_cast<double>(raw_home.y) / 1e7;
        home->altitude_m = raw_home.z;
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
    const mg::core::mission::MissionPlan& client_plan,
    const mg::core::mission::cad::MissionHome* planned_home) {
    if (!mission_uses_home_item(core) || client_plan.items.empty()) {
        return client_plan;
    }

    auto plan = client_plan;
    const bool has_home_item = looks_like_ardupilot_home_item(plan.items.front());
    if (has_home_item && planned_home && planned_home->set) {
        plan.items.front() = build_ardupilot_home_item(core, planned_home);
    } else if (!has_home_item) {
        plan.items.insert(
            plan.items.begin(),
            build_ardupilot_home_item(core, planned_home));
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
    if (mission_uses_home_item(core) && progress.total > 0) {
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
    return mission_uses_home_item(core) ? client_visible_index + 1 : client_visible_index;
}

} // namespace mg::bridge::ffi
