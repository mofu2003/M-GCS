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

/// Thread-local target override used by targeted command dispatch.
/// It does not change the global UI active vehicle.
class VehicleTargetScope {
public:
    VehicleTargetScope(mg_core_t* core, mg::core::vehicle::Vehicle* vehicle,
        std::shared_ptr<mg::core::transport::ConnectionManager> connection = {});
    ~VehicleTargetScope();
    VehicleTargetScope(const VehicleTargetScope&) = delete;
    VehicleTargetScope& operator=(const VehicleTargetScope&) = delete;
private:
    std::shared_ptr<mg::core::transport::ConnectionManager> previous_connection_;
    mg_core_t* previous_core_ = nullptr;
    mg::core::vehicle::Vehicle* previous_ = nullptr;
};

VehicleSessionView resolve_vehicle_session(mg_core_t* core);
mg::core::vehicle::Vehicle* require_vehicle(mg_core_t* core);
mg::bridge::TelemetryCache* session_cache(mg_core_t* core);

} // namespace mg::bridge::ffi

#endif /* MGCORE_FFI_VEHICLE_SESSION_H */
