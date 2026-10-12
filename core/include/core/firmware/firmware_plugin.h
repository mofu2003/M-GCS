/**
 * firmware_plugin.h - Firmware-specific behavior strategy (QGC FirmwarePlugin pattern)
 * 固件插件接口 — 统一封装固件特有逻辑，ArduPilot/PX4 各自实现
 *
 * Consolidates ALL firmware-specific logic behind one interface.
 * Each firmware (ArduPilot, PX4) provides a concrete implementation.
 *
 * What moves into FirmwarePlugin:
 *   - Flight mode names and numbers (from FlightModeManager hardcoding)
 *   - Available modes list (from FlightModeManager::available_modes())
 *   - Mode name → number reverse lookup (from FlightModeManager::get_mode_number())
 *   - Special mode names: RTL, Land, Guided, Loiter, Auto, SmartRTL
 *   - GUIDED mode capabilities and minimum altitudes
 *   - Firmware capabilities declaration
 *
 * What stays outside:
 *   - HEARTBEAT decoding (already in MavlinkAdapter)
 *   - MAVLink message sending (already in MavlinkAdapter)
 *   - Connection management (already in ConnectionManager)
 *
 * Design:
 *   - FirmwarePlugin is an abstract interface (pure virtual)
 *   - ArduPilotPlugin implements for ArduPilot (all vehicle types)
 *   - PX4Plugin implements for PX4
 *   - FirmwarePluginFactory creates the right plugin from HEARTBEAT
 *
 * QGC precedent: src/FirmwarePlugin/FirmwarePlugin.h (24KB, 100+ virtual methods)
 * We implement only the subset needed for current mgroundcontrol features.
 *
 * MP precedent: MissionPlanner uses CurrentState.cs with firmware-specific
 * switch-cases scattered throughout. We avoid that pattern.
 */

#ifndef MGCORE_FIRMWARE_PLUGIN_H
#define MGCORE_FIRMWARE_PLUGIN_H

#include "core/core_types.h"
#include "core/firmware/firmware_adapter.h"
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "core/firmware/param_remapping.h"

namespace mg::core::firmware {

/// Firmware capabilities bitmask (QGC FirmwareCapabilities pattern)
enum FirmwareCapability : uint32_t {
    CapGuidedMode       = 1 << 0,  // Supports GUIDED mode commands
    CapPause            = 1 << 1,  // Supports pause at current location
    CapTakeoff          = 1 << 2,  // Supports guided takeoff
    CapOrbit            = 1 << 3,  // Supports orbit mode
    CapSmartRTL         = 1 << 4,  // Supports Smart RTL
    CapSetHome          = 1 << 5,  // Supports setting home position
    CapMotorTest        = 1 << 6,  // Supports motor test
    CapCalibration      = 1 << 7,  // Supports sensor calibration via MAVLink
    CapRoi              = 1 << 8,  // Supports region-of-interest (QGC ROIModeCapability)
    CapChangeHeading    = 1 << 9,  // Supports changing heading in place (QGC ChangeHeadingCapability)
};

/// A single available flight mode (name + number + settable flag)
struct FlightModeEntry {
    std::string name;
    uint32_t    custom_mode;   // The custom_mode value for HEARTBEAT
    bool        can_be_set;    // Whether user can switch to this mode
};

/// The strategy interface for firmware-specific behavior.
class IFirmwarePlugin {
public:
    virtual ~IFirmwarePlugin() = default;

    /// Get the firmware kind this plugin handles.
    virtual FirmwareKind firmware_kind() const = 0;

    /// Get a human-readable name for this plugin.
    virtual const char* plugin_name() const = 0;

    // ================================================================
    // Flight Mode Mapping
    // ================================================================

    /// Decode custom_mode from HEARTBEAT into a human-readable name.
    virtual std::string decode_flight_mode(VehicleKind vehicle, uint32_t custom_mode) const = 0;

    /// Get the custom_mode number for a named flight mode.
    /// Returns -1 if the mode name is not recognized.
    virtual int encode_flight_mode(VehicleKind vehicle, const std::string& mode_name) const = 0;

    /// Get all available flight modes for a vehicle type.
    virtual std::vector<FlightModeEntry> available_modes(VehicleKind vehicle) const = 0;

    // ================================================================
    // Special Mode Names (used by FlightModeManager for high-level commands)
    // ================================================================

    /// Get the mode name for Return To Launch.
    virtual const char* rtl_mode_name() const = 0;

    /// Get the mode name for Land.
    virtual const char* land_mode_name() const = 0;

    /// Get the mode name for Guided mode.
    virtual const char* guided_mode_name() const = 0;

    /// Get the mode name for Loiter/Hold.
    virtual const char* loiter_mode_name() const = 0;

    /// Get the mode name for Auto/Mission.
    virtual const char* auto_mode_name() const = 0;

    /// Get the mode name for SmartRTL (empty string if not supported).
    virtual const char* smart_rtl_mode_name() const = 0;

    /// Check if a mode name represents GUIDED mode.
    virtual bool is_guided_mode(const std::string& mode_name) const = 0;

    // ================================================================
    // Capabilities
    // ================================================================

    /// Get the capability flags for this firmware + vehicle combination.
    virtual uint32_t capabilities(VehicleKind vehicle) const = 0;

    // ================================================================
    // GUIDED Mode Parameters
    // ================================================================

    /// Minimum takeoff altitude in meters (AGL). Exposed to the frontend via
    /// `mg_firmware_get_type` as `minimum_takeoff_altitude_m`.
    virtual float minimum_takeoff_altitude(VehicleKind vehicle) const = 0;

    // ================================================================
    // Parameter Name Remapping (QGC FirmwarePlugin::remapParamNameMajorVersionMap_t)
    // ================================================================

    /// Remap a parameter name for the given firmware version.
    virtual std::string remap_param_name(VehicleKind vehicle, int major, int minor, const std::string& name) const = 0;

    /// Translate a firmware-era parameter name to the newest canonical name.
    virtual std::string canonical_param_name(VehicleKind vehicle, int major, int minor, const std::string& name) const = 0;

    /// Check if the plugin has parameter remapping support.
    virtual bool has_param_remapping() const = 0;
};

/// Factory: create the right plugin from detected firmware kind.
/// Returns nullptr for Unknown firmware.
std::unique_ptr<IFirmwarePlugin> create_firmware_plugin(FirmwareKind kind);

} // namespace mg::core::firmware

#endif // MGCORE_FIRMWARE_PLUGIN_H
