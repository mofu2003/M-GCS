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

#include "core/mission/cad/mission_command_catalog.h"

#include "ardupilotmega/mavlink.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
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

void add_warning(std::vector<std::string>& warnings, std::string warning) {
    warnings.push_back(std::move(warning));
}

cJSON* object_field(cJSON* object, const char* key) {
    return object ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
}

std::string string_field(cJSON* object, const char* key) {
    cJSON* item = object_field(object, key);
    return item && cJSON_IsString(item) && item->valuestring
        ? std::string(item->valuestring)
        : std::string();
}

bool int_field(cJSON* object, const char* key, int& value) {
    cJSON* item = object_field(object, key);
    if (!item || !cJSON_IsNumber(item)) return false;
    value = static_cast<int>(item->valuedouble);
    return true;
}

bool bool_field(cJSON* object, const char* key, bool& value) {
    cJSON* item = object_field(object, key);
    if (!item) return false;
    if (cJSON_IsBool(item)) {
        value = cJSON_IsTrue(item);
        return true;
    }
    if (cJSON_IsNumber(item)) {
        value = item->valueint != 0;
        return true;
    }
    return false;
}

double number_or_nan(cJSON* item) {
    return item && cJSON_IsNumber(item)
        ? item->valuedouble
        : std::numeric_limits<double>::quiet_NaN();
}

const char* firmware_profile_from_type(int firmware_type) {
    switch (firmware_type) {
        case MAV_AUTOPILOT_ARDUPILOTMEGA: return "ardupilot";
        case MAV_AUTOPILOT_PX4:           return "px4";
        default:                           return "unknown";
    }
}

int firmware_type_from_profile(const std::string& profile) {
    if (profile == "px4") return MAV_AUTOPILOT_PX4;
    return MAV_AUTOPILOT_ARDUPILOTMEGA;
}

const char* vehicle_type_from_mav_type(int vehicle_type) {
    switch (vehicle_type) {
        case MAV_TYPE_FIXED_WING:       return "plane";
        case MAV_TYPE_QUADROTOR:
        case MAV_TYPE_COAXIAL:
        case MAV_TYPE_HELICOPTER:
        case MAV_TYPE_HEXAROTOR:
        case MAV_TYPE_OCTOROTOR:
        case MAV_TYPE_TRICOPTER:        return "copter";
        case MAV_TYPE_VTOL_TAILSITTER_DUOROTOR:
        case MAV_TYPE_VTOL_TAILSITTER_QUADROTOR:
        case MAV_TYPE_VTOL_TILTROTOR:
        case MAV_TYPE_VTOL_FIXEDROTOR:
        case MAV_TYPE_VTOL_TAILSITTER:
        case MAV_TYPE_VTOL_TILTWING:
        case MAV_TYPE_VTOL_RESERVED5:   return "vtol";
        default:                         return "unknown";
    }
}

int mav_type_from_vehicle_type(const std::string& vehicle_type) {
    if (vehicle_type == "plane" || vehicle_type == "fixedwing") {
        return MAV_TYPE_FIXED_WING;
    }
    if (vehicle_type == "vtol") return MAV_TYPE_VTOL_TAILSITTER_QUADROTOR;
    return MAV_TYPE_QUADROTOR;
}

bool catalog_has_command(uint16_t command) {
    const auto& catalog = mission_command_catalog();
    return std::any_of(catalog.begin(), catalog.end(), [command](const auto& item) {
        return item.command == command;
    });
}

bool has_nonempty_array(cJSON* root, const char* section_key, const char* array_key) {
    cJSON* section = object_field(root, section_key);
    if (!section || !cJSON_IsObject(section)) return false;
    cJSON* array = object_field(section, array_key);
    return array && cJSON_IsArray(array) && cJSON_GetArraySize(array) > 0;
}

void add_number_or_null(cJSON* array, double value) {
    cJSON_AddItemToArray(
        array,
        std::isfinite(value) ? cJSON_CreateNumber(value) : cJSON_CreateNull());
}

} // namespace

bool is_supported_interchange_format(const std::string& format) {
    return format == "waypoints" || format == "QGC WPL 110" ||
           format == "plan" || format == "qgc_plan" || format == "qgc.plan";
}

