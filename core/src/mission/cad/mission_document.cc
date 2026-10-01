#include "core/mission/cad/mission_document.h"

#include "mission_document_internal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace mg::core::mission::cad {

namespace {

using detail::finite;
constexpr uint64_t kMaxJsonSafeInteger = 9007199254740991ULL;

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

cJSON* get_item(cJSON* object, const char* key) {
    return object ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
}

std::string get_string(cJSON* object, const char* key, const std::string& fallback) {
    cJSON* item = get_item(object, key);
    return item && cJSON_IsString(item) && item->valuestring
        ? std::string(item->valuestring)
        : fallback;
}

double get_double(cJSON* object, const char* key, double fallback) {
    cJSON* item = get_item(object, key);
    return item && cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

float get_float(cJSON* object, const char* key, float fallback) {
    return static_cast<float>(get_double(object, key, fallback));
}

int get_int(cJSON* object, const char* key, int fallback) {
    cJSON* item = get_item(object, key);
    return item && cJSON_IsNumber(item) ? item->valueint : fallback;
}

uint64_t get_u64(cJSON* object, const char* key, uint64_t fallback) {
    cJSON* item = get_item(object, key);
    if (!item || !cJSON_IsNumber(item) ||
        item->valuedouble < 0.0 ||
        item->valuedouble >
            static_cast<double>(std::numeric_limits<uint64_t>::max())) {
        return fallback;
    }
    return static_cast<uint64_t>(item->valuedouble);
}

bool get_bool(cJSON* object, const char* key, bool fallback) {
    cJSON* item = get_item(object, key);
    if (!item) return fallback;
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    if (cJSON_IsNumber(item)) return item->valueint != 0;
    return fallback;
}

void add_optional_number(cJSON* object, const char* key, double value) {
    if (finite(value)) {
        cJSON_AddNumberToObject(object, key, value);
    }
}

void add_optional_float(cJSON* object, const char* key, float value) {
    if (std::isfinite(value)) {
        cJSON_AddNumberToObject(object, key, value);
    }
}

bool parse_entity(
    cJSON* item,
    MissionEntity& entity,
    std::string& error) {
    if (!item || !cJSON_IsObject(item)) {
        error = "entity must be an object";
        return false;
    }

    const int command = get_int(item, "command", static_cast<int>(entity.command));
    const int frame = get_int(item, "frame", static_cast<int>(entity.frame));
    const int autocontinue = get_int(item, "autocontinue", entity.autocontinue);
    const int order = get_int(item, "order", static_cast<int>(entity.order));
    if (command < 0 || command > std::numeric_limits<uint16_t>::max()) {
        error = "entity command is out of range";
        return false;
    }
    if (frame < 0 || frame > std::numeric_limits<uint8_t>::max()) {
        error = "entity frame is out of range";
        return false;
    }
    if (autocontinue != 0 && autocontinue != 1) {
        error = "entity autocontinue must be 0 or 1";
        return false;
    }
    if (order < 0) {
        error = "entity order must be non-negative";
        return false;
    }

    entity.id = get_string(item, "id", {});
    entity.type = get_string(item, "type", entity.type);
    entity.label = get_string(item, "label", {});
    entity.enabled = get_bool(item, "enabled", true);
    entity.order = static_cast<uint32_t>(order);
    entity.command = static_cast<uint16_t>(command);
    entity.frame = static_cast<uint8_t>(frame);
    entity.autocontinue = static_cast<uint8_t>(autocontinue);
    entity.latitude = get_double(
        item, "lat", get_double(item, "latitude", entity.latitude));
    entity.longitude = get_double(
        item, "lon", get_double(item, "longitude", entity.longitude));
    entity.altitude_m = get_float(
        item, "alt_m", get_float(item, "altitude_m", entity.altitude_m));
    entity.notes = get_string(item, "notes", {});

    cJSON* params = get_item(item, "params");
    if (params && cJSON_IsArray(params)) {
        const int count = std::min(4, cJSON_GetArraySize(params));
        for (int i = 0; i < count; ++i) {
            cJSON* value = cJSON_GetArrayItem(params, i);
            if (value && cJSON_IsNumber(value)) {
                entity.params[static_cast<size_t>(i)] =
                    static_cast<float>(value->valuedouble);
            }
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            const std::string key = "param" + std::to_string(i + 1);
            entity.params[static_cast<size_t>(i)] =
                get_float(item, key.c_str(), entity.params[static_cast<size_t>(i)]);
        }
    }

    cJSON* survey = get_item(item, "survey");
    if (survey && cJSON_IsObject(survey)) {
        MissionSurveySpec& spec = entity.survey;
        spec.width_m = get_double(survey, "width_m", spec.width_m);
        spec.length_m = get_double(survey, "length_m", spec.length_m);
        spec.angle_deg = get_double(survey, "angle_deg", spec.angle_deg);
        spec.cross_grid = get_bool(survey, "cross_grid", spec.cross_grid);
        spec.end_latitude = get_double(
            survey, "end_lat", get_double(survey, "end_latitude", spec.end_latitude));
        spec.end_longitude = get_double(
            survey, "end_lon", get_double(survey, "end_longitude", spec.end_longitude));
        spec.corridor_width_m = get_double(
            survey, "corridor_width_m", spec.corridor_width_m);
        spec.altitude_m = get_float(
            survey, "alt_m", get_float(survey, "altitude_m", spec.altitude_m));
        spec.speed_ms = get_float(
            survey, "speed_ms", spec.speed_ms);
        spec.return_to_start = get_bool(
            survey, "return_to_start", spec.return_to_start);
        spec.forward_overlap_pct = get_float(
            survey, "forward_overlap_pct", spec.forward_overlap_pct);
        spec.side_overlap_pct = get_float(
            survey, "side_overlap_pct", spec.side_overlap_pct);
        spec.focal_length_mm = get_float(
            survey, "focal_length_mm", spec.focal_length_mm);
        spec.sensor_width_mm = get_float(
            survey, "sensor_width_mm", spec.sensor_width_mm);
        spec.sensor_height_mm = get_float(
            survey, "sensor_height_mm", spec.sensor_height_mm);
        spec.image_width_px = get_float(
            survey, "image_width_px", spec.image_width_px);
        spec.image_height_px = get_float(
            survey, "image_height_px", spec.image_height_px);
    }

    return true;
}

} // namespace

bool MissionValidationReport::has_errors() const {
    return std::any_of(
        diagnostics.begin(),
        diagnostics.end(),
        [](const MissionDiagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Error;
        });
}

MissionDocumentService::MissionDocumentService() {
    document_.metadata.id = "mission-" + std::to_string(now_ms());
    document_.metadata.name = "Untitled Mission";
    document_.updated_at_ms = now_ms();
}

MissionDocumentService::MissionDocumentService(MissionDocument document)
    : document_(std::move(document)) {
    if (document_.metadata.id.empty()) {
        document_.metadata.id = "mission-" + std::to_string(now_ms());
    }
    if (document_.revision == 0) {
        document_.revision = 1;
    }
    next_revision_ = document_.revision + 1;
    if (document_.updated_at_ms == 0) {
        document_.updated_at_ms = now_ms();
    }
    {
        mg::core::platform::LockGuard lock(mtx_);
        normalize_locked();
    }
}

MissionDocument MissionDocumentService::snapshot() const {
    mg::core::platform::LockGuard lock(mtx_);
    return document_;
}

uint64_t MissionDocumentService::revision() const {
    mg::core::platform::LockGuard lock(mtx_);
    return document_.revision;
}

bool MissionDocumentService::can_undo() const {
    mg::core::platform::LockGuard lock(mtx_);
    return !undo_stack_.empty();
}

bool MissionDocumentService::can_redo() const {
    mg::core::platform::LockGuard lock(mtx_);
    return !redo_stack_.empty();
}

MissionOperationResult MissionDocumentService::replace_document(
    MissionDocument document,
    uint64_t expected_revision) {
    mg::core::platform::LockGuard lock(mtx_);
    std::string error;
    if (!check_revision_locked(expected_revision, error)) {
        return {false, MissionOperationError::Conflict, error, document_.revision, document_};
    }

    const MissionDocument before = document_;
    if (document.schema_version <= 0) {
        document.schema_version = kMissionDocumentSchemaVersion;
    }
    if (document.schema_version > kMissionDocumentSchemaVersion) {
        return {
            false,
            MissionOperationError::InvalidParam,
            "unsupported mission document schema version",
            document_.revision,
            document_,
        };
    }
    if (document.revision > kMaxJsonSafeInteger) {
        return {
            false,
            MissionOperationError::InvalidParam,
            "mission document revision exceeds the JSON safe integer range",
            document_.revision,
            document_,
        };
    }
    if (document.metadata.id.empty()) {
        document.metadata.id = "mission-" + std::to_string(now_ms());
    }
    if (document.metadata.name.empty()) {
        document.metadata.name = "Untitled Mission";
    }
    document_ = std::move(document);
    normalize_locked();
    next_revision_ = std::max(next_revision_, document_.revision + 1);
    return commit_locked(before);
}

MissionOperationResult MissionDocumentService::add_entity(
    MissionEntity entity,
    uint64_t expected_revision) {
    mg::core::platform::LockGuard lock(mtx_);
    std::string error;
    if (!check_revision_locked(expected_revision, error)) {
        return {false, MissionOperationError::Conflict, error, document_.revision, document_};
    }
    if (entity.id.empty()) {
        entity.id = next_entity_id_locked();
    }
    const bool duplicate = std::any_of(
        document_.entities.begin(),
        document_.entities.end(),
        [&](const MissionEntity& existing) { return existing.id == entity.id; });
    if (duplicate) {
        return {
            false,
            MissionOperationError::InvalidParam,
            "entity id already exists: " + entity.id,
            document_.revision,
            document_,
        };
    }

    const MissionDocument before = document_;
    entity.order = static_cast<uint32_t>(document_.entities.size());
    document_.entities.push_back(std::move(entity));
    normalize_locked();
    return commit_locked(before);
}

MissionOperationResult MissionDocumentService::update_entity(
    MissionEntity entity,
    uint64_t expected_revision) {
    mg::core::platform::LockGuard lock(mtx_);
    std::string error;
    if (!check_revision_locked(expected_revision, error)) {
        return {false, MissionOperationError::Conflict, error, document_.revision, document_};
    }
    if (entity.id.empty()) {
        return {
            false,
            MissionOperationError::InvalidParam,
            "entity id is required",
            document_.revision,
            document_,
        };
    }

    auto found = std::find_if(
        document_.entities.begin(),
        document_.entities.end(),
        [&](const MissionEntity& existing) { return existing.id == entity.id; });
    if (found == document_.entities.end()) {
        return {
            false,
            MissionOperationError::NotFound,
            "entity not found: " + entity.id,
            document_.revision,
            document_,
        };
    }

    const MissionDocument before = document_;
    entity.order = found->order;
    *found = std::move(entity);
    normalize_locked();
    return commit_locked(before);
}

MissionOperationResult MissionDocumentService::remove_entity(
    const std::string& entity_id,
    uint64_t expected_revision) {
    mg::core::platform::LockGuard lock(mtx_);
    std::string error;
    if (!check_revision_locked(expected_revision, error)) {
        return {false, MissionOperationError::Conflict, error, document_.revision, document_};
    }

    auto found = std::find_if(
        document_.entities.begin(),
        document_.entities.end(),
        [&](const MissionEntity& existing) { return existing.id == entity_id; });
    if (found == document_.entities.end()) {
        return {
            false,
            MissionOperationError::NotFound,
            "entity not found: " + entity_id,
            document_.revision,
            document_,
        };
    }

    const MissionDocument before = document_;
    document_.entities.erase(found);
    normalize_locked();
    return commit_locked(before);
}

MissionOperationResult MissionDocumentService::move_entity(
    const std::string& entity_id,
    uint32_t to_index,
    uint64_t expected_revision) {
    mg::core::platform::LockGuard lock(mtx_);
    std::string error;
    if (!check_revision_locked(expected_revision, error)) {
        return {false, MissionOperationError::Conflict, error, document_.revision, document_};
    }

    auto found = std::find_if(
        document_.entities.begin(),
        document_.entities.end(),
        [&](const MissionEntity& existing) { return existing.id == entity_id; });
    if (found == document_.entities.end()) {
        return {
            false,
            MissionOperationError::NotFound,
            "entity not found: " + entity_id,
            document_.revision,
            document_,
        };
    }

    const MissionDocument before = document_;
    MissionEntity entity = *found;
    document_.entities.erase(found);
    const size_t bounded = std::min<size_t>(to_index, document_.entities.size());
    document_.entities.insert(document_.entities.begin() + bounded, std::move(entity));
    normalize_locked();
    return commit_locked(before);
}

MissionOperationResult MissionDocumentService::clear(uint64_t expected_revision) {
    mg::core::platform::LockGuard lock(mtx_);
    std::string error;
    if (!check_revision_locked(expected_revision, error)) {
        return {false, MissionOperationError::Conflict, error, document_.revision, document_};
    }
    if (document_.entities.empty()) {
        return {
            false,
            MissionOperationError::InvalidState,
            "mission document is already empty",
            document_.revision,
            document_,
        };
    }

    const MissionDocument before = document_;
    document_.entities.clear();
    normalize_locked();
    return commit_locked(before);
}

MissionOperationResult MissionDocumentService::undo() {
    mg::core::platform::LockGuard lock(mtx_);
    if (undo_stack_.empty()) {
        return {
            false,
            MissionOperationError::InvalidState,
            "nothing to undo",
            document_.revision,
            document_,
        };
    }

    redo_stack_.push_back(document_);
    document_ = undo_stack_.back();
    undo_stack_.pop_back();
    document_.revision = next_revision_++;
    return {true, MissionOperationError::None, {}, document_.revision, document_};
}

MissionOperationResult MissionDocumentService::redo() {
    mg::core::platform::LockGuard lock(mtx_);
    if (redo_stack_.empty()) {
        return {
            false,
            MissionOperationError::InvalidState,
            "nothing to redo",
            document_.revision,
            document_,
        };
    }

    undo_stack_.push_back(document_);
    document_ = redo_stack_.back();
    redo_stack_.pop_back();
    document_.revision = next_revision_++;
    return {true, MissionOperationError::None, {}, document_.revision, document_};
}

MissionValidationReport MissionDocumentService::validate() const {
    mg::core::platform::LockGuard lock(mtx_);
    return validate_document(document_);
}

MissionCompileResult MissionDocumentService::compile() const {
    mg::core::platform::LockGuard lock(mtx_);
    auto result = compile_document(document_);
    result.document_revision = document_.revision;
    return result;
}

MissionCompileResult MissionDocumentService::compile_at_revision(
    uint64_t expected_revision,
    std::string& error) const {
    mg::core::platform::LockGuard lock(mtx_);
    if (expected_revision == 0 ||
        expected_revision != document_.revision) {
        error =
            "revision mismatch: expected " + std::to_string(expected_revision) +
            ", current " + std::to_string(document_.revision);
        MissionCompileResult result;
        result.document_revision = document_.revision;
        result.report.valid = false;
        return result;
    }
    auto result = compile_document(document_);
    result.document_revision = document_.revision;
    return result;
}

MissionEstimate MissionDocumentService::estimate() const {
    mg::core::platform::LockGuard lock(mtx_);
    return estimate_document(document_);
}

MissionValidationReport MissionDocumentService::validate_document(
    const MissionDocument& document) {
    return detail::validate_document(document);
}

MissionCompileResult MissionDocumentService::compile_document(
    const MissionDocument& document) {
    return detail::compile_document_impl(document);
}

MissionEstimate MissionDocumentService::estimate_document(
    const MissionDocument& document) {
    return detail::estimate_document_impl(document);
}

MissionOperationResult MissionDocumentService::commit_locked(
    MissionDocument before,
    std::string error) {
    if (!error.empty()) {
        return {false, MissionOperationError::Internal, std::move(error), document_.revision, document_};
    }

    if (undo_stack_.size() >= kMaxHistory) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_stack_.push_back(std::move(before));
    redo_stack_.clear();
    document_.revision = next_revision_++;
    document_.updated_at_ms = now_ms();
    return {true, MissionOperationError::None, {}, document_.revision, document_};
}

void MissionDocumentService::normalize_locked() {
    for (size_t index = 0; index < document_.entities.size(); ++index) {
        document_.entities[index].order = static_cast<uint32_t>(index);
        if (document_.entities[index].id.empty()) {
            document_.entities[index].id = next_entity_id_locked();
        }
    }
}

bool MissionDocumentService::check_revision_locked(
    uint64_t expected,
    std::string& error) const {
    if (expected != 0 && expected != document_.revision) {
        error =
            "revision mismatch: expected " + std::to_string(expected) +
            ", current " + std::to_string(document_.revision);
        return false;
    }
    return true;
}

std::string MissionDocumentService::next_entity_id_locked() {
    std::unordered_set<std::string> existing;
    for (const MissionEntity& entity : document_.entities) {
        existing.insert(entity.id);
    }
    std::string candidate;
    do {
        candidate = "entity-" + std::to_string(next_entity_id_++);
    } while (existing.count(candidate) != 0);
    return candidate;
}

std::string serialize_mission_document(const MissionDocument& document) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "schema_version", document.schema_version);

    cJSON* metadata = cJSON_AddObjectToObject(root, "metadata");
    cJSON_AddStringToObject(metadata, "id", document.metadata.id.c_str());
    cJSON_AddStringToObject(metadata, "name", document.metadata.name.c_str());
    cJSON_AddStringToObject(
        metadata, "description", document.metadata.description.c_str());
    cJSON_AddStringToObject(
        metadata,
        "firmware_profile",
        document.metadata.firmware_profile.c_str());
    cJSON_AddStringToObject(
        metadata, "vehicle_type", document.metadata.vehicle_type.c_str());

    cJSON* home = cJSON_AddObjectToObject(root, "home");
    cJSON_AddBoolToObject(home, "set", document.home.set);
    add_optional_number(home, "lat", document.home.latitude);
    add_optional_number(home, "lon", document.home.longitude);
    cJSON_AddNumberToObject(home, "alt_m", document.home.altitude_m);

    cJSON* defaults = cJSON_AddObjectToObject(root, "defaults");
    cJSON_AddNumberToObject(defaults, "frame", document.defaults.frame);
    cJSON_AddNumberToObject(
        defaults, "altitude_m", document.defaults.altitude_m);
    cJSON_AddNumberToObject(defaults, "speed_ms", document.defaults.speed_ms);

    if (!document.rally_points.empty()) {
        cJSON* rally_points = cJSON_AddArrayToObject(root, "rally_points");
        for (const MissionRallyPoint& point : document.rally_points) {
            cJSON* item = cJSON_CreateObject();
            add_optional_number(item, "lat", point.latitude);
            add_optional_number(item, "lon", point.longitude);
            cJSON_AddNumberToObject(item, "alt_m", point.altitude_m);
            cJSON_AddItemToArray(rally_points, item);
        }
    }

    const auto& geo_fence = document.geo_fence;
    if (!geo_fence.polygons.empty() || !geo_fence.circles.empty() ||
        geo_fence.breach_return_set) {
        cJSON* fence = cJSON_AddObjectToObject(root, "geo_fence");
        cJSON* polygons = cJSON_AddArrayToObject(fence, "polygons");
        for (const auto& polygon : geo_fence.polygons) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddBoolToObject(item, "inclusion", polygon.inclusion);
            cJSON* vertices = cJSON_AddArrayToObject(item, "vertices");
            for (const auto& vertex : polygon.vertices) {
                cJSON* point = cJSON_CreateArray();
                add_optional_number(point, "lat", vertex.latitude);
                add_optional_number(point, "lon", vertex.longitude);
                cJSON_AddItemToArray(vertices, point);
            }
            cJSON_AddItemToArray(polygons, item);
        }
        cJSON* circles = cJSON_AddArrayToObject(fence, "circles");
        for (const auto& circle : geo_fence.circles) {
            cJSON* item = cJSON_CreateObject();
            cJSON_AddBoolToObject(item, "inclusion", circle.inclusion);
            cJSON_AddNumberToObject(item, "lat", circle.center.latitude);
            cJSON_AddNumberToObject(item, "lon", circle.center.longitude);
            cJSON_AddNumberToObject(item, "radius_m", circle.radius_m);
            cJSON_AddItemToArray(circles, item);
        }
        if (geo_fence.breach_return_set) {
            cJSON* breach = cJSON_AddObjectToObject(fence, "breach_return");
            add_optional_number(breach, "lat", geo_fence.breach_return.latitude);
            add_optional_number(breach, "lon", geo_fence.breach_return.longitude);
            cJSON_AddNumberToObject(breach, "alt_m", geo_fence.breach_return_altitude_m);
        }
    }

    cJSON_AddNumberToObject(root, "revision", document.revision);
    cJSON_AddNumberToObject(root, "updated_at_ms", document.updated_at_ms);

    if (!document.extensions.empty()) {
        cJSON* extensions = cJSON_AddObjectToObject(root, "extensions");
        for (const auto& [key, raw_json] : document.extensions) {
            cJSON* value = cJSON_Parse(raw_json.c_str());
            if (!value) continue;
            cJSON_AddItemToObject(extensions, key.c_str(), value);
        }
    }

    cJSON* entities = cJSON_AddArrayToObject(root, "entities");
    for (const MissionEntity& entity : document.entities) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", entity.id.c_str());
        cJSON_AddStringToObject(item, "type", entity.type.c_str());
        if (!entity.label.empty()) {
            cJSON_AddStringToObject(item, "label", entity.label.c_str());
        }
        cJSON_AddBoolToObject(item, "enabled", entity.enabled);
        cJSON_AddNumberToObject(item, "order", entity.order);
        cJSON_AddNumberToObject(item, "command", entity.command);
        cJSON_AddNumberToObject(item, "frame", entity.frame);
        cJSON_AddNumberToObject(item, "autocontinue", entity.autocontinue);
        add_optional_number(item, "lat", entity.latitude);
        add_optional_number(item, "lon", entity.longitude);
        add_optional_float(item, "alt_m", entity.altitude_m);
        cJSON* params = cJSON_AddArrayToObject(item, "params");
        for (float value : entity.params) {
            cJSON_AddItemToArray(params, cJSON_CreateNumber(value));
        }
        if (is_parameterized_survey_type(entity.type)) {
            const MissionSurveySpec& spec = entity.survey;
            cJSON* survey = cJSON_AddObjectToObject(item, "survey");
            cJSON_AddNumberToObject(survey, "width_m", spec.width_m);
            cJSON_AddNumberToObject(survey, "length_m", spec.length_m);
            cJSON_AddNumberToObject(survey, "angle_deg", spec.angle_deg);
            cJSON_AddBoolToObject(survey, "cross_grid", spec.cross_grid);
            cJSON_AddNumberToObject(
                survey, "corridor_width_m", spec.corridor_width_m);
            add_optional_number(survey, "end_lat", spec.end_latitude);
            add_optional_number(survey, "end_lon", spec.end_longitude);
            cJSON_AddNumberToObject(survey, "alt_m", spec.altitude_m);
            cJSON_AddNumberToObject(survey, "speed_ms", spec.speed_ms);
            cJSON_AddBoolToObject(
                survey, "return_to_start", spec.return_to_start);
            cJSON_AddNumberToObject(
                survey, "forward_overlap_pct", spec.forward_overlap_pct);
            cJSON_AddNumberToObject(
                survey, "side_overlap_pct", spec.side_overlap_pct);
            cJSON_AddNumberToObject(
                survey, "focal_length_mm", spec.focal_length_mm);
            cJSON_AddNumberToObject(
                survey, "sensor_width_mm", spec.sensor_width_mm);
            cJSON_AddNumberToObject(
                survey, "sensor_height_mm", spec.sensor_height_mm);
            cJSON_AddNumberToObject(
                survey, "image_width_px", spec.image_width_px);
            cJSON_AddNumberToObject(
                survey, "image_height_px", spec.image_height_px);
        }
        if (!entity.notes.empty()) {
            cJSON_AddStringToObject(item, "notes", entity.notes.c_str());
        }
        cJSON_AddItemToArray(entities, item);
    }

    char* json = cJSON_PrintUnformatted(root);
    std::string result = json ? json : "{}";
    cJSON_free(json);
    cJSON_Delete(root);
    return result;
}

