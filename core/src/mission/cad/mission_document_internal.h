#pragma once

#include "core/mission/cad/mission_document.h"
#include "core/mission/survey_calculator.h"

namespace mg::core::mission::cad::detail {

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

MissionCompileResult compile_document_impl(const MissionDocument& document);
MissionEstimate estimate_document_impl(const MissionDocument& document);

} // namespace mg::core::mission::cad::detail
