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
#include <vector>

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

/// 把文档导出为 QGroundControl `.plan` JSON。
///
/// 普通实体写成 `SimpleItem`；`survey_grid` / `corridor` 参数化测绘实体
/// 会写成 QGC 的 `ComplexItem`（Survey / CorridorScan），并从 `SurveyCalculator`
/// 展开的任务项一并嵌入 `TransectStyleComplexItem.Items`，因此 QGC 打开时仍是
/// 可编辑的测绘区域且航线与 M-GCS 一致。若某个测绘实体无法构造 ComplexItem，
/// 会退化成展开后的简单任务项并写入 warning，不会报错丢任务。禁用实体、
/// 以及无法映射的字段仍按原有规则返回错误或写入 `warnings`。
std::string export_qgc_plan_json(
    const MissionDocument& document,
    std::vector<std::string>& warnings,
    std::string& error);

/// 解析 QGroundControl `.plan` JSON 并替换 `document` 的 home / defaults /
/// metadata 固件信息 / entities。`SimpleItem` 兼容旧版 `coordinate` 数组和
/// 当前 `params` 7 值格式；其它类型（QGC ComplexItem）原样保存在
/// `MissionDocument::complex_items`，可通过 `expand_complex_item_entities`
/// 取回其 `TransectStyleComplexItem.Items`。可继续导入但可能丢失的字段写入
/// `warnings`。
bool import_qgc_plan_json(
    const std::string& text,
    MissionDocument& document,
    std::vector<std::string>& warnings,
    std::string& error);

/// 该格式名是否被 `export` / `import` 支持。
bool is_supported_interchange_format(const std::string& format);

/// 根据 firmware profile 和任务复杂度推荐外部交换格式。
///
/// 规则见 ADR 0008：
/// - ArduPilot + 普通任务：waypoints
/// - PX4：plan
/// - 参数化 Survey / Corridor：plan，避免展开后丢失设计意图
/// - 未知固件：waypoints
std::string recommended_mission_interchange_format(
    const MissionDocument& document);

} // namespace mg::core::mission::cad
