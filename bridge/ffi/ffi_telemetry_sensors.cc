#define MGCORE_EXPORTS
/**
 * Normalized power, sensor, and health telemetry FFI entrypoints.
 */
#include "ffi_response.h"








MGCORE_API char* mg_vehicle_get_firmware_version(mg_core_t* core) {
    return mg::bridge::ffi::response::object_from_cache(core,
        R"({"received":false,"major":0,"minor":0,"patch":0,"vendor_id":0,"product_id":0,"capabilities":0,"git_hash":""})",
        [](auto& cache, mg::core::telemetry::JsonBuilder& data) {
            auto snap = cache.snapshot_firmware();
            const auto& d = snap.data;
            data.field("received",     d.received);
            data.field("major",        (int)d.firmware_major);
            data.field("minor",        (int)d.firmware_minor);
            data.field("patch",        (int)d.firmware_patch);
            data.field("vendor_id",    (int)d.vendor_id);
            data.field("product_id",   (int)d.product_id);
            data.field("capabilities", (int64_t)d.capabilities);
            data.field("git_hash",     std::string(d.git_hash));
        });
}
