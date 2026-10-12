/**
 * ardu_plane_modes.h - ArduPilot Plane flight mode table
 * ArduPlane 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduPlane/mode.h (Mode::Number enum)
 * Reference: QGC ArduPlaneFirmwarePlugin, MP ArduPlane.cs
 *
 * 本文件是 ArduPlane 模式的唯一事实来源：名称与可选列表都从下表派生。
 * custom_mode 是上游枚举值，不是下标。Plane 未使用 9。
 *
 * To add a new mode: add one row below. No other files need to change.
 */

#pragma once
#include "core/firmware/modes/ardu_mode_table.h"
#include <cstddef>
#include <cstdint>

namespace mg::core::firmware::modes {

inline const ArduModeEntry* arduplane_mode_table(size_t& count) {
    static const ArduModeEntry kTable[] = {
        { 0, "Manual",       true  },
        { 1, "Circle",       true  },
        { 2, "Stabilize",    true  },
        { 3, "Training",     true  },
        { 4, "Acro",         true  },
        { 5, "FBWA",         true  },
        { 6, "FBWB",         true  },
        { 7, "Cruise",       true  },
        { 8, "Autotune",     true  },
        {10, "Auto",         true  },
        {11, "RTL",          true  },
        {12, "Loiter",       true  },
        {13, "Takeoff",      true  },
        {14, "AvoidADSB",    true  },
        {15, "Guided",       true  },
        {16, "Initialising", false },
        {17, "QStabilize",   true  },
        {18, "QHover",       true  },
        {19, "QLoiter",      true  },
        {20, "QLand",        true  },
        {21, "QRTL",         true  },
        {22, "QAutotune",    true  },
        {23, "QAcro",        true  },
        {24, "Thermal",      false },
        {25, "Loiter2QLand", true  },
        {26, "Autoland",     true  },
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const char* arduplane_mode_name(uint32_t mode) {
    size_t count = 0;
    const ArduModeEntry* table = arduplane_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].custom_mode == mode) return table[i].name;
    }
    return nullptr;
}

} // namespace mg::core::firmware::modes
