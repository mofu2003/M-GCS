/**
 * mission_interchange.h - 任务文件互操作
 *
 * 目前支持 Mission Planner / QGroundControl 通用的 `.waypoints`
 * （`QGC WPL 110`）文本格式：
 *
 *   QGC WPL 110
 *   seq  current  frame  command  p1  p2  p3  p4  lat  lon  alt  autocontinue
 *
 * 第 0 行按惯例是 home（current=1、frame=0、command=16）。字段含义与顺序
 * 以 Mission Planner 的写出实现为准，不凭记忆臆造。
 */
#pragma once

#include "core/mission/cad/mission_document.h"

#include <string>

namespace mg::core::mission::cad {

/// 把文档（连同 home）导出为 `.waypoints` 文本。
/// `compiled` 必须是同一份文档编译后的计划；导出只做序列化，不重新编译。
std::string export_waypoints_text(
    const MissionDocument& document,
    const MissionPlan& compiled,
    std::string& error);

/// 解析 `.waypoints` 文本并替换 `document` 的 home 与 entities。
/// 解析失败时 `document` 不被修改。
bool import_waypoints_text(
    const std::string& text,
    MissionDocument& document,
    std::string& error);

/// 该格式名是否被 `export` / `import` 支持。
bool is_supported_interchange_format(const std::string& format);

} // namespace mg::core::mission::cad
