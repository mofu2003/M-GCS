#include "core/mission/cad/mission_document.h"

#include "core/mission/cad/mission_command_catalog.h"
#include "core/mission/mission_manager.h"

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

constexpr double kMinLatitude = -90.0;
constexpr double kMaxLatitude = 90.0;
constexpr double kMinLongitude = -180.0;
constexpr double kMaxLongitude = 180.0;
constexpr float kMaxMissionAltitudeM = 500.0f;
constexpr uint64_t kMaxJsonSafeInteger = 9007199254740991ULL;

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

bool finite(double value) {
    return std::isfinite(value);
}

bool position_command(uint16_t command) {
    switch (command) {
        case static_cast<uint16_t>(NavCommand::NavWaypoint):
        case static_cast<uint16_t>(NavCommand::NavLoiterUnlim):
        case static_cast<uint16_t>(NavCommand::NavLoiterTurns):
        case static_cast<uint16_t>(NavCommand::NavLoiterTime):
        case static_cast<uint16_t>(NavCommand::NavLand):
        case static_cast<uint16_t>(NavCommand::NavSplineWaypoint):
            return true;
        default:
            return false;
    }
}

bool type_has_expected_command(
    const std::string& type,
    uint16_t command,
    uint16_t& expected) {
    if (type == "waypoint") expected = static_cast<uint16_t>(NavCommand::NavWaypoint);
    else if (type == "takeoff") expected = static_cast<uint16_t>(NavCommand::NavTakeoff);
    else if (type == "land") expected = static_cast<uint16_t>(NavCommand::NavLand);
    else if (type == "rtl") expected = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
    else if (type == "loiter") expected = static_cast<uint16_t>(NavCommand::NavLoiterTurns);
    else return false;
    return command != expected;
}

const MissionCommandDescriptor* find_command_descriptor(uint16_t command) {
    const auto& catalog = mission_command_catalog();
    auto found = std::find_if(
        catalog.begin(),
        catalog.end(),
        [&](const MissionCommandDescriptor& descriptor) {
            return descriptor.command == command;
        });
    return found == catalog.end() ? nullptr : &*found;
}

