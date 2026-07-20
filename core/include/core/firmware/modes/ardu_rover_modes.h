/**
 * ardu_rover_modes.h - ArduPilot Rover flight mode table
 * Rover 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/Rover/mode.h (Mode::Number enum)
 * Reference: QGC APMRoverFirmwarePlugin, MP ArduRover.cs
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* ardupilot_rover_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Manual";
        case 1:  return "Acro";
        case 2:  return "Steering";
        case 3:  return "Hold";
        case 4:  return "Loiter";
        case 5:  return "Follow";
        case 6:  return "Simple";
        case 7:  return "Dock";
        case 10: return "Auto";
        case 11: return "RTL";
        case 12: return "SmartRTL";
        case 13: return "Guided";
        case 15: return "Initialising";
        case 16: return "Autotune";
        case 17: return "PosHold";
        case 18: return "Brake";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
