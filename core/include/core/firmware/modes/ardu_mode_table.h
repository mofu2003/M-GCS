/**
 * ardu_mode_table.h - ArduPilot 模式表条目类型（各机型表共用）
 *
 * 每个机型头文件提供 _mode_table(count) 返回一张 {custom_mode, name, can_be_set}
 * 表，以及由它派生的 _mode_name(mode)。这样「数值→名称」和「名称→数值 / 可选列表」
 * 都来自同一份数据，不会像历史上那样在插件里再抄一张表而整体漂移。
 *
 * Source of truth: ArduPilot/{ArduCopter,ArduPlane,Rover,ArduSub,AntennaTracker}/mode.h
 */

#pragma once
#include <cstdint>

namespace mg::core::firmware::modes {

/// 单个 ArduPilot 飞行模式条目。
struct ArduModeEntry {
    uint32_t    custom_mode;   // HEARTBEAT.custom_mode 的取值（上游 enum 值，不是下标）
    const char* name;          // 展示名
    bool        can_be_set;    // 用户能否直接切到这个模式
};

} // namespace mg::core::firmware::modes
