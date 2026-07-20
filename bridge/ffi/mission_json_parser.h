/**
 * mission_json_parser.h - Shared mission item JSON parser
 *
 * 任务航点 JSON 解析器，自动检测 MAVLink 原始格式和人类可读格式。
 *
 * Supports two JSON formats:
 *   1. MAVLink raw: { "x": degE7, "y": degE7, "z": meters }
 *   2. Human-readable: { "lat": degrees, "lon": degrees, "alt": meters }
 *
 * Format is auto-detected: if "lat" key exists, use human-readable;
 * otherwise use raw x/y/z.
 *
 * Used by both ffi_commands.cc and ffi_mission_management.cc.
 */
#pragma once

#include "core/mission/mission_types.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace mg::bridge::mission_parser {

inline bool parse_mission_item(cJSON* item_json, mg::core::mission::MissionItem& item) {
    if (!item_json) return false;

    auto get_int = [&](const char* key) -> int {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(item_json, key);
        if (f && cJSON_IsNumber(f)) return f->valueint;
        return 0;
    };
    auto get_float = [&](const char* key) -> float {
        cJSON* f = cJSON_GetObjectItemCaseSensitive(item_json, key);
        if (f && cJSON_IsNumber(f)) return static_cast<float>(f->valuedouble);
        return 0.0f;
    };
    auto has_key = [&](const char* key) -> bool {
        return cJSON_GetObjectItemCaseSensitive(item_json, key) != nullptr;
    };

    // Common fields
    item.seq          = static_cast<uint16_t>(get_int("seq"));
    item.frame        = static_cast<uint8_t>(get_int("frame"));
    item.command      = static_cast<uint16_t>(get_int("command"));
    item.autocontinue = static_cast<uint8_t>(get_int("autocontinue"));
    item.current      = static_cast<uint8_t>(get_int("current"));
    item.param1       = get_float("param1");
    item.param2       = get_float("param2");
    item.param3       = get_float("param3");
    item.param4       = get_float("param4");

    // Auto-detect format: "lat" key = human-readable, otherwise raw x/y/z
    if (has_key("lat") || has_key("lon")) {
        // Human-readable format: lat/lon in degrees, alt in meters
        item.x = static_cast<int32_t>(get_float("lat") * 1e7);
        item.y = static_cast<int32_t>(get_float("lon") * 1e7);
        item.z = get_float("alt");
        // "hold_time" alias for param1 (human-friendly)
        if (has_key("hold_time")) {
            item.param1 = get_float("hold_time");
        }
    } else {
        // Raw MAVLink format: x = degE7, y = degE7, z = meters
        item.x = static_cast<int32_t>(get_int("x"));
        item.y = static_cast<int32_t>(get_int("y"));
        item.z = get_float("z");
    }

    return true;
}

} // namespace mg::bridge::mission_parser
