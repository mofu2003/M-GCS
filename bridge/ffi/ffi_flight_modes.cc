#define MGCORE_EXPORTS

#include "ffi_api.h"
#include "ffi_internal.h"

#include <cstring>

#include "core/telemetry/json_builder.h"

#include <optional>
#include <string>

namespace {


} // namespace

MGCORE_API char* mg_flight_get_modes(mg_core_t* core) {
    CHECK_CORE(core);

    auto mavlink = shared_active_mavlink(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    if (!mavlink) {
        jb.field("available", false);
        jb.field("current_mode", "");
        jb.field("armed", false);
        write_heartbeat_freshness(jb, get_active_cache(core));
        jb.raw_field("modes", "[]");
        jb.end_object();
        return to_cstr(jb.str());
    }

    const auto& modes = get_active_flight_modes(core);
    const auto entries = modes.mode_entries();
    const auto status = modes.get_status();

    jb.field("available", !entries.empty());
    jb.field("current_mode", status.current_mode);
    jb.field("armed", status.armed);
    jb.field("base_mode", static_cast<int>(status.base_mode));
    jb.field("custom_mode", static_cast<unsigned long long>(status.custom_mode));
    write_heartbeat_freshness(jb, get_active_cache(core));

    mg::core::telemetry::JsonBuilder array;
    array.begin_array();
    for (const auto& entry : entries) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("id", static_cast<unsigned long long>(entry.custom_mode));
        item.field("name", entry.name);
        item.field("settable", entry.can_be_set);
        item.end_object();
        array.append_json(item.str());
    }
    array.end_array();
    jb.raw_field("modes", array.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_get_mode_mapping(mg_core_t* core) {
    CHECK_CORE(core);

    auto mav = shared_active_mavlink(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    if (!mav) {
        jb.field("available", false);
        jb.raw_field("slots", "[]");
        jb.raw_field("options", "[]");
        jb.end_object();
        return to_cstr(jb.str());
    }

    auto read_param = [mav](const char* name) -> std::optional<float> {
        const auto info = mav->get_param_by_name(name);
        if (!info.has_value()) return std::nullopt;
        return info->value;
    };

    // ArduPilot 用 FLTMODE1..6 保存模式槽。
    const char* prefix = "FLTMODE";
    const bool available = read_param("FLTMODE1").has_value();
    jb.field("available", available);
    jb.field("param_prefix", prefix);

    // 模式选择通道：Plane/Copter 用 FLTMODE_CH，Rover 用 MODE_CH。
    const char* channel_param = nullptr;
    if (read_param("FLTMODE_CH").has_value()) {
        channel_param = "FLTMODE_CH";
    } else if (read_param("MODE_CH").has_value()) {
        channel_param = "MODE_CH";
    }
    if (channel_param) {
        jb.field("channel_param", channel_param);
        jb.field("channel", static_cast<int>(*read_param(channel_param)));
    }

    mg::core::telemetry::JsonBuilder slots;
    slots.begin_array();
    for (int slot = 1; slot <= 6; ++slot) {
        const std::string name = std::string(prefix) + std::to_string(slot);
        const auto value = read_param(name.c_str());
        if (!value.has_value()) continue;
        const int mode_id = static_cast<int>(*value);
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("slot", slot);
        item.field("param", name);
        item.field("value", mode_id);
        // ArduPilot 的 FLTMODEn 就是模式号，可直接用模式表反查。
        item.field("name", get_active_flight_modes(core).decode_mode_name(mode_id));
        item.end_object();
        slots.append_json(item.str());
    }
    slots.end_array();
    jb.raw_field("slots", slots.str());

    // Copter/Rover 的 Simple / SuperSimple 位掩码（Plane 没有）。
    for (const char* simple_param : {"SIMPLE", "SUPER_SIMPLE"}) {
        const auto value = read_param(simple_param);
        if (!value.has_value()) continue;
        jb.field(simple_param, static_cast<int>(*value));
    }

    mg::core::telemetry::JsonBuilder options;
    options.begin_array();
    for (const auto& entry : get_active_flight_modes(core).mode_entries()) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("value", static_cast<unsigned long long>(entry.custom_mode));
        item.field("name", entry.name);
        item.end_object();
        options.append_json(item.str());
    }
    options.end_array();
    jb.raw_field("options", options.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_flight_set_mode(mg_core_t* core, const char* mode_name) {
    CHECK_CORE(core);
    if (!mode_name || mode_name[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "mode name is required");
    }
    auto mav = shared_active_mavlink(core);
    if (!mav) {
        return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle connected");
    }

    const std::string requested(mode_name);
    auto& modes = get_active_flight_modes(core);
    uint8_t base_mode = 0;
    uint32_t custom_mode = 0;
    if (!modes.resolve_mode_request(requested, base_mode, custom_mode)) {
        for (const auto& entry : modes.mode_entries()) {
            if (entry.name == requested) {
                return fail(MG_ERROR_INVALID_PARAM,
                            "flight mode is not selectable on this vehicle");
            }
        }
        return fail(MG_ERROR_INVALID_PARAM, "unknown flight mode for this vehicle");
    }

    // S2：命令结果以 COMMAND_ACK 为准。这里等 ACK 再回成功，避免把“已发出”
    // 当成“已切换”。主界面走非阻塞的 submit/poll，不经过这个同步入口。
    char* acked = ffi_cmd_result_queued(
        mav, MAV_CMD_DO_SET_MODE,
        active_target_sys(core), active_target_comp(core),
        static_cast<float>(base_mode), static_cast<float>(custom_mode),
        0, 0, 0, 0, 0);
    if (!acked || strcmp(acked, ok_json()) != 0) {
        return acked;  // 拒绝、超时或发送失败：原样传播
    }
    mg_free_string(acked);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("mode", requested);
    jb.field("custom_mode", static_cast<unsigned long long>(custom_mode));
    jb.end_object();
    return to_cstr(jb.str());
}
