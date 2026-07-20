#ifndef MGCORE_FFI_VEHICLE_SESSION_H
#define MGCORE_FFI_VEHICLE_SESSION_H

#include "ffi_api.h"
#include "telemetry_cache.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/vehicle/vehicle.h"

struct mg_core_t;

namespace mg::bridge::ffi {

struct VehicleSessionView {
    mg::core::vehicle::Vehicle* vehicle = nullptr;
    mg::core::mavlink::MavlinkAdapter* mavlink = nullptr;
    uint8_t target_sys = 1;
    uint8_t target_comp = 0;
    bool connected = false;
};

VehicleSessionView resolve_vehicle_session(mg_core_t* core);
mg::core::vehicle::Vehicle* require_vehicle(mg_core_t* core);
mg::bridge::TelemetryCache* session_cache(mg_core_t* core);

} // namespace mg::bridge::ffi

#endif /* MGCORE_FFI_VEHICLE_SESSION_H */
