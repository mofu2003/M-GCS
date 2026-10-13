/**
 * firmware_plugin_ardupilot.cc - ArduPilot FirmwarePlugin implementation
 * ArduPilot 插件实现 — 所有机型的模式表和固件行为
 *
 * Mode tables sourced from:
 *   core/firmware/modes/ardu_copter_modes.h  (ArduCopter/mode.h)
 *   core/firmware/modes/ardu_plane_modes.h   (ArduPlane/mode.h)
 *   core/firmware/modes/ardu_rover_modes.h   (Rover/mode.h)
 *   core/firmware/modes/ardu_sub_modes.h     (ArduSub/mode.h)
 *   core/firmware/modes/ardu_tracker_modes.h (AntennaTracker/mode.h)
 *
 * QGC precedent: APMFirmwarePlugin.cc — uses same mode tables,
 *   but via FirmwarePlugin virtual methods instead of switch-cases.
 */

#include "core/firmware/ardupilot_plugin.h"
#include "core/firmware/modes/ardu_copter_modes.h"
#include "core/firmware/modes/ardu_plane_modes.h"
#include "core/firmware/modes/ardu_rover_modes.h"
#include "core/firmware/modes/ardu_sub_modes.h"
#include "core/firmware/modes/ardu_tracker_modes.h"
#include <cstring>
#include <algorithm>

