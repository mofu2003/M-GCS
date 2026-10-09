/**
 * ardu_rover_modes.h - ArduPilot Rover flight mode table
 * ArduRover 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/Rover/mode.h (enum class Mode)
 *
 * 注意：case 的数值必须是 Rover 的**枚举值**，不是模式列表的下标。
 * 本表曾因按下标编号而整体错位，并且混入了 Rover 根本不存在的 Autotune /
 * PosHold / Brake；错位后 LAND 位置上的 9 在 Rover 里其实是 CIRCLE。
 * Rover 未使用 2、13、14、17 及以后。
 *
 * To add a new mode: add one case below. No other files need to change.
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* ardupilot_rover_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Manual";
        case 1:  return "Acro";
        case 3:  return "Steering";
        case 4:  return "Hold";
        case 5:  return "Loiter";
        case 6:  return "Follow";
        case 7:  return "Simple";
        case 8:  return "Dock";
        case 9:  return "Circle";
        case 10: return "Auto";
        case 11: return "RTL";
        case 12: return "SmartRTL";
        case 15: return "Guided";
        case 16: return "Initialising";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
