#ifndef MGCORE_FFI_API_CONNECTION_H
#define MGCORE_FFI_API_CONNECTION_H

#include "ffi_api_core.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Opens one Serial/TCP/UDP physical link from a URI and returns its link ID.
/// Vehicles are created only after autopilot heartbeats and are listed by
/// mg_vehicle_list(). The result is a positive ID, or a negative mg_error_t on
/// failure. Opening a link never changes the active/UI focus.
MGCORE_API int mg_connection_open(mg_core_t* core, const char* endpoint_uri);
/// Returns state for this specific link or legacy vehicle session without
/// selecting it.
MGCORE_API char* mg_connection_get_session_state(mg_core_t* core, int link_id);
/// Closes exactly this link/session without relying on active vehicle focus.
MGCORE_API char* mg_connection_close(mg_core_t* core, int link_id);
/// Closes exactly one Vehicle route without closing a shared physical link.
MGCORE_API char* mg_connection_close_route(mg_core_t* core, int route_id);
/// Lists physical links and their discovered vehicle routes.
MGCORE_API char* mg_connection_list(mg_core_t* core);

/// Compatibility wrapper. Accepts a URI or a bare serial device path.
MGCORE_API int mg_vehicle_open(mg_core_t* core, const char* endpoint);
/// Compatibility wrapper for closing one vehicle and all of its routes.
MGCORE_API char* mg_vehicle_close(mg_core_t* core, int vehicle_id);
MGCORE_API char* mg_vehicle_list(mg_core_t* core);
MGCORE_API char* mg_vehicle_select(mg_core_t* core, int vehicle_id);
/// Applies a 32-byte MAVLink signing key, encoded as 64 hex characters, to
/// every route owned by the vehicle. Timestamp is in 10us ticks since 2015.
MGCORE_API char* mg_vehicle_set_signing_key(mg_core_t* core,
                                            int vehicle_id,
                                            const char* key_hex,
                                            int sign_outgoing,
                                            uint64_t persisted_timestamp);
/// Configures named signing-key candidates for automatic detection. JSON is
/// an array of {name, key_hex, timestamp_ticks}; matching uses the MAVLink2
/// HMAC before the key is enabled.
MGCORE_API char* mg_vehicle_set_signing_candidates(mg_core_t* core,
                                                   int vehicle_id,
                                                   const char* candidates_json);
MGCORE_API char* mg_vehicle_clear_signing_candidates(mg_core_t* core,
                                                     int vehicle_id);
MGCORE_API char* mg_vehicle_clear_signing_key(mg_core_t* core, int vehicle_id);
MGCORE_API char* mg_vehicle_get_signing_status(mg_core_t* core, int vehicle_id);
/// Returns the cached mission items for one Vehicle without changing focus.
MGCORE_API char* mg_vehicle_get_session_mission(mg_core_t* core, int vehicle_id);
/// Sends SETUP_SIGNING (unsigned) to establish the supplied key on the vehicle.
MGCORE_API char* mg_vehicle_send_setup_signing(mg_core_t* core,
                                               int vehicle_id,
                                               const char* key_hex,
                                               uint64_t initial_timestamp);
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
