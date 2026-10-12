/**
 * ardu_copter_modes.h - ArduPilot Copter flight mode table
 * ArduCopter 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduCopter/mode.h (enum class Number)
 * Reference: QGC ArduCopterFirmwarePlugin, MP ArduCopter.cs
 *
 * 本文件是 ArduCopter 模式的**唯一事实来源**：模式名与可选列表都从下面的表派生。
 * 注意 custom_mode 必须是 ArduCopter 的**枚举值**，不是模式列表的下标——本表曾
 * 因按下标编号而从 LAND 起整体错位一格（9=LAND 被写成 Drift），导致选 "Drift"
 * 实际触发降落。Copter 未使用 8、10、12。
 *
 * To add a new mode: add one row below. No other files need to change.
 */

#pragma once
#include "core/firmware/modes/ardu_mode_table.h"
#include <cstddef>
#include <cstdint>

namespace mg::core::firmware::modes {

inline const ArduModeEntry* arducopter_mode_table(size_t& count) {
    static const ArduModeEntry kTable[] = {
        { 0, "Stabilize",   true  },
        { 1, "Acro",        true  },
        { 2, "AltHold",     true  },
        { 3, "Auto",        true  },
        { 4, "Guided",      true  },
        { 5, "Loiter",      true  },
        { 6, "RTL",         true  },
        { 7, "Circle",      true  },
        { 9, "Land",        true  },
        {11, "Drift",       true  },
        {13, "Sport",       true  },
        {14, "Flip",        true  },
        {15, "Autotune",    true  },
        {16, "PosHold",     true  },
        {17, "Brake",       true  },
        {18, "Throw",       true  },
        {19, "AvoidADSB",   true  },
        {20, "GuidedNoGPS", true  },
        {21, "SmartRTL",    true  },
        {22, "FlowHold",    true  },
        {23, "Follow",      true  },
        {24, "Zigzag",      true  },
        {25, "SystemID",    false },
        {26, "Autorotate",  false },
        {27, "AutoRTL",     true  },
        {28, "Turtle",      true  },
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const char* arducopter_mode_name(uint32_t mode) {
    size_t count = 0;
    const ArduModeEntry* table = arducopter_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].custom_mode == mode) return table[i].name;
    }
    return nullptr;
}

} // namespace mg::core::firmware::modes
