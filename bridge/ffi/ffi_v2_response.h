#ifndef MGCORE_FFI_V2_RESPONSE_H
#define MGCORE_FFI_V2_RESPONSE_H

#include "ffi_internal.h"

namespace mg::bridge::ffi::v2 {

inline char* ok(const std::string& data_json) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("data", data_json);
    jb.end_object();
    return to_cstr(jb.str());
}

inline char* error(const char* code, const char* message) {
    JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    JsonBuilder err;
    err.begin_object();
    err.field("code", code);
    err.field("message", message);
    err.end_object();
    jb.raw_field("error", err.str());
    jb.end_object();
    return to_cstr(jb.str());
}

template <typename Writer>
char* object_from_cache(mg_core_t* core, const char* empty_json, Writer&& writer) {
    CHECK_CORE_RET(core, error("CORE_NULL", "null core"));

    auto* cache = get_active_cache(core);
    if (!cache) return ok(empty_json);

    JsonBuilder data;
    data.begin_object();
    writer(*cache, data);
    data.end_object();
    return ok(data.str());
}

} // namespace mg::bridge::ffi::v2

#endif /* MGCORE_FFI_V2_RESPONSE_H */
