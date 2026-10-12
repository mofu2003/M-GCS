/**
 * ardu_sub_modes.h - ArduPilot Sub flight mode table
 * Sub 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduSub/mode.h (control_mode_t enum)
 * Reference: QGC ArduSubFirmwarePlugin
 *
 * 本文件是 Sub 模式的唯一事实来源：名称与可选列表都从下表派生。
 * custom_mode 是上游枚举值，不是下标：POSHOLD=16、MANUAL=19、MOTOR_DETECT=20、
 * SURFTRAK=21。can_be_set 逐条对齐 QGC ArduSubFirmwarePlugin 的
 * availableFlightModes 表（该表带 CanBeSet 列，MOTORDETECTION 为 false、其余 true）。
 */

#pragma once
#include "core/firmware/modes/ardu_mode_table.h"
#include <cstddef>
#include <cstdint>

namespace mg::core::firmware::modes {

inline const ArduModeEntry* ardupilot_sub_mode_table(size_t& count) {
    static const ArduModeEntry kTable[] = {
        { 0, "Stabilize",   true  },
        { 1, "Acro",        true  },
        { 2, "AltHold",     true  },
        { 3, "Auto",        true  },
        { 4, "Guided",      true  },
        { 7, "Circle",      true  },
        { 9, "Surface",     true  },
        {16, "PosHold",     true  },
        {19, "Manual",      true  },
        {20, "MotorDetect", false },  // QGC: CanBeSet=false
        {21, "SurfTrak",    true  },  // ArduSub/mode.h:53 SURFTRAK；QGC: CanBeSet=true
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

inline const char* ardupilot_sub_mode_name(uint32_t mode) {
    size_t count = 0;
    const ArduModeEntry* table = ardupilot_sub_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].custom_mode == mode) return table[i].name;
    }
    return nullptr;
}

} // namespace mg::core::firmware::modes
