#ifndef MGCORE_FFI_VEHICLE_SESSION_H
#define MGCORE_FFI_VEHICLE_SESSION_H

#include "ffi_api.h"
#include "core/telemetry/telemetry_cache.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/vehicle/vehicle.h"

struct mg_core_t;

namespace mg::bridge::ffi {

struct VehicleSessionView {
    /// Keeps the selected Vehicle and its ConnectionManager alive for this operation.
    std::shared_ptr<mg::core::vehicle::Vehicle> vehicle_lease;
    mg::core::vehicle::Vehicle* vehicle = nullptr;
    std::shared_ptr<mg::core::transport::ConnectionManager> connection_lease;
    /// Owning lease for every adapter access in the resolved session. Holding
    /// the view keeps the adapter alive across disconnect and blocking calls.
    std::shared_ptr<mg::core::mavlink::MavlinkAdapter> mavlink_lease;
    uint8_t target_sys = 1;
    uint8_t target_comp = 0;
    bool connected = false;
};

/// Pins one resolved vehicle session for the lifetime of an FFI operation.
/// Nested helpers on the same thread reuse the exact adapter, target IDs, and
/// connection state captured here.
class VehicleSessionScope {
public:
    explicit VehicleSessionScope(mg_core_t* core);
    ~VehicleSessionScope();
    VehicleSessionScope(const VehicleSessionScope&) = delete;
    VehicleSessionScope& operator=(const VehicleSessionScope&) = delete;

    const VehicleSessionView& view() const { return view_; }

private:
    mg_core_t* previous_core_ = nullptr;
    const VehicleSessionView* previous_view_ = nullptr;
    VehicleSessionView view_;
};

/// Thread-local target override used by targeted command dispatch.
/// It does not change the global UI active vehicle.
class VehicleTargetScope {
public:
    VehicleTargetScope(mg_core_t* core,
        std::shared_ptr<mg::core::vehicle::Vehicle> vehicle,
        std::shared_ptr<mg::core::transport::ConnectionManager> connection = {});
    VehicleTargetScope(mg_core_t* core, VehicleSessionView session);
    ~VehicleTargetScope();
    VehicleTargetScope(const VehicleTargetScope&) = delete;
    VehicleTargetScope& operator=(const VehicleTargetScope&) = delete;
private:
    std::shared_ptr<mg::core::transport::ConnectionManager> previous_connection_;
    mg_core_t* previous_core_ = nullptr;
    std::shared_ptr<mg::core::vehicle::Vehicle> previous_vehicle_;
    mg_core_t* previous_session_core_ = nullptr;
    const VehicleSessionView* previous_session_view_ = nullptr;
    VehicleSessionView target_view_;
};

VehicleSessionView resolve_vehicle_session(mg_core_t* core);
mg::core::vehicle::Vehicle* require_vehicle(mg_core_t* core);
mg::core::telemetry::TelemetryCache* session_cache(mg_core_t* core);

} // namespace mg::bridge::ffi

#endif /* MGCORE_FFI_VEHICLE_SESSION_H */