namespace mg::core::firmware {

using namespace modes;

// ================================================================
// Helper: determine ArduPilot vehicle sub-family from VehicleKind
// (mirrors FirmwareAdapter::detect() logic)
// ================================================================
enum class ApVehicle { Unknown, Copter, Plane, Rover, Sub, Tracker };

static ApVehicle classify_ap_vehicle(VehicleKind v) {
    switch (v) {
        case VehicleKind::Quadrotor:
        case VehicleKind::Hexarotor:
        case VehicleKind::Octorotor:
        case VehicleKind::Tricopter:
        case VehicleKind::Coaxial:
        case VehicleKind::Helicopter:
            return ApVehicle::Copter;
        case VehicleKind::FixedWing:
            return ApVehicle::Plane;
        case VehicleKind::GroundRover:
        case VehicleKind::SurfaceBoat:
            return ApVehicle::Rover;
        case VehicleKind::Submarine:
            return ApVehicle::Sub;
        case VehicleKind::Tracker:
            return ApVehicle::Tracker;
        default:
            // VTOL 家族（19-25）跑 Plane 固件。
            if (is_vtol_mav_type(static_cast<uint8_t>(v))) {
                return ApVehicle::Plane;
            }
            // 未识别的 MAV_TYPE（含 >= 26 的 GIMBAL/ADSB/PARAFOIL）一律 Unknown：
            // 不得回退成 Copter，否则非飞行器会继承多旋翼的飞行动作能力。
            // QGC 同位置的做法是 vehicleClass 的 default → VehicleClassGeneric。
            return ApVehicle::Unknown;
    }
}

// ================================================================
// 模式表：直接复用 modes/ 头文件的单一事实来源。
// 这里不再内嵌第二张表——历史上内嵌表与头文件各存一份，且内嵌表落后于上游
// 枚举，导致 available_modes/encode 用错值（选 "Drift" 实际发 custom_mode=9=Land）。
// ================================================================
static const ArduModeEntry* get_mode_table(ApVehicle av, size_t& count) {
    switch (av) {
        case ApVehicle::Copter:  return arducopter_mode_table(count);
        case ApVehicle::Plane:   return arduplane_mode_table(count);
        case ApVehicle::Rover:   return ardupilot_rover_mode_table(count);
        case ApVehicle::Sub:     return ardupilot_sub_mode_table(count);
        case ApVehicle::Tracker: return ardupilot_tracker_mode_table(count);
        default:                 count = 0; return nullptr;
    }
}

// ================================================================
// IFirmwarePlugin implementation
// ================================================================

std::string ArduPilotPlugin::decode_flight_mode(VehicleKind vehicle, uint32_t custom_mode) const {
    ApVehicle av = classify_ap_vehicle(vehicle);
    // Use the existing mode table headers (single source of truth)
    const char* name = nullptr;
    switch (av) {
        case ApVehicle::Copter:  name = arducopter_mode_name(custom_mode); break;
        case ApVehicle::Plane:   name = arduplane_mode_name(custom_mode); break;
        case ApVehicle::Rover:   name = ardupilot_rover_mode_name(custom_mode); break;
        case ApVehicle::Sub:     name = ardupilot_sub_mode_name(custom_mode); break;
        case ApVehicle::Tracker: name = ardupilot_tracker_mode_name(custom_mode); break;
        case ApVehicle::Unknown: break;  // 无模式表：落到下面的 Mode(N) 兜底
    }
    return name ? std::string(name) : "Mode(" + std::to_string(custom_mode) + ")";
}

int ArduPilotPlugin::encode_flight_mode(VehicleKind vehicle, const std::string& mode_name) const {
    ApVehicle av = classify_ap_vehicle(vehicle);
    size_t count = 0;
    const ArduModeEntry* table = get_mode_table(av, count);
    if (!table) return -1;
    for (size_t i = 0; i < count; i++) {
        if (mode_name == table[i].name) {
            return static_cast<int>(table[i].custom_mode);
        }
    }
    return -1;
}

std::vector<FlightModeEntry> ArduPilotPlugin::available_modes(VehicleKind vehicle) const {
    ApVehicle av = classify_ap_vehicle(vehicle);
    size_t count = 0;
    const ArduModeEntry* table = get_mode_table(av, count);
    std::vector<FlightModeEntry> result;
    if (table) {
        result.reserve(count);
        for (size_t i = 0; i < count; i++) {
            result.push_back({table[i].name, table[i].custom_mode, table[i].can_be_set});
        }
    }
    return result;
}

uint32_t ArduPilotPlugin::capabilities(VehicleKind vehicle) const {
    // Keep the base capabilities conservative; vehicle-specific behavior is
    // added below. In particular, fixed-wing and rover must not advertise
    // guided takeoff just because the firmware is ArduPilot.
    // QGC `APMFirmwarePlugin::isCapable`（APMFirmwarePlugin.cc:65-83）把 ROI 放进
    // 基础集合，ChangeHeading 只给多旋翼和潜航器。CapOrbit 在 QGC APM 里没有对应
    // 能力位，但 M-GCS 的环绕走通用 DO_ORBIT，旋翼/固定翼/VTOL 都支持，因此这里
    // 只在确实支持 DO_ORBIT 的机型上声明，让 capability JSON 与地图入口一致。
    uint32_t caps = CapSetHome | CapCalibration | CapRoi;

    ApVehicle av = classify_ap_vehicle(vehicle);
    switch (av) {
        case ApVehicle::Copter:
            caps |= CapGuidedMode | CapPause | CapTakeoff | CapGuidedLand | CapSmartRTL |
                    CapMotorTest | CapChangeHeading | CapOrbit;
            break;
        case ApVehicle::Plane: {
            caps |= CapGuidedMode | CapPause | CapMotorTest | CapOrbit;
            // 只有 VTOL 家族能引导起飞/降落；普通固定翼不能。
            if (is_vtol_mav_type(static_cast<uint8_t>(vehicle))) {
                caps |= CapTakeoff | CapGuidedLand;
            }
            break;
        }
        case ApVehicle::Rover:
            caps |= CapGuidedMode | CapPause | CapSmartRTL;
            break;
        case ApVehicle::Sub:
            caps |= CapGuidedMode | CapPause | CapChangeHeading;
            break;
        case ApVehicle::Tracker:
            caps |= CapGuidedMode;
            break;
        default:
            break;
    }
    return caps;
}

float ArduPilotPlugin::minimum_takeoff_altitude(VehicleKind vehicle) const {
    // QGC APMFirmwarePlugin default: 10 feet = 3.048 meters
    // Copter default, Plane uses different logic via TAKEOFF command
    (void)vehicle;
    return 3.048f;
}

const char* ArduPilotPlugin::quadplane_enable_param_id() const {
    // ArduPlane 的 QuadPlane 总开关。取值 >0 时该固定翼实际上具备多旋翼形态。
    // 上游：ArduPlane/quadplane.cpp 的 Q_ENABLE；MP 的 isQuadPlane 也读它。
    return "Q_ENABLE";
}



} // namespace mg::core::firmware

// ================================================================
// Factory function
// ================================================================

namespace mg::core::firmware {

std::unique_ptr<IFirmwarePlugin> create_firmware_plugin(FirmwareKind kind) {
    if (kind == FirmwareKind::ArduPilot) {
        return std::make_unique<ArduPilotPlugin>();
    }
    return nullptr;
}

} // namespace mg::core::firmware
