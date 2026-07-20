/**
 * telemetry_bindings.h - Declarative telemetry registration table
 *
 * 遥测类型声明式注册表，新增遥测类型只需添加一行。
 *
 * Defines all telemetry types as compile-time data. Each entry includes:
 *   - Event type to fire
 *   - Cache group to update
 *   - Whether to fire events
 *   - Registration function (connects MavlinkAdapter callback to cache update)
 *
 * Adding a new telemetry type: add ONE entry to kTelemetryBindings[].
 * No other files need to change (no switch-case to maintain).
 *
 * v0.9:  Uses TelemetryCache from telemetry_cache.h (FactGroup pattern).
 * v0.18: Added RegisterFn to eliminate switch-case in ffi_connection.cc.
 *        Fully declarative: table drives both cache update AND callback registration.
 */

#ifndef MGCORE_TELEMETRY_BINDINGS_H
#define MGCORE_TELEMETRY_BINDINGS_H

#include "ffi_api.h"
#include "telemetry_cache.h"
#include "ffi_internal.h"
#include <cstddef>
#include <functional>

namespace mg::bridge {

/// Callback signature for cache update + optional event fire.
/// Each binding creates a handler matching this signature.
using TelemetryHandler = std::function<void(const void*)>;

/// Registration function: connects a handler to MavlinkAdapter's callback.
/// Takes mavlink adapter pointer and handler; registers handler as callback.
using RegisterFn = std::function<void(
    mg::core::mavlink::MavlinkAdapter* mav,
    TelemetryHandler handler)>;

/// One row in the declarative telemetry table.
struct TelemetryBinding {
    mg_event_type_t event;          // which event to fire
    TelemetryType   cache;          // which cache group to update
    bool            fire_event;     // whether to call post_event()
    RegisterFn      register_fn;    // connects MavlinkAdapter callback
};

/// Helper: create a RegisterFn for a specific MavlinkAdapter callback setter.
/// Usage: register_with(&MavlinkAdapter::on_attitude)
template<typename CallbackSetter>
RegisterFn make_register_fn(CallbackSetter setter) {
    return [setter](mg::core::mavlink::MavlinkAdapter* mav, TelemetryHandler handler) {
        // Extract the callback argument type from the setter
        (mav->*setter)([handler](const auto& data) {
            handler(&data);
        });
    };
}

/// The complete telemetry registration table.
/// Order does not matter; each entry is independent.
/// To add a new telemetry type, add ONE row here. Nothing else changes.
static const TelemetryBinding kTelemetryBindings[] = {
    // event                 cache target               fire?  register function
    { MG_EVENT_HEARTBEAT,    TelemetryType::kHeartbeat,       true,
      [](mg::core::mavlink::MavlinkAdapter* mav, TelemetryHandler handler) {
          mav->on_heartbeat([handler](const auto& hb) { handler(&hb); });
      }},
    { MG_EVENT_SYS_STATUS,   TelemetryType::kSysStatus,       true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_sys_status)},
    { MG_EVENT_GPS,          TelemetryType::kGps,             true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_gps)},
    { MG_EVENT_ATTITUDE,     TelemetryType::kAttitude,        true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_attitude)},
    { MG_EVENT_TELEMETRY,    TelemetryType::kVfrHud,          false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_vfr_hud)},
    { MG_EVENT_IMU,           TelemetryType::kImu,             false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_imu)},
    { MG_EVENT_PRESSURE,      TelemetryType::kPressure,        false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_pressure)},
    { MG_EVENT_FIRMWARE,     TelemetryType::kFirmwareVersion, true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_firmware_version)},
    { MG_EVENT_RC_CHANNELS,   TelemetryType::kRcChannels,      false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_rc_channels)},
    { MG_EVENT_SERVO_OUTPUT,  TelemetryType::kServoOutput,     false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_servo_output)},
    { MG_EVENT_GLOBAL_POSITION, TelemetryType::kGlobalPosition, true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_global_position)},
    { MG_EVENT_HOME_POSITION, TelemetryType::kHomePosition,    false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_home_position)},
    { MG_EVENT_BATTERY, TelemetryType::kBattery, true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_battery)},
    { MG_EVENT_HIGHRES_IMU, TelemetryType::kHighresImu, false,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_highres_imu)},
    { MG_EVENT_EXTENDED_SYS_STATE, TelemetryType::kExtendedSysState, true,
      make_register_fn(&mg::core::mavlink::MavlinkAdapter::on_extended_sys_state)},
};

static constexpr size_t kTelemetryBindingCount =
    sizeof(kTelemetryBindings) / sizeof(kTelemetryBindings[0]);

} // namespace mg::bridge

#endif /* MGCORE_TELEMETRY_BINDINGS_H */