bool deserialize_mission_document(
    const std::string& json,
    MissionDocument& document,
    std::string& error) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        error = "invalid mission document JSON";
        return false;
    }

    MissionDocument parsed;
    parsed.schema_version = get_int(
        root, "schema_version", kMissionDocumentSchemaVersion);
    if (parsed.schema_version <= 0 ||
        parsed.schema_version > kMissionDocumentSchemaVersion) {
        cJSON_Delete(root);
        error = "unsupported mission document schema version";
        return false;
    }

    cJSON* metadata = get_item(root, "metadata");
    if (metadata && cJSON_IsObject(metadata)) {
        parsed.metadata.id = get_string(metadata, "id", {});
        parsed.metadata.name = get_string(metadata, "name", "Untitled Mission");
        parsed.metadata.description = get_string(metadata, "description", {});
        parsed.metadata.firmware_profile =
            get_string(metadata, "firmware_profile", "ardupilot");
        parsed.metadata.vehicle_type =
            get_string(metadata, "vehicle_type", "copter");
    }

    cJSON* home = get_item(root, "home");
    if (home && cJSON_IsObject(home)) {
        parsed.home.set = get_bool(home, "set", false);
        parsed.home.latitude = get_double(
            home, "lat", std::numeric_limits<double>::quiet_NaN());
        parsed.home.longitude = get_double(
            home, "lon", std::numeric_limits<double>::quiet_NaN());
        parsed.home.altitude_m = get_float(home, "alt_m", 0.0f);
    }

    cJSON* defaults = get_item(root, "defaults");
    if (defaults && cJSON_IsObject(defaults)) {
        parsed.defaults.frame = static_cast<uint8_t>(get_int(
            defaults,
            "frame",
            static_cast<int>(CoordFrame::GlobalRelAlt)));
        parsed.defaults.altitude_m = get_float(defaults, "altitude_m", 50.0f);
        parsed.defaults.speed_ms = get_float(defaults, "speed_ms", 12.0f);
    }

    cJSON* rally_points = get_item(root, "rally_points");
    if (rally_points && cJSON_IsArray(rally_points)) {
        const int count = cJSON_GetArraySize(rally_points);
        parsed.rally_points.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            cJSON* item = cJSON_GetArrayItem(rally_points, i);
            if (!item || !cJSON_IsObject(item)) {
                cJSON_Delete(root);
                error = "rally point must be an object";
                return false;
            }
            MissionRallyPoint point;
            point.latitude = get_double(item, "lat", point.latitude);
            point.longitude = get_double(item, "lon", point.longitude);
            point.altitude_m = get_float(item, "alt_m", point.altitude_m);
            parsed.rally_points.push_back(point);
        }
    }

    cJSON* geo_fence = get_item(root, "geo_fence");
    if (geo_fence && cJSON_IsObject(geo_fence)) {
        cJSON* polygons = get_item(geo_fence, "polygons");
        if (polygons && cJSON_IsArray(polygons)) {
            for (int i = 0; i < cJSON_GetArraySize(polygons); ++i) {
                cJSON* item = cJSON_GetArrayItem(polygons, i);
                if (!item || !cJSON_IsObject(item)) {
                    cJSON_Delete(root);
                    error = "geo fence polygon must be an object";
                    return false;
                }
                MissionGeoFencePolygon polygon;
                polygon.inclusion = get_bool(item, "inclusion", true);
                cJSON* vertices = get_item(item, "vertices");
                if (!vertices || !cJSON_IsArray(vertices)) {
                    cJSON_Delete(root);
                    error = "geo fence polygon vertices must be an array";
                    return false;
                }
                for (int j = 0; j < cJSON_GetArraySize(vertices); ++j) {
                    cJSON* vertex = cJSON_GetArrayItem(vertices, j);
                    if (!vertex || !cJSON_IsArray(vertex) || cJSON_GetArraySize(vertex) < 2) {
                        cJSON_Delete(root);
                        error = "geo fence polygon vertex must be a coordinate";
                        return false;
                    }
                    MissionGeoPoint point;
                    cJSON* latitude = cJSON_GetArrayItem(vertex, 0);
                    cJSON* longitude = cJSON_GetArrayItem(vertex, 1);
                    if (latitude && cJSON_IsNumber(latitude)) point.latitude = latitude->valuedouble;
                    if (longitude && cJSON_IsNumber(longitude)) point.longitude = longitude->valuedouble;
                    polygon.vertices.push_back(point);
                }
                parsed.geo_fence.polygons.push_back(std::move(polygon));
            }
        }
        cJSON* circles = get_item(geo_fence, "circles");
        if (circles && cJSON_IsArray(circles)) {
            for (int i = 0; i < cJSON_GetArraySize(circles); ++i) {
                cJSON* item = cJSON_GetArrayItem(circles, i);
                if (!item || !cJSON_IsObject(item)) {
                    cJSON_Delete(root);
                    error = "geo fence circle must be an object";
                    return false;
                }
                MissionGeoFenceCircle circle;
                circle.inclusion = get_bool(item, "inclusion", true);
                circle.center.latitude = get_double(item, "lat", circle.center.latitude);
                circle.center.longitude = get_double(item, "lon", circle.center.longitude);
                circle.radius_m = get_double(item, "radius_m", 0.0);
                parsed.geo_fence.circles.push_back(circle);
            }
        }
        cJSON* breach = get_item(geo_fence, "breach_return");
        if (breach && cJSON_IsObject(breach)) {
            parsed.geo_fence.breach_return_set = true;
            parsed.geo_fence.breach_return.latitude = get_double(breach, "lat", parsed.geo_fence.breach_return.latitude);
            parsed.geo_fence.breach_return.longitude = get_double(breach, "lon", parsed.geo_fence.breach_return.longitude);
            parsed.geo_fence.breach_return_altitude_m = get_float(breach, "alt_m", 0.0f);
        }
    }

    const uint64_t revision = get_u64(root, "revision", 1);
    if (revision > kMaxJsonSafeInteger) {
        cJSON_Delete(root);
        error = "mission document revision exceeds the JSON safe integer range";
        return false;
    }
    parsed.revision = std::max<uint64_t>(1, revision);
    parsed.updated_at_ms = static_cast<int64_t>(
        get_double(root, "updated_at_ms", static_cast<double>(now_ms())));

    cJSON* extensions = get_item(root, "extensions");
    if (extensions && cJSON_IsObject(extensions)) {
        for (cJSON* entry = extensions->child; entry != nullptr; entry = entry->next) {
            if (!entry->string) continue;
            char* raw = cJSON_PrintUnformatted(entry);
            if (!raw) {
                cJSON_Delete(root);
                error = "mission document extension is not serializable";
                return false;
            }
            parsed.extensions[entry->string] = raw;
            cJSON_free(raw);
        }
    }

    cJSON* entities = get_item(root, "entities");
    if (entities && cJSON_IsArray(entities)) {
        const int count = cJSON_GetArraySize(entities);
        parsed.entities.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            MissionEntity entity{};
            if (!parse_entity(cJSON_GetArrayItem(entities, i), entity, error)) {
                cJSON_Delete(root);
                return false;
            }
            parsed.entities.push_back(std::move(entity));
        }
    }

    cJSON_Delete(root);
    document = std::move(parsed);
    error.clear();
    return true;
}

