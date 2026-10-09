/**
 * ardu_copter_modes.h - ArduPilot Copter flight mode table
 * ArduCopter 飞行模式 — custom_mode 数值到名称的映射表
 *
 * Source: ArduPilot/ArduCopter/mode.h (enum class Number)
 * Reference: QGC APMCopterFirmwarePlugin, MP ArduCopter.cs
 *
 * 注意：case 的数值必须是 ArduCopter 的**枚举值**，不是模式列表的下标。
 * 本表曾因为按下标顺序编号而从 8 起整体错位一格（9=LAND 被写成 Drift），
 * 导致降落时界面显示"Drift"。新增模式请照着上游枚举抄数值。
 * Copter 未使用 8、10、12。
 *
 * To add a new mode: add one case below. No other files need to change.
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

inline const char* arducopter_mode_name(uint32_t mode) {
    switch (mode) {
        case 0:  return "Stabilize";
        case 1:  return "Acro";
        case 2:  return "AltHold";
        case 3:  return "Auto";
        case 4:  return "Guided";
        case 5:  return "Loiter";
        case 6:  return "RTL";
        case 7:  return "Circle";
        case 9:  return "Land";
        case 11: return "Drift";
        case 13: return "Sport";
        case 14: return "Flip";
        case 15: return "Autotune";
        case 16: return "PosHold";
        case 17: return "Brake";
        case 18: return "Throw";
        case 19: return "AvoidADSB";
        case 20: return "GuidedNoGPS";
        case 21: return "SmartRTL";
        case 22: return "FlowHold";
        case 23: return "Follow";
        case 24: return "Zigzag";
        case 25: return "SystemID";
        case 26: return "Autorotate";
        case 27: return "AutoRTL";
        case 28: return "Turtle";
        default: return nullptr;
    }
}

} // namespace mg::core::firmware::modes
