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
