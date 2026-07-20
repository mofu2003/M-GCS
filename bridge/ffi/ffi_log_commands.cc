#define MGCORE_EXPORTS
/**
 * MAVLink log listing and local log recording FFI entrypoints.
 */
#include "ffi_internal.h"
#include "json_builder.h"

namespace {

template <typename Sender>
char* send_active_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}

}  // namespace

MGCORE_API char* mg_log_request_list(mg_core_t* core) {
    return send_active_request(core, "log request list send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.log_request_list(sys, comp);
        });
}

MGCORE_API char* mg_log_get_entries(mg_core_t* core) {
    CHECK_CORE(core);
    auto* mav = get_active_mavlink(core);
    if (!mav) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle");
    auto entries = mav->log_get_entries();

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& entry : entries) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("id", static_cast<int>(entry.id));
        item.field("time_utc", static_cast<int>(entry.time_utc));
        item.field("size_bytes", static_cast<int>(entry.size_bytes));
        item.end_object();
        jb.append_json(item.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_log_list(mg_core_t* core) {
    CHECK_CORE(core);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    mg::bridge::JsonBuilder arr;
    arr.begin_array();
    arr.end_array();
    jb.raw_field("logs", arr.str());
    jb.field("total", 0);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_log_download(mg_core_t* core, int log_id, const char* filepath) {
    CHECK_CORE(core);
    (void)log_id;
    (void)filepath;
    return fail(MG_ERROR_NOT_SUPPORTED, "log download not yet implemented");
}

MGCORE_API char* mg_log_record_start(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "null filepath");
    }

    if (get_active_mavlink_log(core).start_recording(filepath)) return ok();
    return fail(MG_ERROR_SEND_FAILED, "failed to start recording");
}

MGCORE_API char* mg_log_record_stop(mg_core_t* core) {
    CHECK_CORE(core);
    get_active_mavlink_log(core).stop_recording();
    return ok();
}

MGCORE_API char* mg_log_record_status(mg_core_t* core) {
    CHECK_CORE(core);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("recording", get_active_mavlink_log(core).is_recording());
    jb.field("file", get_active_mavlink_log(core).current_file());
    jb.field("bytes", static_cast<int>(get_active_mavlink_log(core).bytes_written()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_logging_get_entries(mg_core_t* core, int count) {
    (void)count;
    return mg_log_get_entries(core);
}

MGCORE_API char* mg_logging_get_stats(mg_core_t* core) {
    return mg_log_record_status(core);
}
