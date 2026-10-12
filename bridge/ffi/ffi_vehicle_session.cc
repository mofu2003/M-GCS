#include "ffi_vehicle_session.h"

#include "ffi_core_context.h"

namespace mg::bridge::ffi {

namespace {

thread_local std::shared_ptr<mg::core::transport::ConnectionManager> t_connection;
thread_local mg_core_t* t_target_core = nullptr;
thread_local mg::core::vehicle::Vehicle* t_vehicle_target_override = nullptr;

mg::core::vehicle::Vehicle* resolve_vehicle(mg_core_t* core) {
    if (!core) return nullptr;
    if (t_target_core == core && t_vehicle_target_override) return t_vehicle_target_override;
    if (auto* active = core->vehicles.active_vehicle()) return active;
    return core->fallback_vehicle.get();
}

} // namespace

VehicleTargetScope::VehicleTargetScope(mg_core_t* core, mg::core::vehicle::Vehicle* vehicle,
        std::shared_ptr<mg::core::transport::ConnectionManager> connection)
    : previous_connection_(t_connection), previous_core_(t_target_core),
      previous_(t_vehicle_target_override) {
    // Nested command scopes keep an existing route lease for the same target.
    if (!connection && t_target_core == core && t_vehicle_target_override == vehicle)
        connection = t_connection;
    t_target_core = core;
    t_vehicle_target_override = vehicle;
    t_connection = std::move(connection);
}

VehicleTargetScope::~VehicleTargetScope() {
    t_vehicle_target_override = previous_;
    t_connection = std::move(previous_connection_);
    t_target_core = previous_core_;
}

VehicleSessionView resolve_vehicle_session(mg_core_t* core) {
    VehicleSessionView view{};
    view.vehicle = resolve_vehicle(core);
    if (!view.vehicle) {
        return view;
    }

    const auto connection = t_target_core == core && t_connection
        ? t_connection : view.vehicle->command_connection();
    // shared_mavlink() hands out an owning lease (never a borrowed pointer), so
    // the view can keep the adapter alive while the caller blocks on it; a
    // concurrent reset() may detach it but must not free it under the caller.
    view.mavlink_lease = connection ? connection->shared_mavlink() : nullptr;
    view.mavlink = view.mavlink_lease.get();
    view.connected = connection && connection->is_connected();

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

mg::core::telemetry::TelemetryCache* session_cache(mg_core_t* core) {
    auto* vehicle = require_vehicle(core);
    return vehicle ? &vehicle->cache() : nullptr;
}

} // namespace mg::bridge::ffi
