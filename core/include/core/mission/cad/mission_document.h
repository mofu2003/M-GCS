#pragma once

#include "core/mission/mission_types.h"
#include "core/platform/platform_mutex.h"

#include <array>
#include <cstdint>
#include <limits>
#include <map>
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

/// 参数化测绘实体的几何参数，只在 type 为 `survey_grid` / `corridor` 时有意义。
///
/// 这些实体在编译期由 `SurveyCalculator` 展开成具体的航点，因此文档里存的
/// 是"设计意图"而不是展开后的几十个点。字段名与前端 / docs 保持一致。
struct MissionSurveySpec {
    // 网格（survey_grid）
    double width_m = 100.0;
    double length_m = 100.0;
    double angle_deg = 0.0;
    bool cross_grid = false;

    // 走廊（corridor）：entity.lat/lon 是起点，这里是终点
    double end_latitude = std::numeric_limits<double>::quiet_NaN();
    double end_longitude = std::numeric_limits<double>::quiet_NaN();
    double corridor_width_m = 50.0;

    // 通用
    float altitude_m = 50.0f;
    float speed_ms = 5.0f;
    bool return_to_start = true;

    // 相机与重叠率，缺省与 CameraSpec 默认值一致
    float forward_overlap_pct = 70.0f;
    float side_overlap_pct = 60.0f;
    float focal_length_mm = 35.0f;
    float sensor_width_mm = 23.5f;
    float sensor_height_mm = 15.6f;
    float image_width_px = 6000.0f;
    float image_height_px = 4000.0f;
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
    MissionSurveySpec survey;
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
    /// Format-specific JSON sections preserved verbatim for lossless round-trips.
    std::map<std::string, std::string> extensions;
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

/// 该实体类型是否由 SurveyCalculator 在编译期展开（而不是 1 实体 1 任务项）。
bool is_parameterized_survey_type(const std::string& type);

/// 由 MAV_CMD 反推实体类型（waypoint / takeoff / land / rtl / loiter）。
/// 导入外部任务（`.waypoints` 文件、飞控回读）时统一用它，避免两处映射不一致。
const char* mission_entity_type_for_command(uint16_t command);

/// 把 `_INT` 系列坐标帧归一成文档使用的普通帧。
///
/// 飞控回读的任务普遍用 `MAV_FRAME_GLOBAL_*_INT`（5/6/11），因为整数经纬度；
/// 文档用 double 存位置、编译时再转 degE7，两者语义相同，所以 5→0、6→3、
/// 11→10。不归一化的话，导入回来的任务会被自己的校验判成 frame_unsupported。
uint8_t normalize_mission_frame(uint8_t frame);

/// 用一份已编译的 MAVLink 任务替换文档的实体列表（home 与 metadata 保留）。
/// 用于“从飞控读取任务”这类导入：任务项只带 command/frame/params/位置，
/// 参数化测绘等意图无法还原，这是格式本身的限制。
bool import_mission_plan_into_document(
    const MissionPlan& plan,
    MissionDocument& document,
    std::string& error);

} // namespace mg::core::mission::cad
