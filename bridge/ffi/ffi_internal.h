/**
 * ffi_internal.h - Internal shared state for FFI bridge
 *
 * FFI 内部共享状态、辅助函数和活跃飞行器路由逻辑。
 *
 * v3.0: All subsystems migrated into Vehicle class.
 *       All ffi_*.cc should use the get_active_* helpers to route
 *       through the active vehicle's subsystems.
 */

#ifndef MGCORE_FFI_INTERNAL_H
#define MGCORE_FFI_INTERNAL_H

#include "ffi_core_context.h"
#include "ffi_api.h"
#include "ffi_mission_bridge.h"
#include "ffi_vehicle_session.h"
#include "json_builder.h"
#include "telemetry_cache.h"
#include "core/sensor/sensor_id_decoder.h"
#include "core/calibration/calibration_service.h"
#include "core/config/param_descriptors.h"
#include "core/firmware/firmware_adapter.h"
#include "core/settings/flight_logging_service.h"
#include "core/telemetry/telemetry_ring_buffer.h"
#include "core/telemetry/waveform_buffer.h"
#include "core/geofence/geofence_manager.h"
#include "core/flight/flight_mode_manager.h"
#include "core/mission/mission_types.h"
#include "core/settings/mavlink_log_writer.h"
#include "ardupilotmega/mavlink.h"

#include <string>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

/* ================================================================
 * Shared Helpers
 * ================================================================ */

inline void post_event(mg_core_t* core, mg_event_type_t type, const std::string& json) {
    core->events.fire(type, json);
}

inline char* ok() { return to_cstr(R"({"success":true})"); }
inline char* fail(mg_error_t error, const char* reason) {
    mg_set_last_error(error);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    jb.field("error", reason ? reason : "");
    jb.field("code", static_cast<int>(error));
    jb.end_object();
    return to_cstr(jb.str());
}

inline void platform_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

// Guard macros: all check the active vehicle or fallback vehicle connection
#define CHECK_CORE(core) do { if (!(core)) return fail(MG_ERROR_NULL_CORE, "null core"); } while(0)
#define CHECK_MAVLINK(core) do { if (!get_active_mavlink(core)) return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle connected"); } while(0)
#define CHECK_CONNECTED(core) do { if (!is_active_connected(core)) return fail(MG_ERROR_NOT_CONNECTED, "not connected"); } while(0)
#define CHECK_CORE_RET(core, ret) do { if (!(core)) return (ret); } while(0)

// Max statustext length for truncation
#ifndef MG_STATUSTEXT_MAX_LEN
#define MG_STATUSTEXT_MAX_LEN 128
#endif

// Forward declarations for calibration FFI (defined in ffi_calibration.cc)
extern "C" void mg_calibration_process_statustext(mg_core_t* core, const char* text, uint8_t severity);
extern "C" void mg_calibration_process_command_ack(mg_core_t* core, uint16_t command, uint8_t result, uint8_t progress);

inline mg::core::mavlink::MavlinkAdapter* get_current_mavlink(mg_core_t* core);

// Helper to get target system/component ID from core
inline uint8_t target_sys(mg_core_t* core) {
    auto* mav = get_current_mavlink(core);
    if (!mav) return 1;
    auto hb = mav->latest_heartbeat();
    return hb.has_value() ? hb->system_id : 1;
}
inline uint8_t target_comp(mg_core_t* core) {
    auto* mav = get_current_mavlink(core);
    if (!mav) return 0;
    auto hb = mav->latest_heartbeat();
    return hb.has_value() ? hb->component_id : 0;
}

// ================================================================
// Active vehicle core helpers
//
// The active vehicle is preferred; fallback_vehicle covers offline and
// single-session test paths so helpers stay uniform.
// ================================================================

/// Get the active Vehicle* (multi-vehicle) or nullptr.
inline mg::core::vehicle::Vehicle* get_active_vehicle(mg_core_t* core) {
    return mg::bridge::ffi::require_vehicle(core);
}

// Get mavlink adapter for current vehicle (or fallback if no active vehicle selected)
inline mg::core::mavlink::MavlinkAdapter* get_current_mavlink(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).mavlink;
}

// ================================================================
// Active vehicle helpers — used by ALL ffi_*.cc files
// ================================================================

// Get mavlink for current vehicle (or fallback)
inline mg::core::mavlink::MavlinkAdapter* get_active_mavlink(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).mavlink;
}

// Get connection manager for current vehicle (or fallback)
inline mg::core::transport::ConnectionManager& get_active_conn(mg_core_t* core) {
    return get_active_vehicle(core)->conn();
}

// Check if current vehicle is connected
inline bool is_active_connected(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).connected;
}

