#pragma once

#include "core/mission/cad/mission_document.h"

namespace mg::core::mission::cad::detail {

// Survey功能已删除的临时存根
struct SurveyResult {
    bool success{false};
    bool valid{false};
    std::string error;
    std::vector<MissionEntity> entities;
    std::vector<MissionEntity> items;
    // Estimate需要的字段
    int waypoint_count{0};
    double total_distance_m{0.0};
    float estimated_time_s{0.0f};
    int photo_count{0};
};

void add_diagnostic(
    MissionValidationReport& report,
    DiagnosticSeverity severity,
    std::string code,
    std::string message,
    std::string entity_id = {},
    std::string field = {});

/// Shared numeric predicate used by validation and estimation.
bool finite(double value);

/// MAVLink commands whose latitude / longitude fields carry a position.
bool position_command(uint16_t command);

/// Validate a document without touching the service history or revision.
MissionValidationReport validate_document(const MissionDocument& document);

/// Expand a parameterized survey/corridor entity through the shared calculator.
SurveyResult expand_survey_entity(const MissionEntity& entity);

/// A compiled entity plus the document item it came from (entity id or complex
/// item id), so callers can report provenance for expanded complex items.
struct ExpandedEntity {
    MissionEntity entity;
    std::string source_id;
};

/// Entities in plan order with QGC ComplexItem geometry expanded in place of the
/// preserved complex entry (using the stored `TransectStyleComplexItem.Items`).
/// Returns false with `error` when a complex item cannot be expanded.
bool expand_document_entities(
    const MissionDocument& document,
    std::vector<ExpandedEntity>& entities,
    std::string& error);

MissionCompileResult compile_document_impl(const MissionDocument& document);
MissionEstimate estimate_document_impl(const MissionDocument& document);

} // namespace mg::core::mission::cad::detail
