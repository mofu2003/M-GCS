#pragma once

#include "core/mission/mission_types.h"
#include "core/platform/platform_mutex.h"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace mg::core::mission::cad {

constexpr int kMissionDocumentSchemaVersion = 1;

enum class DiagnosticSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2,
};

struct MissionDiagnostic {
    std::string code;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string entity_id;
    std::string field;
};

struct MissionDocumentMetadata {
    std::string id;
    std::string name;
    std::string description;
    std::string firmware_profile = "ardupilot";
    std::string vehicle_type = "copter";
};

struct MissionHome {
    bool set = false;
    double latitude = std::numeric_limits<double>::quiet_NaN();
    double longitude = std::numeric_limits<double>::quiet_NaN();
    float altitude_m = 0.0f;
};

struct MissionDefaults {
    uint8_t frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    float altitude_m = 50.0f;
    float speed_ms = 12.0f;
};

struct MissionEntity {
    std::string id;
    std::string type = "waypoint";
    std::string label;
    bool enabled = true;
    uint32_t order = 0;
    uint16_t command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    uint8_t frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    uint8_t autocontinue = 1;
    double latitude = std::numeric_limits<double>::quiet_NaN();
    double longitude = std::numeric_limits<double>::quiet_NaN();
    float altitude_m = std::numeric_limits<float>::quiet_NaN();
    std::array<float, 4> params = {0.0f, 0.0f, 0.0f, 0.0f};
    std::string notes;
};

struct MissionDocument {
    int schema_version = kMissionDocumentSchemaVersion;
    MissionDocumentMetadata metadata;
    MissionHome home;
    MissionDefaults defaults;
    uint64_t revision = 1;
    int64_t updated_at_ms = 0;
    std::vector<MissionEntity> entities;
};

struct MissionValidationReport {
    bool valid = true;
    std::vector<MissionDiagnostic> diagnostics;

    bool has_errors() const;
};

struct MissionEstimate {
    double total_distance_m = 0.0;
    double estimated_time_s = 0.0;
    double min_altitude_m = 0.0;
    double max_altitude_m = 0.0;
    int photo_count = 0;
    int waypoint_count = 0;
    int command_count = 0;
    int enabled_entity_count = 0;
};

struct MissionCompileResult {
    bool valid = false;
    uint64_t document_revision = 0;
    MissionPlan plan;
    std::vector<std::string> source_entity_ids;
    MissionValidationReport report;
};

enum class MissionOperationError : uint8_t {
    None = 0,
    InvalidParam = 1,
    InvalidState = 2,
    NotFound = 3,
    Conflict = 4,
    Internal = 5,
};

struct MissionOperationResult {
    bool success = false;
    MissionOperationError error_code = MissionOperationError::None;
    std::string error;
    uint64_t revision = 0;
    MissionDocument document;
};

class MissionDocumentService {
public:
    MissionDocumentService();
    explicit MissionDocumentService(MissionDocument document);

    MissionDocument snapshot() const;
    uint64_t revision() const;
    bool can_undo() const;
    bool can_redo() const;

    MissionOperationResult replace_document(
        MissionDocument document,
        uint64_t expected_revision = 0);
    MissionOperationResult add_entity(
        MissionEntity entity,
        uint64_t expected_revision = 0);
    MissionOperationResult update_entity(
        MissionEntity entity,
        uint64_t expected_revision = 0);
    MissionOperationResult remove_entity(
        const std::string& entity_id,
        uint64_t expected_revision = 0);
    MissionOperationResult move_entity(
        const std::string& entity_id,
        uint32_t to_index,
        uint64_t expected_revision = 0);
    MissionOperationResult clear(uint64_t expected_revision = 0);

    MissionOperationResult undo();
    MissionOperationResult redo();

    MissionValidationReport validate() const;
    MissionCompileResult compile() const;
    MissionCompileResult compile_at_revision(
        uint64_t expected_revision,
        std::string& error) const;
    MissionEstimate estimate() const;

    static MissionValidationReport validate_document(
        const MissionDocument& document);
    static MissionCompileResult compile_document(
        const MissionDocument& document);
    static MissionEstimate estimate_document(
        const MissionDocument& document);

private:
    mutable mg::core::platform::Mutex mtx_;
    MissionDocument document_;
    std::vector<MissionDocument> undo_stack_;
    std::vector<MissionDocument> redo_stack_;
    uint64_t next_entity_id_ = 1;
    uint64_t next_revision_ = 2;

    static constexpr size_t kMaxHistory = 100;

    MissionOperationResult commit_locked(
        MissionDocument before,
        std::string error = {});
    void normalize_locked();
    bool check_revision_locked(uint64_t expected, std::string& error) const;
    std::string next_entity_id_locked();
};

std::string serialize_mission_document(const MissionDocument& document);
bool deserialize_mission_document(
    const std::string& json,
    MissionDocument& document,
    std::string& error);

bool save_mission_document(
    const std::string& path,
    const MissionDocument& document,
    std::string& error);
bool load_mission_document(
    const std::string& path,
    MissionDocument& document,
    std::string& error);

const char* mission_entity_type_name(const std::string& type);

} // namespace mg::core::mission::cad
