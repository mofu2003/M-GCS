/**
 * px4_modes.h - PX4 flight mode table (single source of truth)
 * PX4 飞行模式 — main_mode/sub_mode 组合到名称的唯一映射表
 *
 * PX4 custom_mode encoding:
 *   struct { uint16_t reserved; uint8_t main_mode; uint8_t sub_mode; }
 *   main_mode = (custom_mode >> 16) & 0xFF
 *   sub_mode  = (custom_mode >> 24) & 0xFF
 *
 * Source: PX4-Autopilot/src/modules/commander/px4_custom_mode.h
 * Reference: QGC src/FirmwarePlugin/PX4/PX4FirmwarePlugin.cc
 *
 * 本文件是 PX4 模式的**唯一事实来源**：PX4Plugin（可用模式列表、编解码）与
 * FirmwareAdapter（状态栏解码、模式角色反查）都从这里取表，不得再各写一份。
 * 历史上这里和 firmware_plugin_px4.cc 各有一张表，命名与集合都漂移过。
 *
 * 文件里有两张不同含义的表，不要混淆：
 *   1. px4_mode_table()        —— MAVLink custom_mode（main/sub）到模式名。
 *   2. px4_fltmode_slot_table() —— 参数 COM_FLTMODE1..6 的取值枚举（RC 开关映射），
 *      数值与 custom_mode 无关。
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace mg::core::firmware::modes {

// ================================================================
// PX4 custom_mode enumerations
// ================================================================

// PX4 main mode values (from px4_custom_mode.h)
enum Px4MainMode : uint8_t {
    PX4_MAIN_MODE_MANUAL          = 1,
    PX4_MAIN_MODE_ALTCTL          = 2,
    PX4_MAIN_MODE_POSCTL          = 3,
    PX4_MAIN_MODE_AUTO            = 4,
    PX4_MAIN_MODE_ACRO            = 5,
    PX4_MAIN_MODE_OFFBOARD        = 6,
    PX4_MAIN_MODE_STABILIZED      = 7,
    PX4_MAIN_MODE_RATTITUDE       = 8,  // 上游：RATTITUDE_DEPRECATED
    PX4_MAIN_MODE_SIMPLE          = 9,  // 上游保留值，未启用
    PX4_MAIN_MODE_TERMINATION     = 10,
    PX4_MAIN_MODE_ALTITUDE_CRUISE = 11, // PX4 1.14+（固定翼）
};

// PX4 POSCTL sub-mode values
enum Px4PosctlSubMode : uint8_t {
    PX4_POSCTL_SUB_MODE_POSCTL = 0,
    PX4_POSCTL_SUB_MODE_ORBIT  = 1,
    PX4_POSCTL_SUB_MODE_SLOW   = 2,
};

// PX4 auto sub-mode values
enum Px4AutoSubMode : uint8_t {
    PX4_AUTO_SUB_MODE_READY         = 1,
    PX4_AUTO_SUB_MODE_TAKEOFF       = 2,
    PX4_AUTO_SUB_MODE_LOITER        = 3,
    PX4_AUTO_SUB_MODE_MISSION       = 4,
    PX4_AUTO_SUB_MODE_RTL           = 5,
    PX4_AUTO_SUB_MODE_LAND          = 6,
    // 7 曾是 RTGS，PX4 于 2020-03-05 删除，编号保留、不可设置、不再出现在模式表里。
    PX4_AUTO_SUB_MODE_RESERVED_DO_NOT_USE = 7,
    PX4_AUTO_SUB_MODE_FOLLOW_TARGET = 8,
    PX4_AUTO_SUB_MODE_PRECLAND      = 9,
    PX4_AUTO_SUB_MODE_VTOL_TAKEOFF  = 10,
    PX4_AUTO_SUB_MODE_EXTERNAL1     = 11, // 外部模式 1..8：由外部控制程序占用
    PX4_AUTO_SUB_MODE_EXTERNAL2     = 12,
    PX4_AUTO_SUB_MODE_EXTERNAL3     = 13,
    PX4_AUTO_SUB_MODE_EXTERNAL4     = 14,
    PX4_AUTO_SUB_MODE_EXTERNAL5     = 15,
    PX4_AUTO_SUB_MODE_EXTERNAL6     = 16,
    PX4_AUTO_SUB_MODE_EXTERNAL7     = 17,
    PX4_AUTO_SUB_MODE_EXTERNAL8     = 18,
    PX4_AUTO_SUB_MODE_GUIDED_COURSE = 19,
};

// ================================================================
// Unified mode table
// ================================================================

struct Px4ModeEntry {
    uint8_t     main_mode;
    uint8_t     sub_mode;    // 0 = 无子模式
    const char* name;
    bool        can_be_set;  // 用户能否直接切到这个模式
};

/// 唯一的 PX4 模式表。命名沿用 M-GCS 既有约定：AUTO 的子模式统一带 "Auto "
/// 前缀（Auto Loiter / Auto RTL / ...），与 QGC 的 "Hold"/"Return" 简写不同，
/// 以避免同时改动状态栏文案与前端依赖。差异已记录在 docs/features/px4-parity.md。
inline const Px4ModeEntry* px4_mode_table(size_t& count) {
    static const Px4ModeEntry kTable[] = {
        // ---- 主模式 ----
        {PX4_MAIN_MODE_MANUAL,          0,                              "Manual",             true },
        {PX4_MAIN_MODE_ALTCTL,          0,                              "Altitude Control",   true },
        {PX4_MAIN_MODE_POSCTL,          PX4_POSCTL_SUB_MODE_POSCTL,     "Position Control",   true },
        {PX4_MAIN_MODE_POSCTL,          PX4_POSCTL_SUB_MODE_ORBIT,      "Orbit",              false},
        {PX4_MAIN_MODE_POSCTL,          PX4_POSCTL_SUB_MODE_SLOW,       "Position Slow",      true },
        {PX4_MAIN_MODE_ACRO,            0,                              "Acro",               true },
        {PX4_MAIN_MODE_OFFBOARD,        0,                              "Offboard",           true },
        {PX4_MAIN_MODE_STABILIZED,      0,                              "Stabilized",         true },
        {PX4_MAIN_MODE_RATTITUDE,       0,                              "Rattitude",          true },
        {PX4_MAIN_MODE_SIMPLE,          0,                              "Simple",             false},
        {PX4_MAIN_MODE_TERMINATION,     0,                              "Termination",        false},
        {PX4_MAIN_MODE_ALTITUDE_CRUISE, 0,                              "Altitude Cruise",    true },
        // ---- AUTO 子模式 ----
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_READY,         "Auto Ready",           false},
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_TAKEOFF,       "Auto Takeoff",         false},
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_LOITER,        "Auto Loiter",          true },
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_MISSION,       "Auto Mission",         true },
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_RTL,           "Auto RTL",             true },
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_LAND,          "Auto Land",            true },
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_FOLLOW_TARGET, "Auto Follow",          false},
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_PRECLAND,      "Auto Precision Land",  true },
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_VTOL_TAKEOFF,  "VTOL Takeoff",         false},
        {PX4_MAIN_MODE_AUTO, PX4_AUTO_SUB_MODE_GUIDED_COURSE, "Auto Guided Course",   false},
        // 注意：AUTO 子模式 7（原 RTGS）与 11..18（外部模式）不在表内。它们不是
        // 用户可选的飞行模式，解码时走 "Mode(N)" 兜底。
    };
    count = sizeof(kTable) / sizeof(kTable[0]);
    return kTable;
}

/// main_mode/sub_mode 打包成 MAVLink custom_mode。
inline uint32_t encode_px4_custom_mode(uint8_t main_mode, uint8_t sub_mode = 0) {
    return (static_cast<uint32_t>(main_mode) << 16) |
           (static_cast<uint32_t>(sub_mode) << 24);
}

/// custom_mode → 模式名。未命中返回空串，由调用方兜底成 "Mode(N)"。
inline std::string decode_px4_mode(uint32_t custom_mode) {
    const uint8_t main_mode = static_cast<uint8_t>((custom_mode >> 16) & 0xFF);
    const uint8_t sub_mode  = static_cast<uint8_t>((custom_mode >> 24) & 0xFF);

    size_t count = 0;
    const Px4ModeEntry* table = px4_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].main_mode == main_mode && table[i].sub_mode == sub_mode) {
            return table[i].name;
        }
    }
    return std::string();
}

/// 模式名 → custom_mode；找不到返回 -1。
inline int encode_px4_mode_name(const std::string& name) {
    size_t count = 0;
    const Px4ModeEntry* table = px4_mode_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (name == table[i].name) {
            return static_cast<int>(encode_px4_custom_mode(table[i].main_mode, table[i].sub_mode));
        }
    }
    return -1;
}

// ================================================================
// COM_FLTMODE1..6 slot enum
// ================================================================
//
// COM_FLTMODE1..6 把 RC 开关档位映射到飞行模式，取值是 PX4 自己的参数枚举，
// **不是**上面的 custom_mode：例如参数值 7 = Offboard，而 custom_mode 的
// Offboard 是 0x00060000。数值来自 PX4 参数元数据（BSD-3，随 PX4-Autopilot
// 分发），QGC 侧同样以该元数据的 enum 文案展示。

struct Px4FlightModeSlotEntry {
    int         value;
    const char* name;
};

inline const Px4FlightModeSlotEntry* px4_fltmode_slot_table(size_t& count) {
    static const Px4FlightModeSlotEntry kSlots[] = {
        {-1,  "Unassigned"},
        {0,   "Manual"},
        {1,   "Altitude"},
        {2,   "Position"},
        {3,   "Mission"},
        {4,   "Hold"},
        {5,   "Return"},
        {6,   "Acro"},
        {7,   "Offboard"},
        {8,   "Stabilized"},
        {9,   "Position Slow"},
        {10,  "Takeoff"},
        {11,  "Land"},
        {12,  "Follow Me"},
        {13,  "Precision Land"},
        {16,  "Altitude Cruise"},
        {100, "External Mode 1"},
        {101, "External Mode 2"},
        {102, "External Mode 3"},
        {103, "External Mode 4"},
        {104, "External Mode 5"},
        {105, "External Mode 6"},
        {106, "External Mode 7"},
        {107, "External Mode 8"},
    };
    count = sizeof(kSlots) / sizeof(kSlots[0]);
    return kSlots;
}

/// COM_FLTMODEn 取值 → 展示名；未知取值返回空串。
inline const char* px4_fltmode_slot_name(int value) {
    size_t count = 0;
    const Px4FlightModeSlotEntry* table = px4_fltmode_slot_table(count);
    for (size_t i = 0; i < count; ++i) {
        if (table[i].value == value) return table[i].name;
    }
    return "";
}

} // namespace mg::core::firmware::modes
