/**
 * mission_interchange.cc - `.waypoints`（QGC WPL 110）读写
 *
 * 写出格式对齐 Mission Planner 的 SaveFile_Click：
 *   - 首行 `QGC WPL 110`
 *   - 第 0 行是 home：seq=0、current=1、frame=0、command=16、param 全 0
 *   - 其余行 seq 从 1 开始、current=0、autocontinue=1
 *   - lat/lon 用十进制度，alt 用米
 *
 * 读取格式对齐 Mission Planner 的 ReadWaypoints：允许 `#` 注释行，
 * 分隔符接受制表符 / 空格 / 逗号。
 */
#include "core/mission/cad/mission_interchange.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace mg::core::mission::cad {

namespace {

constexpr int kHomeCommand = 16;      // MAV_CMD_NAV_WAYPOINT

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (char ch : line) {
        if (ch == '\t' || ch == ' ' || ch == ',' || ch == '\r') {
            if (!current.empty()) {
                fields.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        fields.push_back(current);
    }
    return fields;
}

bool parse_double(const std::string& text, double& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') return false;
    if (!std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

bool parse_long(const std::string& text, long& value) {
    if (text.empty()) return false;
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0') return false;
    value = parsed;
    return true;
}

std::string format_fixed(double value, int decimals) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return std::string(buffer);
}

} // namespace

bool is_supported_interchange_format(const std::string& format) {
    return format == "waypoints" || format == "QGC WPL 110";
}

std::string export_waypoints_text(
    const MissionDocument& document,
    const MissionPlan& compiled,
    std::string& error) {
    error.clear();
    std::ostringstream out;
    out << "QGC WPL 110\n";

    // 第 0 行固定是 home。没有设置 home 时按 Mission Planner 的兜底写法补一行，
    // 保持 seq 从 1 开始给任务项，读端才不会把第一个航点当成 home。
    if (document.home.set && std::isfinite(document.home.latitude) &&
        std::isfinite(document.home.longitude)) {
        out << "0\t1\t0\t" << kHomeCommand << "\t0.00000000\t0.00000000\t"
            << "0.00000000\t0.00000000\t"
            << format_fixed(document.home.latitude, 7) << '\t'
            << format_fixed(document.home.longitude, 7) << '\t'
            << format_fixed(document.home.altitude_m, 6) << "\t1\n";
    } else {
        out << "0\t1\t0\t0\t0.00000000\t0.00000000\t0.00000000\t0.00000000"
            << "\t0.0000000\t0.0000000\t0.000000\t1\n";
    }

    size_t sequence = 1;
    for (const MissionItem& item : compiled.items) {
        out << sequence++ << '\t' << static_cast<int>(item.current) << '\t'
            << static_cast<int>(item.frame) << '\t' << item.command << '\t'
            << format_fixed(item.param1, 8) << '\t'
            << format_fixed(item.param2, 8) << '\t'
            << format_fixed(item.param3, 8) << '\t'
            << format_fixed(item.param4, 8) << '\t'
            << format_fixed(static_cast<double>(item.x) / 1e7, 8) << '\t'
            << format_fixed(static_cast<double>(item.y) / 1e7, 8) << '\t'
            << format_fixed(item.z, 6) << '\t'
            << static_cast<int>(item.autocontinue) << '\n';
    }
    return out.str();
}

bool import_waypoints_text(
    const std::string& text,
    MissionDocument& document,
    std::string& error) {
    error.clear();
    std::istringstream input(text);
    std::string line;
    if (!std::getline(input, line)) {
        error = "waypoint file is empty";
        return false;
    }
    if (line.find("QGC WPL") == std::string::npos) {
        error = "waypoint file is missing the QGC WPL header";
        return false;
    }

    MissionDocument parsed;
    parsed.schema_version = document.schema_version;
    parsed.metadata = document.metadata;
    parsed.defaults = document.defaults;
    parsed.revision = document.revision;
    parsed.updated_at_ms = document.updated_at_ms;

    int entity_index = 0;
    bool saw_any_row = false;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto fields = split_fields(line);
        if (fields.size() < 12) {
            // Mission Planner 直接跳过字段不足的行，这里保持一致。
            continue;
        }

        long sequence = 0;
        long current = 0;
        long frame = 0;
        long command = 0;
        if (!parse_long(fields[0], sequence) ||
            !parse_long(fields[1], current) ||
            !parse_long(fields[2], frame) ||
            !parse_long(fields[3], command)) {
            error = "waypoint row has a non-numeric seq/current/frame/command";
            return false;
        }
        if (command < 0 || command > 65535 || frame < 0 || frame > 255) {
            error = "waypoint row command or frame is out of range";
            return false;
        }

        double params[4] = {0, 0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            if (!parse_double(fields[static_cast<size_t>(4 + i)], params[i])) {
                error = "waypoint row has a non-numeric parameter";
                return false;
            }
        }
        double latitude = 0.0;
        double longitude = 0.0;
        double altitude = 0.0;
        long autocontinue = 1;
        if (!parse_double(fields[8], latitude) ||
            !parse_double(fields[9], longitude) ||
            !parse_double(fields[10], altitude)) {
            error = "waypoint row has a non-numeric position";
            return false;
        }
        if (fields.size() > 11) {
            parse_long(fields[11], autocontinue);
        }

        saw_any_row = true;

        // 第 0 行按惯例是 home：只写进 document.home，不生成实体。
        const bool is_home_row =
            sequence == 0 && current == 1 && command == kHomeCommand;
        if (is_home_row) {
            parsed.home.set = true;
            parsed.home.latitude = latitude;
            parsed.home.longitude = longitude;
            parsed.home.altitude_m = static_cast<float>(altitude);
            continue;
        }
        if (sequence == 0 && command == 0) {
            // Mission Planner 的“home 未知”兜底行，忽略。
            continue;
        }

        MissionEntity entity;
        entity.id = "waypoint-" + std::to_string(++entity_index);
        entity.type = mission_entity_type_for_command(static_cast<uint16_t>(command));
        entity.command = static_cast<uint16_t>(command);
        // 文件里可能是 _INT 帧，归一化后文档校验才认。
        entity.frame = normalize_mission_frame(static_cast<uint8_t>(frame));
        entity.autocontinue = static_cast<uint8_t>(autocontinue != 0 ? 1 : 0);
        entity.latitude = latitude;
        entity.longitude = longitude;
        entity.altitude_m = static_cast<float>(altitude);
        for (size_t i = 0; i < 4; ++i) {
            entity.params[i] = static_cast<float>(params[i]);
        }
        entity.order = static_cast<uint32_t>(parsed.entities.size());
        parsed.entities.push_back(std::move(entity));
    }

    if (!saw_any_row) {
        error = "waypoint file contains no rows";
        return false;
    }

    // 归一化 order，保证与文档顺序一致（validate 会对此发 warning）。
    for (size_t i = 0; i < parsed.entities.size(); ++i) {
        parsed.entities[i].order = static_cast<uint32_t>(i);
    }

    document.home = parsed.home;
    document.entities = std::move(parsed.entities);
    return true;
}

} // namespace mg::core::mission::cad
