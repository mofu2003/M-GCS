#include "ffi_vehicle_session.h"

#include "ffi_core_context.h"

namespace mg::bridge::ffi {

namespace {

thread_local mg_core_t* t_target_core = nullptr;
thread_local mg::core::vehicle::Vehicle* t_vehicle_target_override = nullptr;

mg::core::vehicle::Vehicle* resolve_vehicle(mg_core_t* core) {
    if (!core) return nullptr;
    if (t_target_core == core && t_vehicle_target_override) return t_vehicle_target_override;
    if (auto* active = core->vehicles.active_vehicle()) return active;
    return core->fallback_vehicle.get();
}

} // namespace

VehicleTargetScope::VehicleTargetScope(mg_core_t* core, mg::core::vehicle::Vehicle* vehicle)
    : previous_core_(t_target_core), previous_(t_vehicle_target_override) {
    t_target_core = core;
    t_vehicle_target_override = vehicle;
}

VehicleTargetScope::~VehicleTargetScope() {
    t_vehicle_target_override = previous_;
    t_target_core = previous_core_;
}

VehicleSessionView resolve_vehicle_session(mg_core_t* core) {
    VehicleSessionView view{};
    view.vehicle = resolve_vehicle(core);
    if (!view.vehicle) {
        return view;
    }

    view.mavlink = view.vehicle->mavlink();
    view.connected = view.vehicle->conn().is_connected();

    if (view.mavlink) {
        const auto hb = view.mavlink->latest_heartbeat();
        if (hb.has_value()) {
            view.target_sys = hb->system_id;
            view.target_comp = hb->component_id;
        }
    }
    return view;
}

mg::core::vehicle::Vehicle* require_vehicle(mg_core_t* core) {
    return resolve_vehicle_session(core).vehicle;
}

mg::bridge::TelemetryCache* session_cache(mg_core_t* core) {
    auto* vehicle = require_vehicle(core);
    return vehicle ? &vehicle->cache() : nullptr;
}

} // namespace mg::bridge::ffi
