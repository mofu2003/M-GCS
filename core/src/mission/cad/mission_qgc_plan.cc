/* QGroundControl .plan JSON interchange. */
#include "core/mission/cad/mission_interchange.h"

#include "core/mission/cad/mission_command_catalog.h"
#include "mission_document_internal.h"

#include "ardupilotmega/mavlink.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include "core/util/json.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

std::string compact_json(cJSON* value);

// Keys M-GCS models natively. Everything else on the JSON object is captured
// verbatim so an import -> edit -> export round-trip does not drop user data.
const char* const kSimpleItemKnownKeys[] = {
    "type",       "frame", "command", "autoContinue",
    "params",     "coordinate", "doJumpId",
};
const char* const kMissionKnownKeys[] = {
    "version", "firmwareType", "vehicleType", "cruiseSpeed",
    "plannedHomePosition", "items",
};
const char* const kPlanRootKnownKeys[] = {
    "fileType", "groundStation", "version", "mission", "geoFence", "rallyPoints",
};

template <size_t N>
bool known_key(const char* key, const char* const (&keys)[N]) {
    if (!key) return false;
    for (const char* candidate : keys) {
        if (std::strcmp(key, candidate) == 0) return true;
    }
    return false;
}

// Build a JSON object holding every field of `source` whose key is not in
// `keys`. Caller owns the result (or nullptr when there is nothing to keep).
template <size_t N>
cJSON* collect_extra_fields(cJSON* source, const char* const (&keys)[N]) {
    if (!source) return nullptr;
    cJSON* extras = nullptr;
    for (cJSON* child = source->child; child != nullptr; child = child->next) {
        if (!child->string || known_key(child->string, keys)) continue;
        if (!extras) extras = cJSON_CreateObject();
        if (!extras) return nullptr;
        cJSON_AddItemToObject(extras, child->string, cJSON_Duplicate(child, true));
    }
    return extras;
}

// Preserve "unknown" source fields on a document entity as a compact JSON
// object string. `entity` is untouched when there is nothing extra to keep.
template <size_t N>
void attach_item_extensions(cJSON* item, const char* const (&keys)[N], MissionEntity& entity) {
    cJSON* extras = collect_extra_fields(item, keys);
    if (!extras) return;
    if (extras->child) entity.extensions_json = compact_json(extras);
    cJSON_Delete(extras);
}

// Copy every field of `source` onto `target`, replacing a same-named key. Used
// on export to re-emit the fields captured by `collect_extra_fields`.
void merge_object_fields(cJSON* target, cJSON* source) {
    if (!target || !source) return;
    for (cJSON* child = source->child; child != nullptr; child = child->next) {
        if (!child->string) continue;
        cJSON_DeleteItemFromObjectCaseSensitive(target, child->string);
        cJSON_AddItemToObject(target, child->string, cJSON_Duplicate(child, true));
    }
}

// Parse a preserved verbatim JSON object out of the document extension store.
// Returns nullptr when the key is absent or not a JSON object.
cJSON* extension_object(
    const std::map<std::string, std::string>& extensions,
    const char* key) {
    const auto it = extensions.find(key);
    if (it == extensions.end() || it->second.empty()) return nullptr;
    cJSON* value = mg::core::json::parse(it->second.c_str());
    if (!value || !cJSON_IsObject(value)) {
        cJSON_Delete(value);
        return nullptr;
    }
    return value;
}

const char* firmware_profile_from_type(int firmware_type) {
    switch (firmware_type) {
        case MAV_AUTOPILOT_ARDUPILOTMEGA: return "ardupilot";
        default:                           return "unknown";
    }
}

