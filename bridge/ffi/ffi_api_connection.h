#ifndef MGCORE_FFI_API_CONNECTION_H
#define MGCORE_FFI_API_CONNECTION_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Opens one Serial/TCP/UDP session from a URI and returns its vehicle/session ID.
/// The result is a positive ID, or a negative mg_error_t on failure. Opening a
/// session never changes the active/UI focus.
MGCORE_API int mg_connection_open(mg_core_t* core, const char* endpoint_uri);
/// Returns state for this specific session without selecting it.
MGCORE_API char* mg_connection_get_session_state(mg_core_t* core, int vehicle_id);
/// Closes exactly this session without relying on active vehicle focus.
MGCORE_API char* mg_connection_close(mg_core_t* core, int vehicle_id);

/// Compatibility wrapper. Accepts a URI or a bare serial device path.
MGCORE_API int mg_vehicle_open(mg_core_t* core, const char* endpoint);
/// Compatibility wrapper for mg_connection_close().
MGCORE_API char* mg_vehicle_close(mg_core_t* core, int vehicle_id);
MGCORE_API char* mg_vehicle_list(mg_core_t* core);
MGCORE_API char* mg_vehicle_select(mg_core_t* core, int vehicle_id);
MGCORE_API int mg_vehicle_get_active(mg_core_t* core);
MGCORE_API char* mg_discover_devices(mg_core_t* core, const char* transport_type);

MGCORE_API char* mg_connection_list_ports(mg_core_t* core);
/// Legacy single-connection convenience wrappers. They open then select the
/// new session so existing active-vehicle command callers keep working.
MGCORE_API char* mg_connection_connect(mg_core_t* core, const char* port, int baud);
MGCORE_API char* mg_connection_disconnect(mg_core_t* core);
MGCORE_API char* mg_connection_connect_tcp(mg_core_t* core, const char* endpoint);
MGCORE_API char* mg_connection_connect_udp(mg_core_t* core, const char* endpoint);
MGCORE_API char* mg_connection_get_state(mg_core_t* core);
MGCORE_API char* mg_connection_get_diagnostics(mg_core_t* core);

MGCORE_API char* mg_stream_request(mg_core_t* core, int stream_id, int rate_hz);
MGCORE_API char* mg_stream_set_interval(mg_core_t* core, int msg_id, int interval_us);
MGCORE_API char* mg_send_heartbeat(mg_core_t* core);

MGCORE_API char* mg_connection_get_heartbeat_age(mg_core_t* core);
MGCORE_API char* mg_connection_check_heartbeat(mg_core_t* core, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_CONNECTION_H */