// Get target system/component from active vehicle's heartbeat
inline uint8_t active_target_sys(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).target_sys;
}
inline uint8_t active_target_comp(mg_core_t* core) {
    return mg::bridge::ffi::resolve_vehicle_session(core).target_comp;
}

// ================================================================
// Active vehicle subsystem helpers — route through Vehicle
// ================================================================

// Accessors route through active vehicle or fallback vehicle.
inline mg::bridge::TelemetryCache* get_active_cache(mg_core_t* core) {
    return mg::bridge::ffi::session_cache(core);
}

inline mg::core::calibration::CalibrationService& get_active_calibration(mg_core_t* core) {
    return get_active_vehicle(core)->calibration();
}
inline mg::core::flight::FlightModeManager& get_active_flight_modes(mg_core_t* core) {
    return get_active_vehicle(core)->flight_modes();
}
inline mg::core::geofence::GeofenceManager& get_active_geofence(mg_core_t* core) {
    return get_active_vehicle(core)->geofence();
}
inline mg::core::settings::MavlinkLogWriter& get_active_mavlink_log(mg_core_t* core) {
    return get_active_vehicle(core)->mavlink_log();
}
inline mg::core::settings::FlightLoggingService& get_active_flight_log(mg_core_t* core) {
    return get_active_vehicle(core)->flight_log();
}
inline mg::core::sensor::SensorIdDecoder& get_active_sensor_decoder(mg_core_t* core) {
    return get_active_vehicle(core)->sensor_decoder();
}
inline mg::core::telemetry::TelemetryRingBuffer& get_active_ring_buffer(mg_core_t* core) {
    return get_active_vehicle(core)->ring_buffer();
}
inline mg::core::telemetry::WaveformBuffer* get_active_waveform(mg_core_t* core,
                                                                size_t instance) {
    auto* vehicle = get_active_vehicle(core);
    return vehicle ? vehicle->waveform(instance) : nullptr;
}

inline mg::core::mission::MissionPlan ffi_present_mission_plan(mg_core_t* core,
                                                               const mg::core::mission::MissionPlan& raw_plan) {
    return mg::bridge::ffi::present_mission_plan(core, raw_plan);
}

inline mg::core::mission::MissionPlan ffi_prepare_mission_upload(mg_core_t* core,
                                                                 const mg::core::mission::MissionPlan& client_plan) {
    return mg::bridge::ffi::prepare_mission_upload(core, client_plan);
}


// ================================================================
// Shared parameter helpers for ffi_*.cc
// ================================================================

/// Check if active vehicle is PX4 (returns false for ArduPilot or no vehicle).
inline bool ffi_is_px4(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return false;
    auto hb = mav->latest_heartbeat();
    return hb.has_value() && hb->autopilot_type == 12;
}

inline std::string ffi_param_profile(mg_core_t* core) {
    auto* mav = get_active_mavlink(core);
    if (!mav) return "";
    auto hb = mav->latest_heartbeat();
    if (!hb.has_value() || hb->autopilot_type != 3) return "";

    mg::core::firmware::FirmwareAdapter adapter;
    adapter.detect(hb->autopilot_type, hb->vehicle_type);
    const auto family = adapter.normalized_vehicle_name();
    if (family == "Copter") return "copter";
    if (family == "FixedWing") return "plane";
    if (family == "VTOL") return "vtol";
    return "";
}

/// Get a parameter value by name (returns 0 if not found or no connection).
inline float ffi_get_param(mg_core_t* core, const char* name) {
    auto* mav = get_active_mavlink(core);
    if (!mav || !name) return 0;
    auto p = mav->get_param_by_name(name);
    return p.has_value() ? p->value : 0;
}

/// Set a parameter value by name. Returns true on success.
inline bool ffi_set_param(mg_core_t* core, const char* name, float value) {
    auto* mav = get_active_mavlink(core);
    if (!mav || !name) return false;
    return mav->set_param(active_target_sys(core), active_target_comp(core), name, value, 0);
}

/// Format a parameter name with a numeric suffix (e.g. "SERVO%d_MIN" + 1 -> "SERVO1_MIN").
inline std::string ffi_fmt_param(const char* fmt, int num) {
    if (!fmt) return "";
    char buf[64];
    snprintf(buf, sizeof(buf), fmt, num);
    return std::string(buf);
}
// Global singleton for parameter descriptor database.
// Loaded once from ParameterDescriptors.json on first access.
inline mg::core::config::ParamDescriptors& get_desc_db() {
    static mg::core::config::ParamDescriptors db;
    return db;
}
#endif /* MGCORE_FFI_INTERNAL_H */