std::string recommended_mission_interchange_format(
    const MissionDocument& document) {
    const bool has_complex_entity = std::any_of(
        document.entities.begin(),
        document.entities.end(),
        [](const MissionEntity& entity) {
            return is_parameterized_survey_type(entity.type);
        });
    if (document.metadata.firmware_profile == "px4" || has_complex_entity) {
        return "plan";
    }
    return "waypoints";
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
        std::isfinite(document.home.longitude) &&
        std::isfinite(document.home.altitude_m)) {
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

std::string export_qgc_plan_json(
    const MissionDocument& document,
    std::vector<std::string>& warnings,
    std::string& error) {
    error.clear();
    warnings.clear();

    for (const MissionEntity& entity : document.entities) {
        if (is_parameterized_survey_type(entity.type)) {
            error = "QGC .plan export cannot represent parameterized survey entity: " +
                    entity.id;
            return {};
        }
        if (!entity.enabled) {
            error = "QGC .plan export cannot represent disabled mission entity: " +
                    entity.id;
            return {};
        }
        if (!entity.label.empty() || !entity.notes.empty()) {
            add_warning(
                warnings,
                "entity " + entity.id + " label/notes are not preserved by QGC .plan");
        }
    }

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_CreateObject(), cJSON_Delete);
    if (!root) {
        error = "failed to allocate QGC .plan JSON";
        return {};
    }

    cJSON_AddStringToObject(root.get(), "fileType", "Plan");
    cJSON_AddStringToObject(root.get(), "groundStation", "M-GCS");
    cJSON_AddNumberToObject(root.get(), "version", 1);

    cJSON* mission = cJSON_AddObjectToObject(root.get(), "mission");
    cJSON_AddNumberToObject(mission, "version", 2);

    if (document.metadata.firmware_profile != "ardupilot" &&
        document.metadata.firmware_profile != "px4") {
        add_warning(
            warnings,
            "unknown firmware profile '" + document.metadata.firmware_profile +
                "', exporting as ArduPilot");
    }
    cJSON_AddNumberToObject(
        mission,
        "firmwareType",
        firmware_type_from_profile(document.metadata.firmware_profile));

    if (document.metadata.vehicle_type != "copter" &&
        document.metadata.vehicle_type != "plane" &&
        document.metadata.vehicle_type != "fixedwing" &&
        document.metadata.vehicle_type != "vtol") {
        add_warning(
            warnings,
            "unknown vehicle type '" + document.metadata.vehicle_type +
                "', exporting as quadrotor");
    }
    cJSON_AddNumberToObject(
        mission,
        "vehicleType",
        mav_type_from_vehicle_type(document.metadata.vehicle_type));
    cJSON_AddNumberToObject(mission, "cruiseSpeed", document.defaults.speed_ms);
    cJSON_AddNumberToObject(mission, "hoverSpeed", document.defaults.speed_ms);

    if (document.home.set && std::isfinite(document.home.latitude) &&
        std::isfinite(document.home.longitude)) {
        cJSON* home = cJSON_AddArrayToObject(mission, "plannedHomePosition");
        cJSON_AddItemToArray(home, cJSON_CreateNumber(document.home.latitude));
        cJSON_AddItemToArray(home, cJSON_CreateNumber(document.home.longitude));
        cJSON_AddItemToArray(home, cJSON_CreateNumber(document.home.altitude_m));
    } else {
        cJSON_AddItemToObject(mission, "plannedHomePosition", cJSON_CreateNull());
        add_warning(warnings, "mission home is not set; exported as null");
    }

    cJSON* items = cJSON_AddArrayToObject(mission, "items");
    int sequence = 1;
    for (const MissionEntity& entity : document.entities) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "type", "SimpleItem");
        cJSON_AddNumberToObject(item, "frame", entity.frame);
        cJSON_AddNumberToObject(item, "command", entity.command);
        cJSON_AddBoolToObject(item, "autoContinue", entity.autocontinue != 0);
        cJSON_AddNumberToObject(item, "doJumpId", sequence++);

        cJSON* params = cJSON_AddArrayToObject(item, "params");
        for (float value : entity.params) {
            cJSON_AddItemToArray(params, cJSON_CreateNumber(value));
        }
        add_number_or_null(params, entity.latitude);
        add_number_or_null(params, entity.longitude);
        add_number_or_null(params, entity.altitude_m);
        cJSON_AddItemToArray(items, item);
    }

    cJSON* geo_fence = cJSON_AddObjectToObject(root.get(), "geoFence");
    cJSON_AddNumberToObject(geo_fence, "version", 1);
    cJSON_AddArrayToObject(geo_fence, "polygon");

    cJSON* rally = cJSON_AddObjectToObject(root.get(), "rallyPoints");
    cJSON_AddNumberToObject(rally, "version", 1);
    cJSON_AddArrayToObject(rally, "points");

    char* printed = cJSON_Print(root.get());
    if (!printed) {
        error = "failed to serialize QGC .plan JSON";
        return {};
    }
    std::string text(printed);
    cJSON_free(printed);
    return text;
}