int firmware_type_from_profile(const std::string&) {
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

void add_number_or_null(cJSON* array, double value) {
    cJSON_AddItemToArray(
        array,
        std::isfinite(value) ? cJSON_CreateNumber(value) : cJSON_CreateNull());
}

bool parse_qgc_simple_item(cJSON* item,
                           const std::string& id,
                           uint32_t order,
                           MissionEntity& entity,
                           std::string& error) {
    int frame = 0;
    int command = 0;
    bool auto_continue = false;
    if (!int_field(item, "frame", frame) ||
        !int_field(item, "command", command) ||
        !bool_field(item, "autoContinue", auto_continue)) {
        error = "mission item " + id + " is missing frame, command or autoContinue";
        return false;
    }
    if (frame < 0 || frame > 255 || command < 0 || command > 65535) {
        error = "mission item " + id + " has frame or command out of range";
        return false;
    }
    cJSON* params = object_field(item, "params");
    if (!params || !cJSON_IsArray(params) || cJSON_GetArraySize(params) < 4) {
        error = "mission item " + id + " must contain at least four params";
        return false;
    }

    entity = MissionEntity{};
    entity.id = id;
    entity.command = static_cast<uint16_t>(command);
    entity.type = mission_entity_type_for_command(entity.command);
    entity.frame = normalize_mission_frame(static_cast<uint8_t>(frame));
    entity.autocontinue = static_cast<uint8_t>(auto_continue ? 1 : 0);
    entity.order = order;
    for (int param = 0; param < 4; ++param) {
        cJSON* value = cJSON_GetArrayItem(params, param);
        entity.params[static_cast<size_t>(param)] =
            (value && cJSON_IsNumber(value))
                ? static_cast<float>(value->valuedouble)
                : 0.0f;
    }
    const int params_count = cJSON_GetArraySize(params);
    if (params_count >= 7) {
        // 这条路径只被 `expand_complex_item_entities()` 用来展开 complex item
        // 的几何（全是航点）；文档级导入见 `import_qgc_plan_json()`。
        if (is_global_coordinate_frame(entity.frame) &&
            mission_command_uses_location(entity.command)) {
            entity.latitude = number_or_nan(cJSON_GetArrayItem(params, 4));
            entity.longitude = number_or_nan(cJSON_GetArrayItem(params, 5));
        }
        entity.altitude_m =
            static_cast<float>(number_or_nan(cJSON_GetArrayItem(params, 6)));
    }
    cJSON* coordinate = object_field(item, "coordinate");
    if (coordinate && cJSON_IsArray(coordinate) &&
        cJSON_GetArraySize(coordinate) >= 3) {
        if (is_global_coordinate_frame(entity.frame) &&
            mission_command_uses_location(entity.command)) {
            entity.latitude = number_or_nan(cJSON_GetArrayItem(coordinate, 0));
            entity.longitude = number_or_nan(cJSON_GetArrayItem(coordinate, 1));
        }
        entity.altitude_m =
            static_cast<float>(number_or_nan(cJSON_GetArrayItem(coordinate, 2)));
    }
    attach_item_extensions(item, kSimpleItemKnownKeys, entity);
    return true;
}

std::string compact_json(cJSON* value) {
    if (!value) return {};
    char* printed = cJSON_PrintUnformatted(value);
    if (!printed) return {};
    std::string result(printed);
    cJSON_free(printed);
    return result;
}

bool append_complex_item_json(cJSON* target,
                              const MissionComplexItem& complex,
                              int do_jump_id,
                              std::string& error) {
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> item(
        mg::core::json::parse(complex.json.c_str()), cJSON_Delete);
    if (!item || !cJSON_IsObject(item.get())) {
        error = "mission document complex item '" + complex.id +
                "' is not a JSON object";
        return false;
    }
    // doJumpId is a derived field: renumber it so the merged mission.items
    // sequence stays unique across simple and complex items.
    cJSON_DeleteItemFromObject(item.get(), "doJumpId");
    cJSON_AddNumberToObject(item.get(), "doJumpId", do_jump_id);
    cJSON_AddItemToArray(target, cJSON_Duplicate(item.get(), true));
    return true;
}

void append_complex_item(MissionDocument& document, cJSON* item, int index) {
    MissionComplexItem complex;
    complex.id = "plan-complex-item-" + std::to_string(index + 1);
    complex.item_type = string_field(item, "complexItemType");
    if (complex.item_type.empty()) {
        complex.item_type = string_field(item, "type");
    }
    complex.json = compact_json(item);
    complex.entity_insert_index =
        static_cast<uint32_t>(document.entities.size());
    document.complex_items.push_back(std::move(complex));
}

// --- QGC ComplexItem export ------------------------------------------------
//
// M-GCS stores parameterized survey/corridor tasks as design intent
// (center, size, angle, camera). QGC stores them as a ComplexItem that also
// carries the expanded MissionItem list. We rebuild the QGC geometry from our
// design intent and embed the compiled items so the plan opens as an editable
// Survey / Corridor Scan instead of a flat list of waypoints.

void add_rally_points(cJSON* root,
                      const MissionDocument& document,
                      const char* key) {
    cJSON* rally = cJSON_AddObjectToObject(root, key);
    cJSON_AddNumberToObject(rally, "version", 2);
    cJSON* points = cJSON_AddArrayToObject(rally, "points");
    for (const MissionRallyPoint& point : document.rally_points) {
        cJSON* coordinate = cJSON_CreateArray();
        add_number_or_null(coordinate, point.latitude);
        add_number_or_null(coordinate, point.longitude);
        add_number_or_null(coordinate, point.altitude_m);
        cJSON_AddItemToArray(points, coordinate);
    }
}

bool parse_rally_points(cJSON* root,
                        MissionDocument& document,
                        std::vector<std::string>& warnings,
                        std::string& error) {
    cJSON* rally = object_field(root, "rallyPoints");
    if (!rally) return true;
    if (!cJSON_IsObject(rally)) {
        error = "QGC rallyPoints must be an object";
        return false;
    }
    int version = 0;
    if (!int_field(rally, "version", version)) {
        error = "QGC rallyPoints.version is required";
        return false;
    }
    if (version == 1) {
        add_warning(warnings, "old QGC rallyPoints version 1 is ignored");
        return true;
    }
    if (version != 2) {
        error = "QGC rallyPoints supports version 2";
        return false;
    }
    cJSON* points = object_field(rally, "points");
    if (!points || !cJSON_IsArray(points)) {
        error = "QGC rallyPoints.points must be an array";
        return false;
    }
    const int count = cJSON_GetArraySize(points);
    document.rally_points.reserve(static_cast<size_t>(count));
    for (int index = 0; index < count; ++index) {
        cJSON* coordinate = cJSON_GetArrayItem(points, index);
        if (!coordinate || !cJSON_IsArray(coordinate) ||
            cJSON_GetArraySize(coordinate) != 3) {
            error = "QGC rally point " + std::to_string(index) +
                    " must be a three-value coordinate";
            return false;
        }
        MissionRallyPoint point;
        point.latitude = number_or_nan(cJSON_GetArrayItem(coordinate, 0));
        point.longitude = number_or_nan(cJSON_GetArrayItem(coordinate, 1));
        point.altitude_m = static_cast<float>(
            number_or_nan(cJSON_GetArrayItem(coordinate, 2)));
        document.rally_points.push_back(point);
    }
    return true;
}

void add_geo_fence(cJSON* root,
                   const MissionDocument& document,
                   const char* key) {
    cJSON* fence = cJSON_AddObjectToObject(root, key);
    cJSON_AddNumberToObject(fence, "version", 2);
    cJSON* polygons = cJSON_AddArrayToObject(fence, "polygons");
    for (const auto& polygon : document.geo_fence.polygons) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "version", 1);
        cJSON_AddBoolToObject(item, "inclusion", polygon.inclusion);
        cJSON* vertices = cJSON_AddArrayToObject(item, "polygon");
        for (const auto& vertex : polygon.vertices) {
            cJSON* coordinate = cJSON_CreateArray();
            add_number_or_null(coordinate, vertex.latitude);
            add_number_or_null(coordinate, vertex.longitude);
            cJSON_AddItemToArray(vertices, coordinate);
        }
        cJSON_AddItemToArray(polygons, item);
    }
    cJSON* circles = cJSON_AddArrayToObject(fence, "circles");
    for (const auto& circle : document.geo_fence.circles) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "version", 1);
        cJSON_AddBoolToObject(item, "inclusion", circle.inclusion);
        cJSON* circle_object = cJSON_AddObjectToObject(item, "circle");
        cJSON* center = cJSON_AddArrayToObject(circle_object, "center");
        add_number_or_null(center, circle.center.latitude);
        add_number_or_null(center, circle.center.longitude);
        cJSON_AddNumberToObject(circle_object, "radius", circle.radius_m);
        cJSON_AddItemToArray(circles, item);
    }
    if (document.geo_fence.breach_return_set) {
        cJSON* breach = cJSON_AddArrayToObject(fence, "breachReturn");
        add_number_or_null(breach, document.geo_fence.breach_return.latitude);
        add_number_or_null(breach, document.geo_fence.breach_return.longitude);
        add_number_or_null(breach, document.geo_fence.breach_return_altitude_m);
    }
}

