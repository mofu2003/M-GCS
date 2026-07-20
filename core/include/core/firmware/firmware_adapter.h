/**
 * firmware_adapter.h - Firmware-specific flight mode mapping
 * 固件类型检测和适配 — 自动识别固件类型，路由飞行模式解码
 *
 * Each firmware family (ArduPilot, PX4) encodes flight modes differently
 * in HEARTBEAT.custom_mode. This module provides a unified interface
 * to decode custom_mode into human-readable mode names.
 *
 * Design:
 *   - FirmwareAdapter is the public entry point (auto-detects firmware)
 *   - ArduPilotAdapter handles ArduPilot-specific mode mapping
 *   - PX4Adapter will be added later
 *
 * References:
 *   ArduPilot: ArduCopter/mode.h, ArduPlane/mode.h, Rover/mode.h, ArduSub/mode.h
 *   PX4: px4_custom_mode.h
 */

#ifndef MGCORE_FIRMWARE_ADAPTER_H
#define MGCORE_FIRMWARE_ADAPTER_H

#include "core/core_types.h"

namespace mg::core::firmware {

/// Autopilot firmware family (matches MAV_AUTOPILOT enum values)
enum class FirmwareKind : uint8_t {
    Unknown       = 0,
    ArduPilot     = 3,   // MAV_AUTOPILOT_ARDUPILOTMEGA
    PX4           = 12,  // MAV_AUTOPILOT_PX4
};

/// Vehicle type (matches MAV_TYPE enum values)
enum class VehicleKind : uint8_t {
    Unknown       = 0,
    FixedWing     = 1,   // MAV_TYPE_FIXED_WING
    Quadrotor     = 2,   // MAV_TYPE_QUADROTOR
    Coaxial       = 3,   // MAV_TYPE_COAXIAL
    Helicopter    = 4,   // MAV_TYPE_HELICOPTER
    GroundRover   = 10,  // MAV_TYPE_GROUND_ROVER
    SurfaceBoat   = 11,  // MAV_TYPE_SURFACE_BOAT
    Submarine     = 12,  // MAV_TYPE_SUBMARINE
    Hexarotor     = 13,  // MAV_TYPE_HEXAROTOR
    Octorotor     = 14,  // MAV_TYPE_OCTOROTOR
    Tricopter     = 15,  // MAV_TYPE_TRICOPTER
    VTOL          = 19,  // MAV_TYPE_VTOL_TAILSITTER_DUOROTOR (and all VTOL variants 19-24)
};

/// Decoded flight mode information
struct FlightModeInfo {
    std::string mode_name;       // Human-readable name (e.g. "Loiter", "RTL")
    bool        valid = false;   // Whether the mode was successfully decoded
};

/**
 * FirmwareAdapter - Unified flight mode decoder
 *
 * Usage:
 *   FirmwareAdapter adapter;
 *   adapter.detect(autopilot_type, vehicle_type);  // from HEARTBEAT
 *   auto info = adapter.decode_flight_mode(custom_mode);  // from HEARTBEAT
 */
class FirmwareAdapter {
public:
    FirmwareAdapter();
    ~FirmwareAdapter();

    /// Detect firmware and vehicle type from HEARTBEAT fields.
    /// autopilot_type = HEARTBEAT.autopilot (MAV_AUTOPILOT_xxx)
    /// vehicle_type   = HEARTBEAT.type (MAV_TYPE_xxx)
    void detect(uint8_t autopilot_type, uint8_t vehicle_type);

    /// Get detected firmware kind
    FirmwareKind firmware_kind() const;

    /// Get detected vehicle kind
    VehicleKind vehicle_kind() const;

    /// Get firmware name string (e.g. "ArduPilot", "PX4")
    std::string firmware_name() const;

    /// Get vehicle type string (e.g. "Copter", "Plane", "Rover")
    std::string vehicle_name() const;

    /// Get normalized vehicle family string for UI/FFI consumption.
    /// Returns one of: FixedWing, Copter, Rover, Submarine, VTOL,
    /// AntennaTracker, Unknown.
    std::string normalized_vehicle_name() const;

    /// Decode custom_mode into a human-readable flight mode name.
    /// Returns FlightModeInfo with mode_name and valid flag.
    FlightModeInfo decode_flight_mode(uint32_t custom_mode) const;

private:
    FirmwareKind firmware_kind_ = FirmwareKind::Unknown;
    VehicleKind  vehicle_kind_  = VehicleKind::Unknown;

    // ArduPilot: vehicle sub-type determines which mode table to use
    enum class ArduPilotVehicle { Unknown, Copter, Plane, Rover, Sub, Tracker };
    ArduPilotVehicle ardu_vehicle_ = ArduPilotVehicle::Unknown;

    std::string decode_ardupilot_mode(uint32_t custom_mode) const;

    // PX4 decode (in modes/px4_modes.h, called from decode_flight_mode)
    
};

} // namespace mg::core::firmware

#endif // MGCORE_FIRMWARE_ADAPTER_H

