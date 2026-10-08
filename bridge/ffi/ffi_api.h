/**
 * ffi_api.h - Umbrella C FFI interface for mgroundcontrol.
 *
 * Consumers may include this single header, or bind by domain using the
 * smaller ffi_api_*.h headers. All functions use C-compatible types only.
 *
 * Lifetime: mg_create() -> mg_destroy(). Preferred connection lifecycle is
 * mg_connection_open(uri) -> link ID -> mg_connection_get_session_state()
 * / mg_connection_close(link ID), independent of active focus. A link may
 * discover multiple vehicles; mg_connection_close_route() removes only one
 * route while preserving the shared physical link.
 * Strings: every returned char* must be released with mg_free_string().
 * Routing: mg_vehicle_open() creates an independent session without changing
 * focus. UDP sessions use a shared LinkSession so one socket can discover more
 * than one sysid; each discovered vehicle has its own session/vehicle id.
 * Legacy command/query functions operate on the vehicle selected via
 * mg_vehicle_select(); single-connection helpers select their own session for
 * backwards compatibility. Multi-vehicle callers should use
 * mg_command_execute_for_vehicle() so a command targets an explicit session
 * without changing UI focus.
 */

#ifndef MGCORE_FFI_API_H
#define MGCORE_FFI_API_H

#include "ffi_api_core.h"
#include "ffi_api_connection.h"
#include "ffi_api_telemetry.h"
#include "ffi_api_firmware.h"
#include "ffi_api_params.h"
#include "ffi_api_mission.h"
#include "ffi_api_flight.h"
#include "ffi_api_configuration.h"
#include "ffi_api_geofence.h"
#include "ffi_api_logging.h"
#include "ffi_api_payload.h"
#include "ffi_api_environment.h"

#endif /* MGCORE_FFI_API_H */