bool parse_geo_fence(cJSON* root,
                     MissionDocument& document,
                     std::vector<std::string>& warnings,
                     std::string& error) {
    document.geo_fence = MissionGeoFence{};
    cJSON* fence = object_field(root, "geoFence");
    if (!fence) return true;
    if (!cJSON_IsObject(fence)) {
        error = "QGC geoFence must be an object";
        return false;
    }
    int version = 0;
    if (!int_field(fence, "version", version)) {
        error = "QGC geoFence.version is required";
        return false;
    }
    if (version == 1) {
        add_warning(warnings, "old QGC geoFence version 1 is ignored");
        return true;
    }
    if (version != 2) {
        error = "QGC geoFence supports version 2";
        return false;
    }

    cJSON* polygons = object_field(fence, "polygons");
    if (!polygons || !cJSON_IsArray(polygons)) {
        error = "QGC geoFence.polygons must be an array";
        return false;
    }
    for (int i = 0; i < cJSON_GetArraySize(polygons); ++i) {
        cJSON* item = cJSON_GetArrayItem(polygons, i);
        if (!item || !cJSON_IsObject(item)) {
            error = "QGC geoFence polygon must be an object";
            return false;
        }
        MissionGeoFencePolygon polygon;
        bool inclusion = true;
        if (!bool_field(item, "inclusion", inclusion)) {
            error = "QGC geoFence polygon inclusion is required";
            return false;
        }
        polygon.inclusion = inclusion;
        cJSON* vertices = object_field(item, "polygon");
        if (!vertices || !cJSON_IsArray(vertices)) {
            error = "QGC geoFence polygon vertices must be an array";
            return false;
        }
        for (int j = 0; j < cJSON_GetArraySize(vertices); ++j) {
            cJSON* coordinate = cJSON_GetArrayItem(vertices, j);
            if (!coordinate || !cJSON_IsArray(coordinate) || cJSON_GetArraySize(coordinate) != 2) {
                error = "QGC geoFence polygon vertex must contain two values";
                return false;
            }
            MissionGeoPoint point;
            point.latitude = number_or_nan(cJSON_GetArrayItem(coordinate, 0));
            point.longitude = number_or_nan(cJSON_GetArrayItem(coordinate, 1));
            polygon.vertices.push_back(point);
        }
        document.geo_fence.polygons.push_back(std::move(polygon));
    }

    cJSON* circles = object_field(fence, "circles");
    if (!circles || !cJSON_IsArray(circles)) {
        error = "QGC geoFence.circles must be an array";
        return false;
    }
    for (int i = 0; i < cJSON_GetArraySize(circles); ++i) {
        cJSON* item = cJSON_GetArrayItem(circles, i);
        if (!item || !cJSON_IsObject(item)) {
            error = "QGC geoFence circle must be an object";
            return false;
        }
        MissionGeoFenceCircle circle;
        bool inclusion = true;
        if (!bool_field(item, "inclusion", inclusion)) {
            error = "QGC geoFence circle inclusion is required";
            return false;
        }
        circle.inclusion = inclusion;
        cJSON* circle_object = object_field(item, "circle");
        if (!circle_object || !cJSON_IsObject(circle_object)) {
            error = "QGC geoFence circle payload is required";
            return false;
        }
        cJSON* center = object_field(circle_object, "center");
        cJSON* radius = object_field(circle_object, "radius");
        if (!center || !cJSON_IsArray(center) || cJSON_GetArraySize(center) != 2 ||
            !radius || !cJSON_IsNumber(radius)) {
            error = "QGC geoFence circle center/radius are invalid";
            return false;
        }
        circle.center.latitude = number_or_nan(cJSON_GetArrayItem(center, 0));
        circle.center.longitude = number_or_nan(cJSON_GetArrayItem(center, 1));
        circle.radius_m = radius->valuedouble;
        document.geo_fence.circles.push_back(circle);
    }

    cJSON* breach = object_field(fence, "breachReturn");
    if (breach) {
        if (!cJSON_IsArray(breach) || cJSON_GetArraySize(breach) != 3) {
            error = "QGC geoFence breachReturn must contain three values";
            return false;
        }
        document.geo_fence.breach_return_set = true;
        document.geo_fence.breach_return.latitude = number_or_nan(cJSON_GetArrayItem(breach, 0));
        document.geo_fence.breach_return.longitude = number_or_nan(cJSON_GetArrayItem(breach, 1));
        document.geo_fence.breach_return_altitude_m = static_cast<float>(
            number_or_nan(cJSON_GetArrayItem(breach, 2)));
    }
    return true;
}
} // namespace

