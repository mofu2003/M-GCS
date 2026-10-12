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
enum class ApVehicle { Copter, Plane, Rover, Sub, Tracker };

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
            // VTOL types (19-24) use Plane firmware
            if (static_cast<uint8_t>(v) >= 19 && static_cast<uint8_t>(v) <= 24) {
                return ApVehicle::Plane;
            }
            return ApVehicle::Copter; // default fallback
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
            caps |= CapGuidedMode | CapPause | CapTakeoff | CapSmartRTL |
                    CapMotorTest | CapChangeHeading | CapOrbit;
            break;
        case ApVehicle::Plane: {
            caps |= CapGuidedMode | CapPause | CapMotorTest | CapOrbit;
            const uint8_t mav_type = static_cast<uint8_t>(vehicle);
            if (mav_type >= 19 && mav_type <= 24) {
                caps |= CapTakeoff;  // VTOL takeoff
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



// ================================================================
// Parameter remapping implementation
// ================================================================

void ArduPilotPlugin::ensure_remap_loaded(VehicleKind vehicle) const {
    mg::core::platform::LockGuard lk(remap_mtx_);
    if (remap_loaded_) return;
    ApVehicle av = classify_ap_vehicle(vehicle);
    switch (av) {
        case ApVehicle::Copter:  remap_.load_ardupilot_copter_tables(); break;
        case ApVehicle::Plane:   remap_.load_ardupilot_plane_tables(); break;
        case ApVehicle::Rover:   remap_.load_ardupilot_rover_tables(); break;
        default:                 remap_.load_ardupilot_copter_tables(); break;
    }
    remap_loaded_ = true;
}

std::string ArduPilotPlugin::remap_param_name(VehicleKind vehicle, int major, int minor, const std::string& name) const {
    ensure_remap_loaded(vehicle);
    return remap_.remap(major, minor, name);
}

std::string ArduPilotPlugin::canonical_param_name(VehicleKind vehicle, int major, int minor, const std::string& name) const {
    ensure_remap_loaded(vehicle);
    return remap_.canonical_name(major, minor, name);
}

bool ArduPilotPlugin::has_param_remapping() const { return true; }

} // namespace mg::core::firmware

// ================================================================
// Factory function
// ================================================================
#include "core/firmware/px4_plugin.h"

namespace mg::core::firmware {

std::unique_ptr<IFirmwarePlugin> create_firmware_plugin(FirmwareKind kind) {
    switch (kind) {
        case FirmwareKind::ArduPilot:
            return std::make_unique<ArduPilotPlugin>();
        case FirmwareKind::PX4:
            return std::make_unique<PX4Plugin>();
        default:
            return nullptr;
    }
}

} // namespace mg::core::firmware
