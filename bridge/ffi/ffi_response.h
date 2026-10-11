#ifndef MGCORE_FFI_RESPONSE_H
#define MGCORE_FFI_RESPONSE_H

#include "ffi_internal.h"

namespace mg::bridge::ffi::response {

inline char* ok(const std::string& data_json) {
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("data", data_json);
    jb.end_object();
    return to_cstr(jb.str());
}

inline char* error(mg_error_t code, const char* message) {
    mg_set_last_error(code);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    jb.raw_field("error", ffi_error_object_json(code, message ? message : ""));
    jb.end_object();
    return to_cstr(jb.str());
}

template <typename Writer>
char* object_from_cache(mg_core_t* core, const char* empty_json, Writer&& writer) {
    CHECK_CORE_RET(core, error(MG_ERROR_NULL_CORE, "null core"));

    auto* cache = get_active_cache(core);
    if (!cache) return ok(empty_json);

    mg::core::telemetry::JsonBuilder data;
    data.begin_object();
    writer(*cache, data);
    data.end_object();
    return ok(data.str());
}

} // namespace mg::bridge::ffi::response

#endif /* MGCORE_FFI_RESPONSE_H */
