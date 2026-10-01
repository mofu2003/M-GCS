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
        default:
            // VTOL types (19-24) use Plane firmware
            if (static_cast<uint8_t>(v) >= 19 && static_cast<uint8_t>(v) <= 24) {
                return ApVehicle::Plane;
            }
            return ApVehicle::Copter; // default fallback
    }
}

// ================================================================
// Mode table for a vehicle sub-family
// ================================================================
struct ModeTableRow {
    uint32_t custom_mode;
    const char* name;
    bool can_be_set;
};

// Copter modes (complete from ardu_copter_modes.h)
static const ModeTableRow kCopterModes[] = {
    { 0, "Stabilize",   true  },
    { 1, "Acro",        true  },
    { 2, "AltHold",     true  },
    { 3, "Auto",        true  },
    { 4, "Guided",      true  },
    { 5, "Loiter",      true  },
    { 6, "RTL",         true  },
    { 7, "Circle",      true  },
    { 8, "Land",        true  },
    { 9, "Drift",       true  },
    {11, "Sport",       true  },
    {13, "Flip",        true  },
    {14, "Autotune",    true  },
    {15, "PosHold",     true  },
    {16, "Brake",       true  },
    {17, "Throw",       true  },
    {18, "AvoidADSB",   true  },
    {19, "GuidedNoGPS", true  },
    {20, "SmartRTL",    true  },
    {21, "FlowHold",    true  },
    {22, "Follow",      true  },
    {23, "Zigzag",      true  },
    {24, "SystemID",    false },
    {25, "Autorotate",  false },
    {26, "AutoRTL",     true  },
    {27, "Turtle",      true  },
};

// Plane modes (complete from ardu_plane_modes.h)
static const ModeTableRow kPlaneModes[] = {
    { 0, "Manual",       true  },
    { 1, "Circle",       true  },
    { 2, "Stabilize",    true  },
    { 3, "Training",     true  },
    { 4, "Acro",         true  },
    { 5, "FBWA",         true  },
    { 6, "FBWB",         true  },
    { 7, "Cruise",       true  },
    { 8, "Autotune",     true  },
    {10, "Auto",         true  },
    {11, "RTL",          true  },
    {12, "Loiter",       true  },
    {13, "Takeoff",      true  },
    {14, "AvoidADSB",    true  },
    {15, "Guided",       true  },
    {17, "QStabilize",   true  },
    {18, "QHover",       true  },
    {19, "QLoiter",      true  },
    {20, "QLand",        true  },
    {21, "QRTL",         true  },
    {22, "QAutotune",    true  },
    {23, "QAcro",        true  },
    {24, "Thermal",      false },
    {25, "Loiter2QLand", true  },
    {26, "Autoland",     true  },
};

// Rover modes (complete from ardu_rover_modes.h)
static const ModeTableRow kRoverModes[] = {
    { 0, "Manual",       true  },
    { 1, "Acro",         true  },
    { 2, "Steering",     true  },
    { 3, "Hold",         true  },
    { 4, "Loiter",       true  },
    { 5, "Follow",       true  },
    { 6, "Simple",       true  },
    { 7, "Dock",         true  },
    {10, "Auto",         true  },
    {11, "RTL",          true  },
    {12, "SmartRTL",     true  },
    {13, "Guided",       true  },
    {15, "Initialising", false },
    {16, "Autotune",     true  },
    {17, "PosHold",      true  },
    {18, "Brake",        true  },
};

// Sub modes (complete from ardu_sub_modes.h)
static const ModeTableRow kSubModes[] = {
    { 0, "Stabilize",    true  },
    { 1, "Acro",         true  },
    { 2, "AltHold",      true  },
    { 3, "Auto",         true  },
    { 4, "Guided",       true  },
    { 7, "Circle",       true  },
    { 9, "Surface",      true  },
    {16, "MotorDetect",  false },
    {19, "Manual",       true  },
};

// Tracker modes (complete from ardu_tracker_modes.h)
static const ModeTableRow kTrackerModes[] = {
    { 0, "Manual",     true  },
    { 1, "Stop",       true  },
    { 2, "Scan",       true  },
    { 3, "ServoTest",  false },
    { 4, "Auto",       true  },
    {10, "Guided",     true  },
};

// Get the mode table for a vehicle sub-family
static const ModeTableRow* get_mode_table(ApVehicle av, size_t& count) {
    switch (av) {
        case ApVehicle::Copter:  count = sizeof(kCopterModes) / sizeof(kCopterModes[0]); return kCopterModes;
        case ApVehicle::Plane:   count = sizeof(kPlaneModes) / sizeof(kPlaneModes[0]);   return kPlaneModes;
        case ApVehicle::Rover:   count = sizeof(kRoverModes) / sizeof(kRoverModes[0]);   return kRoverModes;
        case ApVehicle::Sub:     count = sizeof(kSubModes) / sizeof(kSubModes[0]);       return kSubModes;
        case ApVehicle::Tracker: count = sizeof(kTrackerModes) / sizeof(kTrackerModes[0]); return kTrackerModes;
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
    const ModeTableRow* table = get_mode_table(av, count);
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
    const ModeTableRow* table = get_mode_table(av, count);
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
    // All ArduPilot vehicles support these capabilities
    uint32_t caps = CapGuidedMode | CapPause | CapTakeoff | CapSetHome | CapCalibration;

    ApVehicle av = classify_ap_vehicle(vehicle);
    switch (av) {
        case ApVehicle::Copter:
            caps |= CapSmartRTL | CapMotorTest;
            break;
        case ApVehicle::Plane:
            caps |= CapMotorTest;
            break;
        case ApVehicle::Rover:
            caps |= CapSmartRTL;
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

float ArduPilotPlugin::default_loiter_radius(VehicleKind vehicle) const {
    ApVehicle av = classify_ap_vehicle(vehicle);
    switch (av) {
        case ApVehicle::Plane: return 100.0f;  // QGC default for fixed wing
        default:               return 50.0f;   // QGC default for copter
    }
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
