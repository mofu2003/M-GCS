/**
 * ardu_plane_modes.h - ArduPilot Plane flight mode table
 * ArduPlane 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduPlane/mode.h (Mode::Number enum)
 * Reference: QGC APMPlaneFirmwarePlugin, MP ArduPlane.cs
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* arduplane_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Manual";
        case 1:  return "Circle";
        case 2:  return "Stabilize";
        case 3:  return "Training";
        case 4:  return "Acro";
        case 5:  return "FBWA";
        case 6:  return "FBWB";
        case 7:  return "Cruise";
        case 8:  return "Autotune";
        case 10: return "Auto";
        case 11: return "RTL";
        case 12: return "Loiter";
        case 13: return "Takeoff";
        case 14: return "AvoidADSB";
        case 15: return "Guided";
        case 17: return "QStabilize";
        case 18: return "QHover";
        case 19: return "QLoiter";
        case 20: return "QLand";
        case 21: return "QRTL";
        case 22: return "QAutotune";
        case 23: return "QAcro";
        case 24: return "Thermal";
        case 25: return "Loiter2QLand";
        case 26: return "Autoland";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
