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
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!filepath || filepath[0] == '\0' || log_id < 0 || log_id > 65535) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid log id or filepath");
    }
    auto* mav = get_active_mavlink(core);
    if (!mav->log_download_start(active_target_sys(core), active_target_comp(core),
                                 static_cast<uint16_t>(log_id), filepath)) {
        return fail(MG_ERROR_SEND_FAILED, "log download request failed");
    }
    return ok();
}

MGCORE_API char* mg_log_download_cancel(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!get_active_mavlink(core)->log_download_cancel()) {
        return fail(MG_ERROR_NOT_SUPPORTED, "no active log download");
    }
    return ok();
}

MGCORE_API char* mg_log_download_status(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    const auto status = get_active_mavlink(core)->log_download_status();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("active", status.active);
    jb.field("completed", status.completed);
    jb.field("cancelled", status.cancelled);
    jb.field("state", status.active ? "downloading" :
                         (status.completed ? "completed" :
                          (status.cancelled ? "cancelled" :
                           (status.error.empty() ? "idle" : "failed"))));
    jb.field("log_id", static_cast<int>(status.id));
    jb.field("received_bytes", static_cast<int>(status.received_bytes));
    jb.field("total_bytes", static_cast<int>(status.total_bytes));
    jb.field("path", status.filepath);
    jb.field("message", status.error);
    jb.end_object();
    return to_cstr(jb.str());
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

MGCORE_API char* mg_fs_list_directory(mg_core_t* core, const char* remote_path) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!remote_path || remote_path[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "invalid remote directory");
    }
    std::string error;
    const auto entries = get_active_mavlink(core)->list_directory(
        active_target_sys(core), active_target_comp(core), remote_path, &error);
    if (!error.empty()) return fail(MG_ERROR_SEND_FAILED, error.c_str());

    mg::bridge::JsonBuilder result;
    result.begin_object();
    result.field("success", true);
    result.field("path", remote_path);
    mg::bridge::JsonBuilder items;
    items.begin_array();
    for (const auto& entry : entries) {
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("name", entry.name);
        item.field("directory", entry.is_directory);
        item.field("size_bytes", static_cast<int>(entry.size));
        item.end_object();
        items.append_json(item.str());
    }
    items.end_array();
    result.raw_field("items", items.str());
    result.end_object();
    return to_cstr(result.str());
}

MGCORE_API char* mg_fs_download_file(mg_core_t* core, const char* remote_path,
                                      const char* local_path) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!remote_path || !local_path || remote_path[0] == '\0' || local_path[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "invalid file path");
    }
    std::string error;
    if (!get_active_mavlink(core)->download_file(
            active_target_sys(core), active_target_comp(core),
            remote_path, local_path, &error)) {
        return fail(MG_ERROR_SEND_FAILED,
                    error.empty() ? "file download failed" : error.c_str());
    }
    return ok();
}

MGCORE_API char* mg_fs_upload_file(mg_core_t* core, const char* local_path,
                                   const char* remote_path) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!local_path || !remote_path || local_path[0] == '\0' || remote_path[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "invalid file path");
    }
    std::string error;
    if (!get_active_mavlink(core)->upload_file(
            active_target_sys(core), active_target_comp(core),
            local_path, remote_path, &error)) {
        return fail(MG_ERROR_SEND_FAILED,
                    error.empty() ? "file upload failed" : error.c_str());
    }
    return ok();
}

MGCORE_API char* mg_fs_download_cancel(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    get_active_mavlink(core)->cancel_file_transfer();
    return ok();
}
