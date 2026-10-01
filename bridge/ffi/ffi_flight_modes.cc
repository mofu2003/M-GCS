#define MGCORE_EXPORTS

#include "ffi_api.h"
#include "ffi_internal.h"

#include "json_builder.h"

namespace {

using mg::bridge::JsonBuilder;

} // namespace

MGCORE_API char* mg_flight_get_modes(mg_core_t* core) {
    CHECK_CORE(core);

    auto* mavlink = get_active_mavlink(core);
    JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);

    if (!mavlink) {
        jb.field("available", false);
        jb.field("current_mode", "");
        jb.field("armed", false);
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

    JsonBuilder array;
    array.begin_array();
    for (const auto& entry : entries) {
        JsonBuilder item;
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

MGCORE_API char* mg_flight_set_mode(mg_core_t* core, const char* mode_name) {
    CHECK_CORE(core);
    if (!mode_name || mode_name[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "mode name is required");
    }
    if (!get_active_mavlink(core)) {
        return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle connected");
    }

    const std::string requested(mode_name);
    auto& modes = get_active_flight_modes(core);
    for (const auto& entry : modes.mode_entries()) {
        if (entry.name != requested) continue;
        if (!entry.can_be_set) {
            return fail(MG_ERROR_INVALID_PARAM,
                        "flight mode is not selectable on this vehicle");
        }
        if (!modes.set_mode(requested)) {
            return fail(MG_ERROR_SEND_FAILED, "flight mode change failed to send");
        }
        JsonBuilder jb;
        jb.begin_object();
        jb.field("success", true);
        jb.field("mode", requested);
        jb.field("custom_mode", static_cast<unsigned long long>(entry.custom_mode));
        jb.end_object();
        return to_cstr(jb.str());
    }
    return fail(MG_ERROR_INVALID_PARAM, "unknown flight mode for this vehicle");
}