bool save_mission_document(
    const std::string& path,
    const MissionDocument& document,
    std::string& error) {
    if (path.empty()) {
        error = "file path is empty";
        return false;
    }

    const std::string json = serialize_mission_document(document);
    const std::string temporary = path + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            error = "failed to open mission document for writing";
            return false;
        }
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
        output.flush();
        if (!output.good()) {
            error = "failed to write mission document";
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        const std::string backup = path + ".bak";
        std::error_code backup_error;
        std::filesystem::remove(backup, backup_error);
        backup_error.clear();
        bool had_original = std::filesystem::exists(path);
        if (had_original) {
            std::filesystem::rename(path, backup, backup_error);
            if (backup_error) {
                error = "failed to stage existing mission document";
                return false;
            }
        }
        ec.clear();
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            std::error_code restore_error;
            if (had_original) {
                std::filesystem::rename(backup, path, restore_error);
            }
            std::filesystem::remove(temporary, restore_error);
            error = "failed to replace mission document file";
            return false;
        }
        if (had_original) {
            std::error_code cleanup_error;
            std::filesystem::remove(backup, cleanup_error);
        }
    }
    error.clear();
    return true;
}

bool load_mission_document(
    const std::string& path,
    MissionDocument& document,
    std::string& error) {
    if (path.empty()) {
        error = "file path is empty";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = "mission document file not found";
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return deserialize_mission_document(buffer.str(), document, error);
}

const char* mission_entity_type_name(const std::string& type) {
    return type.c_str();
}

bool is_parameterized_survey_type(const std::string& type) {
    return type == "survey_grid" || type == "corridor";
}

const char* mission_entity_type_for_command(uint16_t command) {
    switch (command) {
        case static_cast<uint16_t>(NavCommand::NavTakeoff):          return "takeoff";
        case static_cast<uint16_t>(NavCommand::NavLand):             return "land";
        case static_cast<uint16_t>(NavCommand::NavReturnToLaunch):   return "rtl";
        case static_cast<uint16_t>(NavCommand::NavLoiterTurns):
        case static_cast<uint16_t>(NavCommand::NavLoiterTime):
        case static_cast<uint16_t>(NavCommand::NavLoiterUnlim):      return "loiter";
        // 其余命令（改速度/相机/舵机/延时/距离条件）不属于上面的分类。
        // 返回 "command" 而不是 "waypoint"，否则校验会拿它跟 NAV_WAYPOINT 比对，
        // 给每个非航点命令都报一条 type_command_mismatch。
        default:                                                     return "command";
    }
}

uint8_t normalize_mission_frame(uint8_t frame) {
    switch (frame) {
        case 5:  return 0;   // GLOBAL_INT -> GLOBAL
        case 6:  return 3;   // GLOBAL_RELATIVE_ALT_INT -> GLOBAL_RELATIVE_ALT
        case 11: return 10;  // GLOBAL_TERRAIN_ALT_INT -> GLOBAL_TERRAIN_ALT
        default: return frame;
    }
}

bool import_mission_plan_into_document(
    const MissionPlan& plan,
    MissionDocument& document,
    std::string& error) {
    error.clear();
    if (plan.items.empty()) {
        error = "vehicle returned an empty mission";
        return false;
    }

    std::vector<MissionEntity> entities;
    entities.reserve(plan.items.size());
    int index = 0;
    for (const MissionItem& item : plan.items) {
        MissionEntity entity;
        entity.id = "wp-" + std::to_string(++index);
        entity.type = mission_entity_type_for_command(item.command);
        entity.command = item.command;
        entity.frame = normalize_mission_frame(item.frame);
        entity.autocontinue = item.autocontinue;
        entity.latitude = static_cast<double>(item.x) / 1e7;
        entity.longitude = static_cast<double>(item.y) / 1e7;
        entity.altitude_m = item.z;
        entity.params = {item.param1, item.param2, item.param3, item.param4};
        entity.order = static_cast<uint32_t>(entities.size());
        entities.push_back(std::move(entity));
    }

    document.entities = std::move(entities);
    return true;
}

} // namespace mg::core::mission::cad
