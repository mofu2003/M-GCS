#ifndef MGCORE_FFI_API_CONNECTION_H
#define MGCORE_FFI_API_CONNECTION_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Opens an independent vehicle session. Does not change the active/UI focus.
/// Use mg_vehicle_select() only when legacy active-vehicle APIs are intended.
MGCORE_API int mg_vehicle_open(mg_core_t* core, const char* endpoint);
MGCORE_API char* mg_vehicle_close(mg_core_t* core, int vehicle_id);
MGCORE_API char* mg_vehicle_list(mg_core_t* core);
MGCORE_API char* mg_vehicle_select(mg_core_t* core, int vehicle_id);
MGCORE_API int mg_vehicle_get_active(mg_core_t* core);
MGCORE_API char* mg_discover_devices(mg_core_t* core, const char* transport_type);

MGCORE_API char* mg_connection_list_ports(mg_core_t* core);
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