std::string export_qgc_plan_json(
    const MissionDocument& document,
    std::vector<std::string>& warnings,
    std::string& error) {
    error.clear();
    warnings.clear();

    for (const MissionEntity& entity : document.entities) {
        if (!entity.enabled) {
            error = "QGC .plan export cannot represent disabled mission entity: " +
                    entity.id;
            return {};
        }
        if (is_parameterized_survey_type(entity.type)) {
            if (entity.type == "survey_grid" && entity.survey.cross_grid) {
                add_warning(
                    warnings,
                    "survey entity " + entity.id +
                        " uses a cross grid; QGC Survey cannot reproduce the "
                        "second pass from its single grid angle");
            }
            add_warning(
                warnings,
                "survey entity " + entity.id +
                    " is exported as a QGC complex item; editing its parameters "
                    "in QGC regenerates the path with QGC geometry");
            continue;
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

    if (document.metadata.firmware_profile != "ardupilot") {
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

    // Preserved complex items are interleaved with simple entities so the QGC
    // plan keeps its original item order instead of appending them at the end.
    std::vector<size_t> complex_order(document.complex_items.size());
    for (size_t i = 0; i < complex_order.size(); ++i) {
        complex_order[i] = i;
    }
    std::stable_sort(
        complex_order.begin(),
        complex_order.end(),
        [&document](size_t a, size_t b) {
            return document.complex_items[a].entity_insert_index <
                   document.complex_items[b].entity_insert_index;
        });

    size_t next_complex = 0;
    const size_t entity_count = document.entities.size();
    auto emit_complex_before = [&](size_t entity_index) -> bool {
        while (next_complex < complex_order.size() &&
               document.complex_items[complex_order[next_complex]]
                       .entity_insert_index <= entity_index) {
            const MissionComplexItem& complex =
                document.complex_items[complex_order[next_complex]];
            if (!append_complex_item_json(items, complex, sequence, error)) {
                return false;
            }
            ++sequence;
            ++next_complex;
        }
        return true;
    };

    for (size_t index = 0; index < entity_count; ++index) {
        if (!emit_complex_before(index)) return {};
        const MissionEntity& entity = document.entities[index];

        // Survey功能已删除：跳过survey类型并记录警告
        if (is_parameterized_survey_type(entity.type)) {
            add_warning(
                warnings,
                "survey entity " + entity.id +
                    " skipped (survey generation removed in 0.2.x)");
            continue;
        }

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
        if (is_global_coordinate_frame(entity.frame) &&
            mission_command_uses_location(entity.command)) {
            add_number_or_null(params, entity.latitude);
            add_number_or_null(params, entity.longitude);
        } else {
            // MAV_FRAME_MISSION 的 param5/6 不是坐标；不要因为文档里的
            // 默认值或旧导入残留而把它们写成假经纬度。
            add_number_or_null(params, std::numeric_limits<double>::quiet_NaN());
            add_number_or_null(params, std::numeric_limits<double>::quiet_NaN());
        }
        add_number_or_null(params, entity.altitude_m);
        if (!entity.extensions_json.empty()) {
            cJSON* extras = mg::core::json::parse(entity.extensions_json.c_str());
            merge_object_fields(item, extras);
            cJSON_Delete(extras);
        }
        cJSON_AddItemToArray(items, item);
    }
    // Flush trailing complex items, including the append-at-end sentinel.
    if (!emit_complex_before(entity_count)) return {};
    while (next_complex < complex_order.size()) {
        const MissionComplexItem& complex =
            document.complex_items[complex_order[next_complex]];
        if (!append_complex_item_json(items, complex, sequence, error)) {
            return {};
        }
        ++sequence;
        ++next_complex;
    }

    add_geo_fence(root.get(), document, "geoFence");
    add_rally_points(root.get(), document, "rallyPoints");

    // Re-emit the fields captured on import that M-GCS does not model
    // natively, so a QGC .plan round-trip keeps the user's extra sections and
    // the original hoverSpeed / globalPlanAltitudeMode values.
    {
        cJSON* root_extras =
            extension_object(document.extensions, "qgc_plan.root_fields");
        merge_object_fields(root.get(), root_extras);
        cJSON_Delete(root_extras);
        cJSON* mission_extras =
            extension_object(document.extensions, "qgc_plan.mission_fields");
        merge_object_fields(mission, mission_extras);
        cJSON_Delete(mission_extras);
    }

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
        mg::core::json::parse(text.c_str()), cJSON_Delete);
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
    parsed.complex_items.clear();
    parsed.extensions.erase("geoFence");
    parsed.extensions.erase("rallyPoints");
    parsed.extensions.erase("qgc_plan.root_fields");
    parsed.extensions.erase("qgc_plan.mission_fields");
    parsed.rally_points.clear();
    parsed.geo_fence = MissionGeoFence{};
    if (!parse_geo_fence(root.get(), parsed, warnings, error)) return false;
    if (!parse_rally_points(root.get(), parsed, warnings, error)) return false;

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
            append_complex_item(parsed, item, index);
            add_warning(
                warnings,
                "QGC item type '" + item_type +
                    "' preserved as a complex item (read-only, not yet "
                    "semantically editable)");
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
            if (is_global_coordinate_frame(entity.frame) &&
                mission_command_uses_location(entity.command)) {
                entity.latitude = number_or_nan(cJSON_GetArrayItem(params, 4));
                entity.longitude = number_or_nan(cJSON_GetArrayItem(params, 5));
            }
            entity.altitude_m = static_cast<float>(
                number_or_nan(cJSON_GetArrayItem(params, 6)));
        }

        cJSON* coordinate = object_field(item, "coordinate");
        if (coordinate && cJSON_IsArray(coordinate) && cJSON_GetArraySize(coordinate) >= 3) {
            if (is_global_coordinate_frame(entity.frame) &&
                mission_command_uses_location(entity.command)) {
                entity.latitude = number_or_nan(cJSON_GetArrayItem(coordinate, 0));
                entity.longitude = number_or_nan(cJSON_GetArrayItem(coordinate, 1));
            }
            entity.altitude_m = static_cast<float>(
                number_or_nan(cJSON_GetArrayItem(coordinate, 2)));
        }

        // Fields M-GCS does not model natively (altitudeMode, altitude,
        // AMSLAltAboveTerrain, ...) are kept verbatim on the entity so the
        // export puts them back instead of dropping user data.
        attach_item_extensions(item, kSimpleItemKnownKeys, entity);
        if (!catalog_has_command(static_cast<uint16_t>(command))) {
            add_warning(
                warnings,
                "QGC item " + std::to_string(index) +
                    " uses command " + std::to_string(command) +
                    ", which is not in the supported command catalog");
        }
        parsed.entities.push_back(std::move(entity));
    }

    // Preserve unknown top-level and mission-object sections so a QGC plan
    // that carries extra metadata survives the round-trip.
    {
        cJSON* root_extras = collect_extra_fields(root.get(), kPlanRootKnownKeys);
        if (root_extras) {
            if (root_extras->child) {
                parsed.extensions["qgc_plan.root_fields"] = compact_json(root_extras);
            }
            cJSON_Delete(root_extras);
        }
        cJSON* mission_extras = collect_extra_fields(mission, kMissionKnownKeys);
        if (mission_extras) {
            if (mission_extras->child) {
                parsed.extensions["qgc_plan.mission_fields"] =
                    compact_json(mission_extras);
            }
            cJSON_Delete(mission_extras);
        }
    }

    document.home = parsed.home;
    document.metadata = parsed.metadata;
    document.rally_points = std::move(parsed.rally_points);
    document.geo_fence = std::move(parsed.geo_fence);
    document.defaults = parsed.defaults;
    document.entities = std::move(parsed.entities);
    document.complex_items = std::move(parsed.complex_items);
    document.extensions = std::move(parsed.extensions);
    return true;
}

