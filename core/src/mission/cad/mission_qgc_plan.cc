/* QGroundControl .plan JSON interchange. */
#include "core/mission/cad/mission_interchange.h"

#include "core/mission/cad/mission_command_catalog.h"

#include "ardupilotmega/mavlink.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace mg::core::mission::cad {

namespace {
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

std::string compact_json(cJSON* value) {
    if (!value) return {};
    char* printed = cJSON_PrintUnformatted(value);
    if (!printed) return {};
    std::string result(printed);
    cJSON_free(printed);
    return result;
}

bool append_extension_array(cJSON* target,
                            const MissionDocument& document,
                            const char* key,
                            std::string& error) {
    const auto it = document.extensions.find(key);
    if (it == document.extensions.end()) return true;
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> array(
        cJSON_Parse(it->second.c_str()), cJSON_Delete);
    if (!array || !cJSON_IsArray(array.get())) {
        error = std::string("mission document extension ") + key +
                " is not a JSON array";
        return false;
    }
    for (cJSON* item = array->child; item != nullptr; item = item->next) {
        cJSON_AddItemToArray(target, cJSON_Duplicate(item, true));
    }
    return true;
}

void add_extension_object_or_empty(cJSON* root,
                                   const MissionDocument& document,
                                   const char* key,
                                   const char* default_array_key) {
    const auto it = document.extensions.find(key);
    if (it != document.extensions.end()) {
        cJSON* value = cJSON_Parse(it->second.c_str());
        if (value && cJSON_IsObject(value)) {
            cJSON_AddItemToObject(root, key, value);
            return;
        }
        cJSON_Delete(value);
    }
    cJSON* value = cJSON_AddObjectToObject(root, key);
    cJSON_AddNumberToObject(value, "version", 1);
    cJSON_AddArrayToObject(value, default_array_key);
}

void append_opaque_item(MissionDocument& document,
                        const char* key,
                        cJSON* item) {
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> array(nullptr, cJSON_Delete);
    const auto existing = document.extensions.find(key);
    if (existing != document.extensions.end()) {
        array.reset(cJSON_Parse(existing->second.c_str()));
    }
    if (!array || !cJSON_IsArray(array.get())) {
        array.reset(cJSON_CreateArray());
    }
    cJSON_AddItemToArray(array.get(), cJSON_Duplicate(item, true));
    document.extensions[key] = compact_json(array.get());
}
} // namespace

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
    if (!append_extension_array(items, document, "qgc_plan.complex_items", error)) {
        return {};
    }

    add_extension_object_or_empty(root.get(), document, "geoFence", "polygon");
    add_extension_object_or_empty(root.get(), document, "rallyPoints", "points");

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
    parsed.extensions.erase("qgc_plan.complex_items");
    parsed.extensions.erase("geoFence");
    parsed.extensions.erase("rallyPoints");

    cJSON* geo_fence = object_field(root.get(), "geoFence");
    if (has_nonempty_array(root.get(), "geoFence", "polygon")) {
        parsed.extensions["geoFence"] = compact_json(geo_fence);
        add_warning(warnings, "QGC GeoFence preserved as an opaque extension");
    }
    cJSON* rally_points = object_field(root.get(), "rallyPoints");
    if (has_nonempty_array(root.get(), "rallyPoints", "points")) {
        parsed.extensions["rallyPoints"] = compact_json(rally_points);
        add_warning(warnings, "QGC Rally points preserved as an opaque extension");
    }

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
            append_opaque_item(parsed, "qgc_plan.complex_items", item);
            add_warning(
                warnings,
                "QGC item type '" + item_type + "' preserved as an opaque extension");
            continue;
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
    document.extensions = std::move(parsed.extensions);
    return true;
}

} // namespace mg::core::mission::cad
