/**
 * firmware_plugin_px4.cc - PX4 FirmwarePlugin implementation
 * PX4 插件实现 — main_mode/sub_mode 组合编码解码
 *
 * PX4 encodes flight modes as: custom_mode = (main_mode << 16) | (sub_mode << 24)
 * Main modes: Manual(1), AltCtl(2), PosCtl(3), Auto(4), Acro(5), Offboard(6), Stabilized(7), Rattitude(8)
 * Auto sub-modes: Ready(1), Takeoff(2), Loiter(3), Mission(4), RTL(5), Land(6), RTGS(7), FollowTarget(8), Precland(9)
 *
 * QGC precedent: PX4FirmwarePlugin.cc
 */

#include "core/firmware/px4_plugin.h"
#include <map>

namespace mg::core::firmware {

// PX4 main mode values (from px4_custom_mode.h)
static constexpr uint8_t kPX4MainModeManual     = 1;
static constexpr uint8_t kPX4MainModeAltCtl     = 2;
static constexpr uint8_t kPX4MainModePosCtl     = 3;
static constexpr uint8_t kPX4MainModeAuto       = 4;
static constexpr uint8_t kPX4MainModeAcro       = 5;
static constexpr uint8_t kPX4MainModeOffboard   = 6;
static constexpr uint8_t kPX4MainModeStabilized = 7;
static constexpr uint8_t kPX4MainModeRattitude  = 8;

// PX4 auto sub-mode values
static constexpr uint8_t kPX4SubModeAutoReady        = 1;
static constexpr uint8_t kPX4SubModeAutoTakeoff      = 2;
static constexpr uint8_t kPX4SubModeAutoLoiter       = 3;
static constexpr uint8_t kPX4SubModeAutoMission      = 4;
static constexpr uint8_t kPX4SubModeAutoRTL          = 5;
static constexpr uint8_t kPX4SubModeAutoLand         = 6;
static constexpr uint8_t kPX4SubModeAutoRTGS         = 7;
static constexpr uint8_t kPX4SubModeAutoFollowTarget = 8;
static constexpr uint8_t kPX4SubModeAutoPrecland     = 9;

// Helper: extract main_mode and sub_mode from PX4 custom_mode
static void decode_px4_custom(uint32_t custom_mode, uint8_t& main_mode, uint8_t& sub_mode) {
    main_mode = static_cast<uint8_t>((custom_mode >> 16) & 0xFF);
    sub_mode  = static_cast<uint8_t>((custom_mode >> 24) & 0xFF);
}

// Helper: encode PX4 custom_mode from main_mode and sub_mode
static uint32_t encode_px4_custom(uint8_t main_mode, uint8_t sub_mode = 0) {
    return (static_cast<uint32_t>(main_mode) << 16) | (static_cast<uint32_t>(sub_mode) << 24);
}

// ================================================================
// PX4 mode table
// ================================================================
struct PX4ModeEntry {
    uint8_t main_mode;
    uint8_t sub_mode;   // 0 = no sub-mode
    const char* name;
    bool can_be_set;
};

static const PX4ModeEntry kPX4Modes[] = {
    {kPX4MainModeManual,     0, "Manual",           true },
    {kPX4MainModeAltCtl,     0, "Altitude Control", true },
    {kPX4MainModePosCtl,     0, "Position Control", true },
    {kPX4MainModeAcro,       0, "Acro",             true },
    {kPX4MainModeOffboard,   0, "Offboard",         true },
    {kPX4MainModeStabilized, 0, "Stabilized",       true },
    {kPX4MainModeRattitude,  0, "Rattitude",        true },
    // Auto sub-modes
    {kPX4MainModeAuto, kPX4SubModeAutoReady,        "Auto Ready",     false },
    {kPX4MainModeAuto, kPX4SubModeAutoTakeoff,      "Auto Takeoff",   false },
    {kPX4MainModeAuto, kPX4SubModeAutoLoiter,       "Auto Loiter",    true },
    {kPX4MainModeAuto, kPX4SubModeAutoMission,      "Auto Mission",   true },
    {kPX4MainModeAuto, kPX4SubModeAutoRTL,          "Auto RTL",       true },
    {kPX4MainModeAuto, kPX4SubModeAutoLand,         "Auto Land",      true },
    {kPX4MainModeAuto, kPX4SubModeAutoRTGS,         "Auto RTGS",      false },
    {kPX4MainModeAuto, kPX4SubModeAutoFollowTarget, "Follow Target",  false },
    {kPX4MainModeAuto, kPX4SubModeAutoPrecland,     "Precision Land", false },
};

static constexpr size_t kPX4ModeCount = sizeof(kPX4Modes) / sizeof(kPX4Modes[0]);

// ================================================================
// IFirmwarePlugin implementation
// ================================================================

std::string PX4Plugin::decode_flight_mode(VehicleKind /*vehicle*/, uint32_t custom_mode) const {
    uint8_t main_mode = 0, sub_mode = 0;
    decode_px4_custom(custom_mode, main_mode, sub_mode);

    for (size_t i = 0; i < kPX4ModeCount; i++) {
        if (kPX4Modes[i].main_mode == main_mode && kPX4Modes[i].sub_mode == sub_mode) {
            return kPX4Modes[i].name;
        }
    }
    return "Mode(" + std::to_string(custom_mode) + ")";
}

int PX4Plugin::encode_flight_mode(VehicleKind /*vehicle*/, const std::string& mode_name) const {
    for (size_t i = 0; i < kPX4ModeCount; i++) {
        if (mode_name == kPX4Modes[i].name) {
            return static_cast<int>(encode_px4_custom(kPX4Modes[i].main_mode, kPX4Modes[i].sub_mode));
        }
    }
    return -1;
}

std::vector<FlightModeEntry> PX4Plugin::available_modes(VehicleKind /*vehicle*/) const {
    std::vector<FlightModeEntry> result;
    result.reserve(kPX4ModeCount);
    for (size_t i = 0; i < kPX4ModeCount; i++) {
        uint32_t cm = encode_px4_custom(kPX4Modes[i].main_mode, kPX4Modes[i].sub_mode);
        result.push_back({kPX4Modes[i].name, cm, kPX4Modes[i].can_be_set});
    }
    return result;
}

uint32_t PX4Plugin::capabilities(VehicleKind /*vehicle*/) const {
    // PX4 supports guided mode via Offboard, pause, takeoff
    return CapGuidedMode | CapPause | CapTakeoff | CapSetHome | CapCalibration;
}

float PX4Plugin::minimum_takeoff_altitude(VehicleKind /*vehicle*/) const {
    // PX4 default minimum takeoff altitude: 2.5 meters
    return 2.5f;
}

float PX4Plugin::default_loiter_radius(VehicleKind /*vehicle*/) const {
    // PX4 default loiter radius: 50 meters
    return 50.0f;
}

} // namespace mg::core::firmware
