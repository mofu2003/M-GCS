/**
 * ardu_rover_modes.h - ArduPilot Rover flight mode table
 * ArduRover 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/Rover/mode.h (enum class Mode)
 *
 * 本文件是 Rover 模式的唯一事实来源：名称与可选列表都从下表派生。
 * custom_mode 是上游枚举值，不是下标——本表曾按下标编号而整体错位，并在
 * LAND 的位置混入 Rover 根本不存在的 Autotune/PosHold/Brake（9 其实是 Circle）。
 * Rover 未使用 2、13、14、17 及以后。
 *
 * To add a new mode: add one row below. No other files need to change.
 */

#pragma once
#include "core/firmware/modes/ardu_mode_table.h"
#include <cstddef>
#include <cstdint>

namespace mg::core::firmware::modes {

inline const ArduModeEntry* ardupilot_rover_mode_table(size_t& count) {
    static const ArduModeEntry kTable[] = {
        { 0, "Manual",       true  },
        { 1, "Acro",         true  },
        { 3, "Steering",     true  },
        { 4, "Hold",         true  },
        { 5, "Loiter",       true  },
        { 6, "Follow",       true  },
        { 7, "Simple",       true  },
        { 8, "Dock",         true  },
        { 9, "Circle",       true  },
        {10, "Auto",         true  },
        {11, "RTL",          true  },
        {12, "SmartRTL",     true  },
        {15, "Guided",       true  },
        {16, "Initialising", false },
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const char* ardupilot_rover_mode_name(uint32_t mode) {
    size_t count = 0;
    const ArduModeEntry* table = ardupilot_rover_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].custom_mode == mode) return table[i].name;
    }
    return nullptr;
}

} // namespace mg::core::firmware::modes