bool expand_complex_item_entities(
    const MissionComplexItem& complex,
    std::vector<MissionEntity>& entities,
    std::string& error) {
    error.clear();
    entities.clear();

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        mg::core::json::parse(complex.json.c_str()), cJSON_Delete);
    if (!root || !cJSON_IsObject(root.get())) {
        error = "complex item '" + complex.id + "' payload is not a JSON object";
        return false;
    }

    // QGC writes the internal MissionItem list into the complex item when the
    // plan is saved (TransectStyleComplexItem::_save), so the expanded geometry
    // can be reused verbatim instead of re-running the survey generator.
    cJSON* transect = object_field(root.get(), "TransectStyleComplexItem");
    if (!transect || !cJSON_IsObject(transect)) {
        error = "complex item '" + complex.id + "' (" + complex.item_type +
                ") has no TransectStyleComplexItem geometry to expand";
        return false;
    }
    cJSON* items = object_field(transect, "Items");
    if (!items || !cJSON_IsArray(items) || cJSON_GetArraySize(items) == 0) {
        error = "complex item '" + complex.id +
                "' has no stored mission items to expand";
        return false;
    }

    const int count = cJSON_GetArraySize(items);
    for (int index = 0; index < count; ++index) {
        cJSON* item = cJSON_GetArrayItem(items, index);
        if (!item || !cJSON_IsObject(item)) {
            error = "complex item '" + complex.id + "' mission item " +
                    std::to_string(index) + " is not an object";
            entities.clear();
            return false;
        }
        MissionEntity entity;
        const std::string id =
            complex.id + "-item-" + std::to_string(index + 1);
        if (!parse_qgc_simple_item(item, id, static_cast<uint32_t>(index),
                                   entity, error)) {
            entities.clear();
            return false;
        }
        entities.push_back(std::move(entity));
    }
    return true;
}

} // namespace mg::core::mission::cad
