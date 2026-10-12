/**
 * firmware_adapter.h - Firmware-specific flight mode mapping
 * 固件类型检测和适配 — 自动识别固件类型，路由飞行模式解码
 *
 * ArduPilot encodes flight modes in HEARTBEAT.custom_mode; this module
 * decodes custom_mode into human-readable mode names.
 *
 * Design:
 *   - FirmwareAdapter is the public entry point (auto-detects firmware)
 *   - ArduPilot mode mapping lives in modes/ardu_*_modes.h
 *
 * References:
 *   ArduPilot: ArduCopter/mode.h, ArduPlane/mode.h, Rover/mode.h, ArduSub/mode.h
 */

#ifndef MGCORE_FIRMWARE_ADAPTER_H
#define MGCORE_FIRMWARE_ADAPTER_H

#include "core/core_types.h"

namespace mg::core::firmware {

/// Autopilot firmware family (matches MAV_AUTOPILOT enum values)
enum class FirmwareKind : uint8_t {
    Unknown       = 0,
    ArduPilot     = 3,   // MAV_AUTOPILOT_ARDUPILOTMEGA
};

/// Vehicle type (matches MAV_TYPE enum values)
enum class VehicleKind : uint8_t {
    Unknown       = 0,
    FixedWing     = 1,   // MAV_TYPE_FIXED_WING
    Quadrotor     = 2,   // MAV_TYPE_QUADROTOR
    Coaxial       = 3,   // MAV_TYPE_COAXIAL
    Helicopter    = 4,   // MAV_TYPE_HELICOPTER
    Tracker       = 5,   // MAV_TYPE_ANTENNA_TRACKER
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

    /// Get firmware name string (e.g. "ArduPilot")
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

    /// Resolve a flight mode name (e.g. "RTL", "Land", "Loiter") to this
    /// firmware/vehicle's custom_mode value. nullopt when the mode does not
    /// exist for this combination (e.g. Rover has no Land).
    ///
    /// 由 decode_flight_mode 反查得到，因此两张表不可能各自漂移（S6：
    /// 模式号只能来自插件，通用代码不得写字面量）。
    std::optional<uint32_t> encode_flight_mode(const std::string& name) const;

    /// 语义化模式角色。通用代码只按角色请求，具体模式号与名称由插件决定
    /// （ArduPilot 用 "RTL"/"Land"/"Loiter"）。不属于该固件的角色返回
    /// nullopt，调用方必须放弃下发。
    enum class ModeRole { ReturnToLaunch, Land, Loiter };
    std::optional<uint32_t> resolve_mode_role(ModeRole role) const;

private:
    FirmwareKind firmware_kind_ = FirmwareKind::Unknown;
    VehicleKind  vehicle_kind_  = VehicleKind::Unknown;

    // ArduPilot: vehicle sub-type determines which mode table to use
    enum class ArduPilotVehicle { Unknown, Copter, Plane, Rover, Sub, Tracker };
    ArduPilotVehicle ardu_vehicle_ = ArduPilotVehicle::Unknown;

    std::string decode_ardupilot_mode(uint32_t custom_mode) const;
};

} // namespace mg::core::firmware

#endif // MGCORE_FIRMWARE_ADAPTER_H

