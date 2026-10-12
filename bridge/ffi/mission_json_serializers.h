#ifndef MGCORE_MISSION_JSON_SERIALIZERS_H
#define MGCORE_MISSION_JSON_SERIALIZERS_H

#include "core/telemetry/json_builder.h"
#include "core/mission/mission_types.h"

namespace mg::bridge {

/// `firmware_profile` 使用 `mg::bridge::ffi::ffi_param_profile()` 的取值
/// （`"ardupilot"`/归一化机型/空串）。空串等价 ArduPilot 语义，
/// 用于没有活动会话（离线文档）或本地生成航点的场景。
inline std::string mission_item_json(
    const mg::core::mission::MissionItem& item,
    const std::string& firmware_profile = std::string()) {
    mg::core::telemetry::JsonBuilder jb;
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

inline std::string mission_plan_json(
    const mg::core::mission::MissionPlan& plan,
    const std::string& firmware_profile = std::string()) {
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid",         plan.valid);
    jb.field("count",         static_cast<int>(plan.items.size()));
    jb.field("current_index", static_cast<int>(plan.current_index));
    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    for (const auto& item : plan.items) {
        arr.append_json(mission_item_json(item, firmware_profile));
    }
    arr.end_array();
    jb.raw_field("items", arr.str());
    jb.end_object();
    return jb.str();
}
} // namespace mg::bridge

#endif /* MGCORE_MISSION_JSON_SERIALIZERS_H */
