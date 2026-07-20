#define MGCORE_EXPORTS
/**
 * Terrain query FFI entrypoints.
 */
#include "ffi_internal.h"
#include "core/terrain/terrain_query.h"

MGCORE_API char* mg_terrain_query(mg_core_t* core, double lat, double lon) {
    CHECK_CORE(core);
    mg::core::terrain::TerrainQuery tq;
    auto pt = tq.query_elevation(lat, lon);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid", pt.valid);
    jb.field("lat", pt.lat_deg);
    jb.field("lon", pt.lon_deg);
    jb.field("elevation", static_cast<double>(pt.elevation_m));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_terrain_query_line(mg_core_t* core,
                                        double lat1, double lon1,
                                        double lat2, double lon2,
                                        int num_samples) {
    CHECK_CORE(core);
    mg::core::terrain::TerrainQuery tq;
    auto result = tq.query_line(lat1, lon1, lat2, lon2, num_samples);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("valid", result.valid);
    mg::bridge::JsonBuilder arr;
    arr.begin_array();
    for (auto& pt : result.points) {
        mg::bridge::JsonBuilder obj;
        obj.begin_object();
        obj.field("lat", pt.lat_deg);
        obj.field("lon", pt.lon_deg);
        obj.field("elevation", static_cast<double>(pt.elevation_m));
        obj.end_object();
        arr.append_json(obj.str());
    }
    arr.end_array();
    jb.raw_field("points", arr.str());
    jb.end_object();
    return to_cstr(jb.str());
}
