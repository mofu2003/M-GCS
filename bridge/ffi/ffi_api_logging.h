#ifndef MGCORE_FFI_API_LOGGING_H
#define MGCORE_FFI_API_LOGGING_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_log_request_list(mg_core_t* core);
MGCORE_API char* mg_log_get_entries(mg_core_t* core);
MGCORE_API char* mg_log_list(mg_core_t* core);
MGCORE_API char* mg_log_download(mg_core_t* core, int log_id, const char* filepath);
MGCORE_API char* mg_log_download_cancel(mg_core_t* core);
MGCORE_API char* mg_log_download_status(mg_core_t* core);
MGCORE_API char* mg_log_record_start(mg_core_t* core, const char* filepath);
MGCORE_API char* mg_log_record_stop(mg_core_t* core);
MGCORE_API char* mg_log_record_status(mg_core_t* core);

/* Compatibility aliases retained for older consumers. */
MGCORE_API char* mg_logging_get_entries(mg_core_t* core, int count);
MGCORE_API char* mg_logging_get_stats(mg_core_t* core);

MGCORE_API char* mg_log_file_start(mg_core_t* core, const char* session_name);
MGCORE_API char* mg_log_file_stop(mg_core_t* core);
MGCORE_API char* mg_log_file_status(mg_core_t* core);

MGCORE_API char* mg_log_start(mg_core_t* core);
MGCORE_API char* mg_log_stop(mg_core_t* core);
MGCORE_API char* mg_log_get_status(mg_core_t* core);
MGCORE_API char* mg_log_get_events(mg_core_t* core);

/* Generic vehicle file-system access through MAVLink FTP. */
MGCORE_API char* mg_fs_list_directory(mg_core_t* core, const char* remote_path);
MGCORE_API char* mg_fs_download_file(mg_core_t* core, const char* remote_path,
                                      const char* local_path);
MGCORE_API char* mg_fs_upload_file(mg_core_t* core, const char* local_path,
                                    const char* remote_path);
MGCORE_API char* mg_fs_download_cancel(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_LOGGING_H */