void add_diagnostic(
    MissionValidationReport& report,
    DiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string entity_id = {},
    std::string field = {}) {
    report.diagnostics.push_back(
        MissionDiagnostic{
            std::move(code),
            severity,
            std::move(message),
            std::move(entity_id),
            std::move(field),
        });
    if (severity == DiagnosticSeverity::Error) {
        report.valid = false;
    }
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
    MissionValidationReport report;
    if (document.schema_version <= 0 ||
        document.schema_version > kMissionDocumentSchemaVersion) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "schema.unsupported",
            "Unsupported mission document schema version",
            {},
            "schema_version");
    }
    if (document.metadata.id.empty()) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Warning,
            "document.id_missing",
            "Mission document id is empty",
            {},
            "metadata.id");
    }
    if (!(document.defaults.altitude_m > 0.0f) ||
        !std::isfinite(document.defaults.altitude_m)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "defaults.altitude",
            "Default altitude must be positive",
            {},
            "defaults.altitude_m");
    }
    if (!(document.defaults.speed_ms > 0.0f) ||
        !std::isfinite(document.defaults.speed_ms)) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "defaults.speed",
            "Default speed must be positive",
            {},
            "defaults.speed_ms");
    }
    if (document.home.set) {
        if (!finite(document.home.latitude) ||
            document.home.latitude < kMinLatitude ||
            document.home.latitude > kMaxLatitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "home.latitude",
                "Home latitude is invalid",
                {},
                "home.latitude");
        }
        if (!finite(document.home.longitude) ||
            document.home.longitude < kMinLongitude ||
            document.home.longitude > kMaxLongitude) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "home.longitude",
                "Home longitude is invalid",
                {},
                "home.longitude");
        }
    } else {
        add_diagnostic(
            report,
            DiagnosticSeverity::Warning,
            "home.missing",
            "Home position is not set; relative altitudes cannot be previewed accurately",
            {},
            "home");
    }

    std::unordered_set<std::string> ids;
    size_t enabled_count = 0;
    std::vector<const MissionEntity*> enabled;
    for (size_t index = 0; index < document.entities.size(); ++index) {
        const MissionEntity& entity = document.entities[index];
        if (entity.id.empty()) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.id_missing",
                "Mission entity id is empty",
                entity.id,
                "id");
        } else if (!ids.insert(entity.id).second) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.id_duplicate",
                "Mission entity id is duplicated",
                entity.id,
                "id");
        }
        if (entity.order != index) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "entity.order",
                "Mission entity order does not match document order",
                entity.id,
                "order");
        }
        if (!entity.enabled) {
            continue;
        }
        ++enabled_count;
        enabled.push_back(&entity);

        if (entity.command == 0) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.command_missing",
                "Mission command is required",
                entity.id,
                "command");
            continue;
        }
        if (entity.frame != 0 && entity.frame != 2 &&
            entity.frame != 3 && entity.frame != 10) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.frame_unsupported",
                "Mission frame is not supported",
                entity.id,
                "frame");
        }
        if (entity.autocontinue > 1) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.autocontinue",
                "Autocontinue must be 0 or 1",
                entity.id,
                "autocontinue");
        }
        for (size_t parameter_index = 0;
             parameter_index < entity.params.size();
             ++parameter_index) {
            if (!std::isfinite(entity.params[parameter_index])) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_non_finite",
                    "Mission parameter must be finite",
                    entity.id,
                    "params[" + std::to_string(parameter_index) + "]");
            }
        }
        if (entity.type == "survey_grid" || entity.type == "corridor") {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.type_unsupported",
                "Parameterized survey entities are not implemented yet",
                entity.id,
                "type");
            continue;
        }

        const MissionCommandDescriptor* descriptor =
            find_command_descriptor(entity.command);
        if (!descriptor) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.command_unsupported",
                "Mission command is not present in the supported command catalog",
                entity.id,
                "command");
            continue;
        }

        uint16_t expected_command = 0;
        if (type_has_expected_command(entity.type, entity.command, expected_command)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "entity.type_command_mismatch",
                "Entity type does not match its command",
                entity.id,
                "command");
        }

        for (const MissionCommandParameter& parameter : descriptor->parameters) {
            if (parameter.index < 1 || parameter.index > 4) {
                continue;
            }
            const float value = entity.params[parameter.index - 1];
            const std::string field =
                "params[" + std::to_string(parameter.index - 1) + "]";
            if (parameter.has_minimum && value < parameter.minimum) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_minimum",
                    parameter.label + " is below the supported minimum",
                    entity.id,
                    field);
            }
            if (parameter.has_maximum && value > parameter.maximum) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_maximum",
                    parameter.label + " is above the supported maximum",
                    entity.id,
                    field);
            }
            if (parameter.type == MissionParameterType::Integer &&
                std::abs(value - std::round(value)) > 1e-6f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_integer",
                    parameter.label + " must be an integer",
                    entity.id,
                    field);
            }
            if (parameter.type == MissionParameterType::Boolean &&
                std::abs(value) > 1e-6f &&
                std::abs(value - 1.0f) > 1e-6f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.parameter_boolean",
                    parameter.label + " must be 0 or 1",
                    entity.id,
                    field);
            }
            if (parameter.type == MissionParameterType::Enum &&
                !parameter.options.empty()) {
                const bool option_matches = std::any_of(
                    parameter.options.begin(),
                    parameter.options.end(),
                    [&](const MissionCommandOption& option) {
                        return std::abs(value - option.value) < 1e-6;
                    });
                if (!option_matches) {
                    add_diagnostic(
                        report,
                        DiagnosticSeverity::Error,
                        "entity.parameter_enum",
                        parameter.label + " is not a supported option",
                        entity.id,
                        field);
                }
            }
        }

        if (position_command(entity.command)) {
            if (!finite(entity.latitude) ||
                entity.latitude < kMinLatitude ||
                entity.latitude > kMaxLatitude) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.latitude",
                    "Mission latitude is invalid or missing",
                    entity.id,
                    "lat");
            }
            if (!finite(entity.longitude) ||
                entity.longitude < kMinLongitude ||
                entity.longitude > kMaxLongitude) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.longitude",
                    "Mission longitude is invalid or missing",
                    entity.id,
                    "lon");
            }
        }

        const float altitude = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        if (position_command(entity.command) ||
            entity.command == static_cast<uint16_t>(NavCommand::NavTakeoff)) {
            if (!(altitude >= 0.0f)) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.altitude",
                    "Mission altitude must be non-negative",
                    entity.id,
                    "alt_m");
            } else if (altitude > kMaxMissionAltitudeM) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Warning,
                    "entity.altitude_high",
                    "Mission altitude is unusually high",
                    entity.id,
                    "alt_m");
            }
        }

        if (entity.command == static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            if (entity.params[0] < 0.0f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.hold_time",
                    "Waypoint hold time must be non-negative",
                    entity.id,
                    "params[0]");
            }
            if (entity.params[1] < 0.0f) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.accept_radius",
                    "Waypoint acceptance radius must be non-negative",
                    entity.id,
                    "params[1]");
            }
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavLoiterUnlim) ||
            entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns) ||
            entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTime)) {
            if (!(entity.params[2] > 0.0f)) {
                add_diagnostic(
                    report,
                    DiagnosticSeverity::Error,
                    "entity.loiter_radius",
                    "Loiter radius must be positive",
                    entity.id,
                    "params[2]");
            }
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::DoSetSpeed) &&
            !(entity.params[1] > 0.0f)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.speed",
                "Speed change must be positive",
                entity.id,
                "params[1]");
        }
        if (entity.command ==
                static_cast<uint16_t>(NavCommand::DoSetCamTrigDist) &&
            entity.params[0] < 0.0f) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Error,
                "entity.trigger_distance",
                "Camera trigger distance must be non-negative",
                entity.id,
                "params[0]");
        }
    }

    if (enabled_count == 0) {
        add_diagnostic(
            report,
            DiagnosticSeverity::Error,
            "mission.empty",
            "Mission document has no enabled entities",
            {},
            "entities");
    } else {
        const MissionEntity& first = *enabled.front();
        if (first.command != static_cast<uint16_t>(NavCommand::NavTakeoff) &&
            first.command != static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "mission.first_item",
                "First mission item should be takeoff or waypoint",
                first.id,
                "command");
        }
        const MissionEntity& last = *enabled.back();
        if (last.command != static_cast<uint16_t>(NavCommand::NavLand) &&
            last.command != static_cast<uint16_t>(NavCommand::NavReturnToLaunch)) {
            add_diagnostic(
                report,
                DiagnosticSeverity::Warning,
                "mission.last_item",
                "Last mission item should be land or return-to-launch",
                last.id,
                "command");
        }
    }

    return report;
}

