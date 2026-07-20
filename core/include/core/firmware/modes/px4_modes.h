/**
 * px4_modes.h - PX4 flight mode table
 * PX4 飞行模式 — main_mode/sub_mode 组合到名称的映射
 *
 * PX4 custom_mode encoding:
 *   struct { uint16_t reserved; uint8_t main_mode; uint8_t sub_mode; }
 *   main_mode = (custom_mode >> 16) & 0xFF
 *   sub_mode  = (custom_mode >> 24) & 0xFF
 *
 * Source: PX4/src/modules/commander/px4_custom_mode.h
 * Reference: QGC PX4FirmwarePlugin.cc
 */

#pragma once
#include <cstdint>
#include <string>

namespace mg::core::firmware::modes {

// PX4 main mode values (from px4_custom_mode.h)
enum Px4MainMode : uint8_t {
    PX4_MAIN_MODE_MANUAL       = 1,
    PX4_MAIN_MODE_ALTCTL       = 2,
    PX4_MAIN_MODE_POSCTL       = 3,
    PX4_MAIN_MODE_AUTO         = 4,
    PX4_MAIN_MODE_ACRO         = 5,
    PX4_MAIN_MODE_OFFBOARD     = 6,
    PX4_MAIN_MODE_STABILIZED   = 7,
    PX4_MAIN_MODE_RATTITUDE    = 8,
    PX4_MAIN_MODE_SIMPLE       = 9,
    PX4_MAIN_MODE_TERMINATION  = 10,
};

// PX4 auto sub-mode values
enum Px4AutoSubMode : uint8_t {
    PX4_AUTO_SUB_MODE_READY         = 1,
    PX4_AUTO_SUB_MODE_TAKEOFF       = 2,
    PX4_AUTO_SUB_MODE_LOITER        = 3,
    PX4_AUTO_SUB_MODE_MISSION       = 4,
    PX4_AUTO_SUB_MODE_RTL           = 5,
    PX4_AUTO_SUB_MODE_LAND          = 6,
    PX4_AUTO_SUB_MODE_RESERVED      = 7,
    PX4_AUTO_SUB_MODE_FOLLOW_TARGET = 8,
    PX4_AUTO_SUB_MODE_PRECLAND      = 9,
    PX4_AUTO_SUB_MODE_VTOL_TAKEOFF  = 10,
};

inline std::string decode_px4_mode(uint32_t custom_mode) {
    uint8_t main_mode = (custom_mode >> 16) & 0xFF;
    uint8_t sub_mode  = (custom_mode >> 24) & 0xFF;

    switch (main_mode) {
        case PX4_MAIN_MODE_MANUAL:       return "Manual";
        case PX4_MAIN_MODE_ALTCTL:       return "Altitude Control";
        case PX4_MAIN_MODE_POSCTL:       return "Position Control";
        case PX4_MAIN_MODE_ACRO:         return "Acro";
        case PX4_MAIN_MODE_OFFBOARD:     return "Offboard";
        case PX4_MAIN_MODE_STABILIZED:   return "Stabilized";
        case PX4_MAIN_MODE_RATTITUDE:    return "Rattitude";
        case PX4_MAIN_MODE_SIMPLE:       return "Simple";
        case PX4_MAIN_MODE_TERMINATION:  return "Termination";
        case PX4_MAIN_MODE_AUTO:
            switch (sub_mode) {
                case PX4_AUTO_SUB_MODE_READY:         return "Auto Ready";
                case PX4_AUTO_SUB_MODE_TAKEOFF:       return "Auto Takeoff";
                case PX4_AUTO_SUB_MODE_LOITER:        return "Auto Loiter";
                case PX4_AUTO_SUB_MODE_MISSION:       return "Auto Mission";
                case PX4_AUTO_SUB_MODE_RTL:           return "Auto RTL";
                case PX4_AUTO_SUB_MODE_LAND:          return "Auto Land";
                case PX4_AUTO_SUB_MODE_FOLLOW_TARGET: return "Auto Follow";
                case PX4_AUTO_SUB_MODE_PRECLAND:      return "Auto Precision Land";
                case PX4_AUTO_SUB_MODE_VTOL_TAKEOFF:  return "VTOL Takeoff";
                default:                              return "Auto";
            }
        default: return "";
    }
}

} // namespace mg::core::firmware::modes
