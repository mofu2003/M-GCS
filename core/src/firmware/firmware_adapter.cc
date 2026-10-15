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
 *
 * References:
 *   QGC: src/FirmwarePlugin/APM/APMFirmwarePlugin.cc
 *   MP:  ExtLibs/ArduPilot/ArduPilot.cs
 */

#include "core/firmware/firmware_adapter.h"

#include <cctype>
#include "core/firmware/modes/ardu_copter_modes.h"
#include "core/firmware/modes/ardu_plane_modes.h"
#include "core/firmware/modes/ardu_rover_modes.h"
#include "core/firmware/modes/ardu_sub_modes.h"
#include "core/firmware/modes/ardu_tracker_modes.h"

namespace mg::core::firmware {

using namespace modes;

FirmwareAdapter::FirmwareAdapter() = default;
FirmwareAdapter::~FirmwareAdapter() = default;

void FirmwareAdapter::detect(uint8_t autopilot_type, uint8_t vehicle_type,
                             std::optional<float> quadplane_enable) {
    // Detect firmware kind (只有 ArduPilot)
    if (autopilot_type == 3) {  // MAV_AUTOPILOT_ARDUPILOTMEGA
        firmware_kind_ = FirmwareKind::ArduPilot;
    } else {
        firmware_kind_ = FirmwareKind::Unknown;
    }

    // Detect vehicle kind
    vehicle_kind_ = static_cast<VehicleKind>(vehicle_type);

    // MAV_TYPE 的 VTOL 家族是 19–25 连续七个值，而 VehicleKind 只声明了
    // VTOL = 19。上面这行强转对 19 之外的六个值都不会等于 VehicleKind::VTOL，
    // 于是 20–25 会掉出所有 switch(vehicle_kind())：pause_mode 退化成空串、
    // 命令目录的 vehicle_class 退化成空串（机型过滤空转，NAV_VTOL_TAKEOFF /
    // NAV_VTOL_LAND 对真 VTOL 重新不可见）。
    //
    // 这里把整个家族无条件收敛到 VehicleKind::VTOL，使下游只存在一种取值。
    // 谓词由 firmware_adapter.h 的 is_vtol_mav_type() 提供，不得在此重写区间。
    if (is_vtol_mav_type(vehicle_type)) {
        vehicle_kind_ = VehicleKind::VTOL;
    }

    // ArduPilot QuadPlane defaults to MAV_TYPE_FIXED_WING in HEARTBEAT. The
    // backend is the single authority for this distinction: once Q_ENABLE is
    // cached, promote the raw heartbeat type to the semantic VTOL kind.
    // 上面那条只对「显式声明 VTOL 身份」的机型生效；这一条补的是
    // Q_MAV_TYPE=0 的上游默认 QuadPlane——它上报 MAV_TYPE_FIXED_WING，
    // 身份只能靠参数判定。两者互不重叠：前者看 MAV_TYPE，后者看参数。
    if (firmware_kind_ == FirmwareKind::ArduPilot &&
        vehicle_type == static_cast<uint8_t>(VehicleKind::FixedWing) &&
        quadplane_enable.has_value() && *quadplane_enable > 0.0f) {
        vehicle_kind_ = VehicleKind::VTOL;
    }

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
            default:
                // VTOL 家族（19-25）跑 Plane 固件。
                if (is_vtol_mav_type(vehicle_type)) {
                    ardu_vehicle_ = ArduPilotVehicle::Plane;
                }
                // 其余值保持 Unknown：不认识的 MAV_TYPE 不声明任何机型，
                // 由调用方放弃下发（QGC vehicleClass 的 default → Generic 同义）。
                break;
        }
    }
}

FirmwareKind FirmwareAdapter::firmware_kind() const { return firmware_kind_; }
VehicleKind  FirmwareAdapter::vehicle_kind()  const { return vehicle_kind_; }

std::string FirmwareAdapter::firmware_name() const {
    switch (firmware_kind_) {
        case FirmwareKind::ArduPilot: return "ArduPilot";
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
                    if (is_vtol_mav_type(static_cast<uint8_t>(vehicle_kind_))) {
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
                if (is_vtol_mav_type(static_cast<uint8_t>(vehicle_kind_))) {
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
            if (is_vtol_mav_type(static_cast<uint8_t>(vehicle_kind_))) {
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
    }

    if (!info.valid) {
        info.mode_name = "Mode(" + std::to_string(custom_mode) + ")";
        info.valid = true;  // fallback is still "valid" as a display string
    }

    return info;
}

namespace {

/// 大小写不敏感比较：界面与 FFI 可能传来 "rtl" / "RTL"。
bool mode_name_equals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

/// decode 对未知值会返回 "Mode(N)" 兜底串，反查时必须跳过。
bool is_placeholder_mode_name(const std::string& name) {
    return name.rfind("Mode(", 0) == 0;
}

}  // namespace

std::optional<uint32_t> FirmwareAdapter::encode_flight_mode(const std::string& name) const {
    if (name.empty() || firmware_kind_ == FirmwareKind::Unknown) return std::nullopt;

    const auto matches = [this, &name](uint32_t custom_mode) {
        const std::string candidate = decode_flight_mode(custom_mode).mode_name;
        if (candidate.empty() || is_placeholder_mode_name(candidate)) return false;
        return mode_name_equals(candidate, name);
    };

    for (uint32_t mode = 0; mode <= 0xFF; ++mode) {
        if (matches(mode)) return mode;
    }
    return std::nullopt;
}

std::optional<uint32_t> FirmwareAdapter::resolve_mode_role(ModeRole role) const {
    const char* ardupilot_name = nullptr;
    switch (role) {
        case ModeRole::ReturnToLaunch: ardupilot_name = "RTL";    break;
        case ModeRole::Land:           ardupilot_name = "Land";   break;
        case ModeRole::Loiter:         ardupilot_name = "Loiter"; break;
    }

    if (firmware_kind_ == FirmwareKind::ArduPilot) {
        return ardupilot_name ? encode_flight_mode(ardupilot_name) : std::nullopt;
    }
    return std::nullopt;
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