MissionCompileResult MissionDocumentService::compile_document(
    const MissionDocument& document) {
    MissionCompileResult result;
    result.report = validate_document(document);
    if (result.report.has_errors()) {
        return result;
    }

    for (const MissionEntity& entity : document.entities) {
        if (!entity.enabled) {
            continue;
        }
        MissionItem item{};
        item.seq = static_cast<uint16_t>(result.plan.items.size());
        item.frame = entity.frame;
        item.command = entity.command;
        item.autocontinue = entity.autocontinue;
        item.param1 = entity.params[0];
        item.param2 = entity.params[1];
        item.param3 = entity.params[2];
        item.param4 = entity.params[3];
        if (std::isfinite(entity.latitude)) {
            item.x = static_cast<int32_t>(std::llround(entity.latitude * 1e7));
        }
        if (std::isfinite(entity.longitude)) {
            item.y = static_cast<int32_t>(std::llround(entity.longitude * 1e7));
        }
        item.z = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        item.mission_type = 0;
        result.plan.items.push_back(item);
        result.source_entity_ids.push_back(entity.id);
    }

    result.plan.valid = !result.plan.items.empty();
    result.valid = result.plan.valid;
    return result;
}

MissionEstimate MissionDocumentService::estimate_document(
    const MissionDocument& document) {
    MissionEstimate estimate;
    bool has_previous = false;
    double previous_lat = 0.0;
    double previous_lon = 0.0;
    double current_speed = document.defaults.speed_ms;
    double photo_trigger_distance = 0.0;

    for (const MissionEntity& entity : document.entities) {
        if (!entity.enabled) {
            continue;
        }
        ++estimate.enabled_entity_count;
        if (entity.command == static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            ++estimate.waypoint_count;
        } else {
            ++estimate.command_count;
        }

        const float altitude = std::isfinite(entity.altitude_m)
            ? entity.altitude_m
            : document.defaults.altitude_m;
        if (std::isfinite(altitude) && altitude >= 0.0f) {
            if (estimate.min_altitude_m == 0.0 ||
                altitude < estimate.min_altitude_m) {
                estimate.min_altitude_m = altitude;
            }
            estimate.max_altitude_m =
                std::max(estimate.max_altitude_m, static_cast<double>(altitude));
        }

        if (position_command(entity.command) &&
            finite(entity.latitude) &&
            finite(entity.longitude)) {
            if (has_previous) {
                const double segment_distance = MissionManager::haversine_distance(
                    previous_lat,
                    previous_lon,
                    entity.latitude,
                    entity.longitude);
                estimate.total_distance_m += segment_distance;
                if (current_speed > 0.0) {
                    estimate.estimated_time_s +=
                        segment_distance / current_speed;
                }
                if (photo_trigger_distance > 0.0) {
                    estimate.photo_count += static_cast<int>(
                        std::floor(segment_distance / photo_trigger_distance));
                }
            }
            previous_lat = entity.latitude;
            previous_lon = entity.longitude;
            has_previous = true;
        }

        if (entity.command == static_cast<uint16_t>(NavCommand::DoSetSpeed) &&
            entity.params[1] > 0.0f &&
            (entity.params[0] == 0.0f || entity.params[0] == 1.0f)) {
            current_speed = entity.params[1];
        }
        if (entity.command ==
                static_cast<uint16_t>(NavCommand::DoSetCamTrigDist) &&
            entity.params[0] > 0.0f) {
            photo_trigger_distance = entity.params[0];
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavWaypoint)) {
            estimate.estimated_time_s += std::max(0.0f, entity.params[0]);
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTime)) {
            estimate.estimated_time_s += std::max(0.0f, entity.params[0]);
        }
        if (entity.command == static_cast<uint16_t>(NavCommand::NavLoiterTurns)) {
            estimate.estimated_time_s +=
                std::max(0.0f, entity.params[0]) * 10.0;
        }
    }

    return estimate;
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

    cJSON_AddNumberToObject(root, "revision", document.revision);
    cJSON_AddNumberToObject(root, "updated_at_ms", document.updated_at_ms);

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

    const uint64_t revision = get_u64(root, "revision", 1);
    if (revision > kMaxJsonSafeInteger) {
        cJSON_Delete(root);
        error = "mission document revision exceeds the JSON safe integer range";
        return false;
    }
    parsed.revision = std::max<uint64_t>(1, revision);
    parsed.updated_at_ms = static_cast<int64_t>(
        get_double(root, "updated_at_ms", static_cast<double>(now_ms())));

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

} // namespace mg::core::mission::cad