bool import_qgc_plan_json(
    const std::string& text,
    MissionDocument& document,
    std::vector<std::string>& warnings,
    std::string& error) {
    error.clear();
    warnings.clear();

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_Parse(text.c_str()), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) {
        error = "QGC .plan is not a JSON object";
        return false;
    }

    const std::string file_type = string_field(root.get(), "fileType");
    if (!file_type.empty() && file_type != "Plan") {
        error = "QGC .plan fileType must be Plan";
        return false;
    }
    if (has_nonempty_array(root.get(), "geoFence", "polygon")) {
        error = "QGC .plan contains a non-empty GeoFence, which is not supported by the mission document";
        return false;
    }
    if (has_nonempty_array(root.get(), "rallyPoints", "points")) {
        error = "QGC .plan contains non-empty Rally points, which are not supported by the mission document";
        return false;
    }

    cJSON* mission = object_field(root.get(), "mission");
    if (!mission || !cJSON_IsObject(mission)) {
        error = "QGC .plan is missing the mission object";
        return false;
    }

    int mission_version = 0;
    if (int_field(mission, "version", mission_version) && mission_version != 2) {
        add_warning(
            warnings,
            "unsupported QGC mission version " + std::to_string(mission_version) +
                ", attempting to import as version 2");
    }

    MissionDocument parsed = document;
    parsed.entities.clear();

    int firmware_type = 0;
    if (int_field(mission, "firmwareType", firmware_type)) {
        parsed.metadata.firmware_profile = firmware_profile_from_type(firmware_type);
        if (parsed.metadata.firmware_profile == "unknown") {
            add_warning(
                warnings,
                "unknown QGC firmwareType " + std::to_string(firmware_type));
        }
    } else {
        error = "QGC .plan mission.firmwareType is required";
        return false;
    }

    int vehicle_type = 0;
    if (int_field(mission, "vehicleType", vehicle_type)) {
        parsed.metadata.vehicle_type = vehicle_type_from_mav_type(vehicle_type);
        if (parsed.metadata.vehicle_type == "unknown") {
            add_warning(
                warnings,
                "unknown QGC vehicleType " + std::to_string(vehicle_type));
        }
    } else {
        add_warning(warnings, "QGC .plan mission.vehicleType is missing");
    }

    double cruise_speed = 0.0;
    cJSON* cruise_speed_item = object_field(mission, "cruiseSpeed");
    if (cruise_speed_item && cJSON_IsNumber(cruise_speed_item)) {
        cruise_speed = cruise_speed_item->valuedouble;
        if (std::isfinite(cruise_speed) && cruise_speed > 0.0) {
            parsed.defaults.speed_ms = static_cast<float>(cruise_speed);
        } else {
            add_warning(warnings, "QGC cruiseSpeed is not a positive finite value");
        }
    }
    if (object_field(mission, "hoverSpeed")) {
        add_warning(warnings, "QGC hoverSpeed is not preserved by the mission document");
    }
    if (object_field(mission, "globalPlanAltitudeMode")) {
        add_warning(
            warnings,
            "QGC globalPlanAltitudeMode is not preserved; per-item frame is authoritative");
    }

    cJSON* home_json = object_field(mission, "plannedHomePosition");
    parsed.home = MissionHome{};
    if (home_json && cJSON_IsArray(home_json) && cJSON_GetArraySize(home_json) >= 3) {
        const double latitude = number_or_nan(cJSON_GetArrayItem(home_json, 0));
        const double longitude = number_or_nan(cJSON_GetArrayItem(home_json, 1));
        const double altitude = number_or_nan(cJSON_GetArrayItem(home_json, 2));
        if (std::isfinite(latitude) && std::isfinite(longitude) &&
            std::isfinite(altitude)) {
            parsed.home.set = true;
            parsed.home.latitude = latitude;
            parsed.home.longitude = longitude;
            parsed.home.altitude_m = static_cast<float>(altitude);
        } else {
            add_warning(warnings, "QGC plannedHomePosition contains invalid values");
        }
    } else if (home_json && cJSON_IsNull(home_json)) {
        add_warning(warnings, "QGC plannedHomePosition is null");
    } else {
        add_warning(warnings, "QGC .plan is missing plannedHomePosition");
    }

    cJSON* items = object_field(mission, "items");
    if (!items || !cJSON_IsArray(items)) {
        error = "QGC .plan mission.items must be an array";
        return false;
    }

    const int item_count = cJSON_GetArraySize(items);
    for (int index = 0; index < item_count; ++index) {
        cJSON* item = cJSON_GetArrayItem(items, index);
        if (!item || !cJSON_IsObject(item)) {
            error = "QGC .plan mission item " + std::to_string(index) +
                    " is not an object";
            return false;
        }

        const std::string item_type = string_field(item, "type");
        if (item_type != "SimpleItem") {
            error = "unsupported QGC .plan item type '" + item_type +
                    "' at index " + std::to_string(index);
            return false;
        }

        int frame = 0;
        int command = 0;
        bool auto_continue = false;
        if (!int_field(item, "frame", frame) ||
            !int_field(item, "command", command) ||
            !bool_field(item, "autoContinue", auto_continue)) {
            error = "QGC SimpleItem at index " + std::to_string(index) +
                    " is missing frame, command or autoContinue";
            return false;
        }
        if (frame < 0 || frame > 255 || command < 0 || command > 65535) {
            error = "QGC SimpleItem at index " + std::to_string(index) +
                    " has frame or command out of range";
            return false;
        }

        cJSON* params = object_field(item, "params");
        if (!params || !cJSON_IsArray(params) || cJSON_GetArraySize(params) < 4) {
            error = "QGC SimpleItem at index " + std::to_string(index) +
                    " must contain at least four params";
            return false;
        }

        MissionEntity entity;
        entity.id = "plan-item-" + std::to_string(index + 1);
        entity.type = mission_entity_type_for_command(static_cast<uint16_t>(command));
        entity.command = static_cast<uint16_t>(command);
        entity.frame = normalize_mission_frame(static_cast<uint8_t>(frame));
        entity.autocontinue = static_cast<uint8_t>(auto_continue ? 1 : 0);
        entity.order = static_cast<uint32_t>(index);
        for (int param = 0; param < 4; ++param) {
            cJSON* value = cJSON_GetArrayItem(params, param);
            if (value && cJSON_IsNumber(value)) {
                entity.params[static_cast<size_t>(param)] =
                    static_cast<float>(value->valuedouble);
            } else {
                entity.params[static_cast<size_t>(param)] = 0.0f;
                if (value && cJSON_IsNull(value)) {
                    add_warning(
                        warnings,
                        "QGC item " + std::to_string(index) + " param" +
                            std::to_string(param + 1) + " is null; imported as 0");
                }
            }
        }

        const int params_count = cJSON_GetArraySize(params);
        if (params_count >= 7) {
            entity.latitude = number_or_nan(cJSON_GetArrayItem(params, 4));
            entity.longitude = number_or_nan(cJSON_GetArrayItem(params, 5));
            entity.altitude_m = static_cast<float>(
                number_or_nan(cJSON_GetArrayItem(params, 6)));
        }

        cJSON* coordinate = object_field(item, "coordinate");
        if (coordinate && cJSON_IsArray(coordinate) && cJSON_GetArraySize(coordinate) >= 3) {
            entity.latitude = number_or_nan(cJSON_GetArrayItem(coordinate, 0));
            entity.longitude = number_or_nan(cJSON_GetArrayItem(coordinate, 1));
            entity.altitude_m = static_cast<float>(
                number_or_nan(cJSON_GetArrayItem(coordinate, 2)));
        }

        if (object_field(item, "altitudeMode") ||
            object_field(item, "altitude") ||
            object_field(item, "AMSLAltAboveTerrain")) {
            add_warning(
                warnings,
                "QGC item " + std::to_string(index) +
                    " altitudeMode/terrain fields are not preserved");
        }
        if (!catalog_has_command(static_cast<uint16_t>(command))) {
            add_warning(
                warnings,
                "QGC item " + std::to_string(index) +
                    " uses command " + std::to_string(command) +
                    ", which is not in the supported command catalog");
        }
        parsed.entities.push_back(std::move(entity));
    }

    document.home = parsed.home;
    document.metadata = parsed.metadata;
    document.defaults = parsed.defaults;
    document.entities = std::move(parsed.entities);
    return true;
}

} // namespace mg::core::mission::cad
