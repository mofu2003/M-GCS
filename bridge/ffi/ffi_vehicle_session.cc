#include "ffi_vehicle_session.h"

#include "ffi_core_context.h"

namespace mg::bridge::ffi {

namespace {

thread_local std::shared_ptr<mg::core::transport::ConnectionManager> t_connection;
thread_local mg_core_t* t_target_core = nullptr;
thread_local std::shared_ptr<mg::core::vehicle::Vehicle> t_vehicle_target_override;
thread_local mg_core_t* t_session_core = nullptr;
thread_local const VehicleSessionView* t_session_view = nullptr;

VehicleSessionView capture_vehicle_session(mg_core_t* core) {
    VehicleSessionView view{};
    if (!core) return view;
    if (t_target_core == core && t_vehicle_target_override) {
        view.vehicle_lease = t_vehicle_target_override;
    } else {
        view.vehicle_lease = core->vehicles.active_vehicle_shared();
        if (!view.vehicle_lease) view.vehicle_lease = core->fallback_vehicle;
    }
    view.vehicle = view.vehicle_lease.get();
    if (!view.vehicle) return view;

    const auto connection = t_target_core == core && t_connection
        ? t_connection : view.vehicle->command_connection();
    view.connection_lease = connection;
    view.mavlink_lease = connection ? connection->shared_mavlink() : nullptr;
    view.connected = connection && connection->is_connected();

    if (view.mavlink_lease) {
        const auto hb = view.mavlink_lease->latest_heartbeat();
        if (hb.has_value()) {
            view.target_sys = hb->system_id;
            view.target_comp = hb->component_id;
        }
    }
    return view;
}

} // namespace

VehicleSessionScope::VehicleSessionScope(mg_core_t* core)
    : previous_core_(t_session_core), previous_view_(t_session_view),
      view_(resolve_vehicle_session(core)) {
    t_session_core = core;
    t_session_view = &view_;
}

VehicleSessionScope::~VehicleSessionScope() {
    t_session_view = previous_view_;
    t_session_core = previous_core_;
}

VehicleTargetScope::VehicleTargetScope(
        mg_core_t* core, std::shared_ptr<mg::core::vehicle::Vehicle> vehicle,
        std::shared_ptr<mg::core::transport::ConnectionManager> connection)
    : previous_connection_(t_connection), previous_core_(t_target_core),
      previous_vehicle_(t_vehicle_target_override), previous_session_core_(t_session_core),
      previous_session_view_(t_session_view) {
    // Nested command scopes keep an existing route lease for the same target.
    const bool reuse_session = !connection && t_target_core == core &&
        t_vehicle_target_override == vehicle && t_session_core == core &&
        t_session_view && t_session_view->vehicle == vehicle.get();
    if (!connection && t_target_core == core && t_vehicle_target_override == vehicle)
        connection = t_connection;
    t_target_core = core;
    t_vehicle_target_override = std::move(vehicle);
    t_connection = std::move(connection);
    target_view_ = reuse_session ? *previous_session_view_
                                 : capture_vehicle_session(core);
    t_session_core = core;
    t_session_view = &target_view_;
}

VehicleTargetScope::VehicleTargetScope(mg_core_t* core, VehicleSessionView session)
    : previous_connection_(t_connection), previous_core_(t_target_core),
      previous_vehicle_(t_vehicle_target_override), previous_session_core_(t_session_core),
      previous_session_view_(t_session_view), target_view_(std::move(session)) {
    t_target_core = core;
    t_vehicle_target_override = target_view_.vehicle_lease;
    t_connection = target_view_.connection_lease;
    t_session_core = core;
    t_session_view = &target_view_;
}

VehicleTargetScope::~VehicleTargetScope() {
    t_vehicle_target_override = std::move(previous_vehicle_);
    t_connection = std::move(previous_connection_);
    t_target_core = previous_core_;
    t_session_view = previous_session_view_;
    t_session_core = previous_session_core_;
}

VehicleSessionView resolve_vehicle_session(mg_core_t* core) {
    if (t_session_core == core && t_session_view) return *t_session_view;
    return capture_vehicle_session(core);
}

mg::core::vehicle::Vehicle* require_vehicle(mg_core_t* core) {
    return resolve_vehicle_session(core).vehicle;
}

mg::core::telemetry::TelemetryCache* session_cache(mg_core_t* core) {
    auto* vehicle = require_vehicle(core);
    return vehicle ? &vehicle->cache() : nullptr;
}

} // namespace mg::bridge::ffi
