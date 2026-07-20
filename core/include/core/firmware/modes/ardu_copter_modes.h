/**
 * ardu_copter_modes.h - ArduPilot Copter flight mode table
 * ArduCopter 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduCopter/mode.h (CopterMode enum)
 * Reference: QGC APMCopterFirmwarePlugin, MP ArduCopter.cs
 *
 * To add a new mode: add one case below. No other files need to change.
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* arducopter_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Stabilize";
        case 1:  return "Acro";
        case 2:  return "AltHold";
        case 3:  return "Auto";
        case 4:  return "Guided";
        case 5:  return "Loiter";
        case 6:  return "RTL";
        case 7:  return "Circle";
        case 8:  return "Land";
        case 9:  return "Drift";
        case 11: return "Sport";
        case 13: return "Flip";
        case 14: return "Autotune";
        case 15: return "PosHold";
        case 16: return "Brake";
        case 17: return "Throw";
        case 18: return "AvoidADSB";
        case 19: return "GuidedNoGPS";
        case 20: return "SmartRTL";
        case 21: return "FlowHold";
        case 22: return "Follow";
        case 23: return "Zigzag";
        case 24: return "SystemID";
        case 25: return "Autorotate";
        case 26: return "AutoRTL";
        case 27: return "Turtle";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
