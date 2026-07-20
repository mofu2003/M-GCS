/**
 * ffi_api.h - Umbrella C FFI interface for mgroundcontrol.
 *
 * Flutter/Dart may include this single header, or bind by domain using the
 * smaller ffi_api_*.h headers. All functions use C-compatible types only.
 *
 * Lifetime: mg_create() -> mg_destroy()
 * Strings: every returned char* must be released with mg_free_string().
 * Routing: command/query functions operate on the active vehicle selected via
 * mg_vehicle_select(), with legacy single-connection helpers kept for callers
 * that do not manage multiple vehicles yet.
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
