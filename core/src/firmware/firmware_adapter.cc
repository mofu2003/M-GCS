/**
 * firmware_adapter.cc - Firmware detection + flight mode routing
 * 固件适配实现 — 固件类型检测与飞行模式表路由
 *
 * v0.9: Refactored to use separated mode tables (QGC FirmwarePlugin pattern).
 *       Each vehicle type's mode table is in its own header under modes/.
 *       FirmwareAdapter only handles detection + routing.
 *
 * Mode table sources:
 *   modes/ardu_copter_modes.h   - ArduCopter/mode.h
 *   modes/ardu_plane_modes.h    - ArduPlane/mode.h
 *   modes/ardu_rover_modes.h    - Rover/mode.h
 *   modes/ardu_sub_modes.h      - ArduSub/mode.h
 *   modes/ardu_tracker_modes.h  - AntennaTracker/mode.h
 *   modes/px4_modes.h           - px4_custom_mode.h
 *
 * References:
 *   QGC: src/FirmwarePlugin/APM/APMFirmwarePlugin.cc
 *   QGC: src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc
 *   MP:  ExtLibs/ArduPilot/ArduPilot.cs
 */

#include "core/firmware/firmware_adapter.h"
#include "core/firmware/modes/ardu_copter_modes.h"
#include "core/firmware/modes/ardu_plane_modes.h"
#include "core/firmware/modes/ardu_rover_modes.h"
#include "core/firmware/modes/ardu_sub_modes.h"
#include "core/firmware/modes/ardu_tracker_modes.h"
#include "core/firmware/modes/px4_modes.h"

