/**
 * ardu_tracker_modes.h - ArduPilot AntennaTracker flight mode table
 * AntennaTracker 模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/AntennaTracker/mode.h (enum class Mode)
 *
 * 注意：case 的数值必须是 Tracker 的**枚举值**，不是模式列表的下标。
 * 本表曾因按下标编号把 STOP(1) / SCAN(2) / SERVOTEST(3) 整体前移一格。
 *
 * To add a new mode: add one case below. No other files need to change.
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* ardupilot_tracker_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Manual";
        case 1:  return "Stop";
        case 2:  return "Scan";
        case 3:  return "ServoTest";
        case 4:  return "Guided";
        case 10: return "Auto";
        case 16: return "Initialising";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
