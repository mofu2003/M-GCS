#pragma once

#include "ffi_internal.h"

#include "core/mission/cad/mission_document.h"

#include <memory>
#include <string>
#include <vector>

namespace mg::bridge::mission_detail {

std::shared_ptr<mg::core::mission::cad::MissionDocumentService> ensure_document(
    mg_core_t* core);

void store_document(mg_core_t* core,
    const std::shared_ptr<mg::core::mission::cad::MissionDocumentService>& service);

std::string string_array_json(const std::vector<std::string>& values);

std::string document_snapshot_json(
    const std::shared_ptr<mg::core::mission::cad::MissionDocumentService>& service,
    bool include_operation_state,
    const std::vector<std::string>& warnings = {});

char* validation_failure_json(
    const char* error,
    const mg::core::mission::cad::MissionValidationReport& report);

} // namespace mg::bridge::mission_detail