namespace mg::core::firmware {

using namespace modes;

FirmwareAdapter::FirmwareAdapter() = default;
FirmwareAdapter::~FirmwareAdapter() = default;

void FirmwareAdapter::detect(uint8_t autopilot_type, uint8_t vehicle_type) {
    // Detect firmware kind
    switch (autopilot_type) {
        case 3:  firmware_kind_ = FirmwareKind::ArduPilot; break;  // MAV_AUTOPILOT_ARDUPILOTMEGA
        case 12: firmware_kind_ = FirmwareKind::PX4;       break;  // MAV_AUTOPILOT_PX4
        default: firmware_kind_ = FirmwareKind::Unknown;   break;
    }

    // Detect vehicle kind
    vehicle_kind_ = static_cast<VehicleKind>(vehicle_type);

    // For ArduPilot, determine the sub-family (Copter/Plane/Rover/Sub/Tracker)
    // Reference: QGC APMFirmwarePlugin::arduPilotFirmwareType()
    ardu_vehicle_ = ArduPilotVehicle::Unknown;
    if (firmware_kind_ == FirmwareKind::ArduPilot) {
        switch (vehicle_type) {
            case 1:   // MAV_TYPE_FIXED_WING
                ardu_vehicle_ = ArduPilotVehicle::Plane;
                break;
            case 2:   // Quadrotor
            case 13:  // Hexarotor
            case 14:  // Octorotor
            case 15:  // Tricopter
            case 3:   // Coaxial
            case 4:   // Helicopter
                ardu_vehicle_ = ArduPilotVehicle::Copter;
                break;
            case 10:  // GroundRover
                ardu_vehicle_ = ArduPilotVehicle::Rover;
                break;
            case 11:  // SurfaceBoat (Rover firmware handles boats)
                ardu_vehicle_ = ArduPilotVehicle::Rover;
                break;
            case 12:  // Submarine
                ardu_vehicle_ = ArduPilotVehicle::Sub;
                break;
            case 5:   // AntennaTracker
                ardu_vehicle_ = ArduPilotVehicle::Tracker;
                break;
            case 19:  // VTOL types (19-24) use Plane firmware
            case 20:
            case 21:
            case 22:
            case 23:
            case 24:
                ardu_vehicle_ = ArduPilotVehicle::Plane;
                break;
            default:
                break;
        }
    }
}

FirmwareKind FirmwareAdapter::firmware_kind() const { return firmware_kind_; }
VehicleKind  FirmwareAdapter::vehicle_kind()  const { return vehicle_kind_; }

std::string FirmwareAdapter::firmware_name() const {
    switch (firmware_kind_) {
        case FirmwareKind::ArduPilot: return "ArduPilot";
        case FirmwareKind::PX4:       return "PX4";
        default:                      return "Unknown";
    }
}

std::string FirmwareAdapter::vehicle_name() const {
    switch (ardu_vehicle_) {
        case ArduPilotVehicle::Copter:  return "Copter";
        case ArduPilotVehicle::Plane:   return "Plane";
        case ArduPilotVehicle::Rover:   return "Rover";
        case ArduPilotVehicle::Sub:     return "Sub";
        case ArduPilotVehicle::Tracker: return "Tracker";
        default:
            switch (vehicle_kind_) {
                case VehicleKind::FixedWing:   return "Fixed Wing";
                case VehicleKind::Quadrotor:   return "Quadrotor";
                case VehicleKind::Hexarotor:   return "Hexarotor";
                case VehicleKind::Octorotor:   return "Octorotor";
                case VehicleKind::Tricopter:   return "Tricopter";
                case VehicleKind::Helicopter:  return "Helicopter";
                case VehicleKind::GroundRover: return "Rover";
                case VehicleKind::SurfaceBoat: return "Boat";
                case VehicleKind::Submarine:   return "Submarine";
                default:
                    if (static_cast<uint8_t>(vehicle_kind_) >= 19 &&
                        static_cast<uint8_t>(vehicle_kind_) <= 24) {
                        return "VTOL";
                    }
                    return "Unknown";
            }
    }
}

std::string FirmwareAdapter::normalized_vehicle_name() const {
    if (firmware_kind_ == FirmwareKind::ArduPilot) {
        switch (ardu_vehicle_) {
            case ArduPilotVehicle::Copter:  return "Copter";
            case ArduPilotVehicle::Plane:
                if (static_cast<uint8_t>(vehicle_kind_) >= 19 &&
                    static_cast<uint8_t>(vehicle_kind_) <= 24) {
                    return "VTOL";
                }
                return "FixedWing";
            case ArduPilotVehicle::Rover:   return "Rover";
            case ArduPilotVehicle::Sub:     return "Submarine";
            case ArduPilotVehicle::Tracker: return "AntennaTracker";
            default: break;
        }
    }

    switch (vehicle_kind_) {
        case VehicleKind::FixedWing:
            return "FixedWing";
        case VehicleKind::Quadrotor:
        case VehicleKind::Coaxial:
        case VehicleKind::Helicopter:
        case VehicleKind::Hexarotor:
        case VehicleKind::Octorotor:
        case VehicleKind::Tricopter:
            return "Copter";
        case VehicleKind::GroundRover:
        case VehicleKind::SurfaceBoat:
            return "Rover";
        case VehicleKind::Submarine:
            return "Submarine";
        default:
            if (static_cast<uint8_t>(vehicle_kind_) >= 19 &&
                static_cast<uint8_t>(vehicle_kind_) <= 24) {
                return "VTOL";
            }
            return "Unknown";
    }
}

FlightModeInfo FirmwareAdapter::decode_flight_mode(uint32_t custom_mode) const {
    FlightModeInfo info;

    if (firmware_kind_ == FirmwareKind::ArduPilot) {
        info.mode_name = decode_ardupilot_mode(custom_mode);
        info.valid = !info.mode_name.empty();
    } else if (firmware_kind_ == FirmwareKind::PX4) {
        info.mode_name = decode_px4_mode(custom_mode);
        info.valid = !info.mode_name.empty();
    }

    if (!info.valid) {
        info.mode_name = "Mode(" + std::to_string(custom_mode) + ")";
        info.valid = true;  // fallback is still "valid" as a display string
    }

    return info;
}

std::string FirmwareAdapter::decode_ardupilot_mode(uint32_t custom_mode) const {
    const char* name = nullptr;

    // Route to the appropriate mode table (QGC FirmwarePlugin dispatch pattern)
    switch (ardu_vehicle_) {
        case ArduPilotVehicle::Copter:  name = arducopter_mode_name(custom_mode); break;
        case ArduPilotVehicle::Plane:   name = arduplane_mode_name(custom_mode); break;
        case ArduPilotVehicle::Rover:   name = ardupilot_rover_mode_name(custom_mode); break;
        case ArduPilotVehicle::Sub:     name = ardupilot_sub_mode_name(custom_mode); break;
        case ArduPilotVehicle::Tracker: name = ardupilot_tracker_mode_name(custom_mode); break;
        default: break;
    }

    return name ? std::string(name) : std::string();
}

} // namespace mg::core::firmware
