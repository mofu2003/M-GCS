#ifndef MGCORE_MISSION_JSON_SERIALIZERS_H
#define MGCORE_MISSION_JSON_SERIALIZERS_H

#include "json_builder.h"
#include "core/mission/mission_types.h"

namespace mg::bridge {

inline std::string mission_item_json(const mg::core::mission::MissionItem& item) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("seq",          static_cast<int>(item.seq));
    jb.field("frame",        static_cast<int>(item.frame));
    jb.field("frame_name",   mg::core::mission::frame_name(item.frame));
    jb.field("command",      static_cast<int>(item.command));
    jb.field("command_name", mg::core::mission::command_name(item.command));
    jb.field("current",      static_cast<int>(item.current));
    jb.field("autocontinue", static_cast<int>(item.autocontinue));
    jb.field("param1",       item.param1);
    jb.field("param2",       item.param2);
    jb.field("param3",       item.param3);
    jb.field("param4",       item.param4);
    jb.field("x",            item.x);
    jb.field("y",            item.y);
    jb.field("z",            item.z);
    if (mg::core::mission::is_global_coordinate_frame(item.frame) &&
        mg::core::mission::mission_command_uses_location(item.command)) {
        jb.field("lat", static_cast<double>(item.x) / 1e7);
        jb.field("lon", static_cast<double>(item.y) / 1e7);
    }
    jb.end_object();
    return jb.str();
}

inline std::string mission_plan_json(const mg::core::mission::MissionPlan& plan) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("valid",         plan.valid);
    jb.field("count",         static_cast<int>(plan.items.size()));
    jb.field("current_index", static_cast<int>(plan.current_index));
    JsonBuilder arr;
    arr.begin_array();
    for (const auto& item : plan.items) {
        arr.append_json(mission_item_json(item));
    }
    arr.end_array();
    jb.raw_field("items", arr.str());
    jb.end_object();
    return jb.str();
}

inline std::string mission_progress_json(const mg::core::mission::MissionTransferProgress& p) {
    JsonBuilder jb;
    jb.begin_object();
    const char* state_str = "idle";
    switch (p.state) {
        case mg::core::mission::MissionState::Idle:        state_str = "idle"; break;
        case mg::core::mission::MissionState::Downloading: state_str = "downloading"; break;
        case mg::core::mission::MissionState::Uploading:   state_str = "uploading"; break;
        case mg::core::mission::MissionState::WaitingAck:  state_str = "waiting_ack"; break;
    }
    jb.field("state",    state_str);
    jb.field("total",    static_cast<int>(p.total));
    jb.field("received", static_cast<int>(p.received));
    jb.field("sent",     static_cast<int>(p.sent));
    jb.field("result",   static_cast<int>(p.result));
    jb.field("error",    p.error);
    int pct = 0;
    if (p.total > 0) {
        if (p.received > 0) pct = (p.received * 100) / p.total;
        else if (p.sent > 0) pct = (p.sent * 100) / p.total;
    }
    jb.field("progress_pct", pct);
    jb.end_object();
    return jb.str();
}

} // namespace mg::bridge

#endif /* MGCORE_MISSION_JSON_SERIALIZERS_H */
