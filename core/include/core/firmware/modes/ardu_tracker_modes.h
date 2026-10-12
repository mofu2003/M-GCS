/**
 * ardu_tracker_modes.h - ArduPilot AntennaTracker flight mode table
 * AntennaTracker 模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/AntennaTracker/mode.h (enum class Mode)
 *
 * 本文件是 Tracker 模式的唯一事实来源：名称与可选列表都从下表派生。
 * custom_mode 是上游枚举值，不是下标——本表曾按下标编号把 STOP/SCAN/SERVOTEST
 * 整体前移一格，并把 GUIDED(4) 写成 Auto。上游：MANUAL=0 STOP=1 SCAN=2
 * SERVOTEST=3 GUIDED=4 AUTO=10 INITIALISING=16。
 *
 * To add a new mode: add one row below. No other files need to change.
 */

#pragma once
#include "core/firmware/modes/ardu_mode_table.h"
#include <cstddef>
#include <cstdint>

namespace mg::core::firmware::modes {

inline const ArduModeEntry* ardupilot_tracker_mode_table(size_t& count) {
    static const ArduModeEntry kTable[] = {
        { 0, "Manual",       true  },
        { 1, "Stop",         true  },
        { 2, "Scan",         true  },
        { 3, "ServoTest",    false },
        { 4, "Guided",       true  },
        {10, "Auto",         true  },
        {16, "Initialising", false },
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const char* ardupilot_tracker_mode_name(uint32_t mode) {
    size_t count = 0;
    const ArduModeEntry* table = ardupilot_tracker_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].custom_mode == mode) return table[i].name;
    }
    return nullptr;
}

} // namespace mg::core::firmware::modes
