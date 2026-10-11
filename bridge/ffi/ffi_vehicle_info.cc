#define MGCORE_EXPORTS
/**
 * Vehicle information and passive status FFI entrypoints.
 */
#include "ffi_internal.h"

MGCORE_API char* mg_vehicle_get_flight_time(mg_core_t* core) {
    CHECK_CORE(core);
    auto* vehicle = get_active_vehicle(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("flight_time_s", vehicle ? static_cast<int>(vehicle->flight_time_s()) : 0);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_vehicle_get_adsb_traffic(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    arr.end_array();
    jb.raw_field("vehicles", arr.str());
    jb.field("count", 0);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_vehicle_get_distance_sensors(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    mg::core::telemetry::JsonBuilder arr;
    arr.begin_array();
    arr.end_array();
    jb.raw_field("sensors", arr.str());
    jb.end_object();
    return to_cstr(jb.str());
}
