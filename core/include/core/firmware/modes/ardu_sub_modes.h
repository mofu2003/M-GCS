/**
 * ardu_sub_modes.h - ArduPilot Sub flight mode table
 * Sub 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduSub/mode.h (control_mode_t enum)
 * Reference: QGC APMSubFirmwarePlugin
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* ardupilot_sub_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Stabilize";
        case 1:  return "Acro";
        case 2:  return "AltHold";
        case 3:  return "Auto";
        case 4:  return "Guided";
        case 7:  return "Circle";
        case 9:  return "Surface";
        case 16: return "PosHold";
        case 19: return "Manual";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
