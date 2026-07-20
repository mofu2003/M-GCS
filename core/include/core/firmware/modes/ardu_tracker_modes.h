/**
 * ardu_tracker_modes.h - ArduPilot Tracker flight mode table
 * Tracker 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/AntennaTracker/mode.h
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* ardupilot_tracker_mode_name(uint32_t mode) {
    switch (mode) {
        case 0: return "Manual";
        case 1: return "Scan";
        case 2: return "ServoTest";
        case 4: return "Guided";
        case 10: return "Auto";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
