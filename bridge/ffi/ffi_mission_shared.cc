#define MGCORE_EXPORTS

#include "ffi_mission_internal.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"
#include <set>

namespace mg::bridge::mission_detail {

using mg::core::mission::cad::DiagnosticSeverity;
using mg::core::mission::cad::MissionDiagnostic;
using mg::core::mission::cad::MissionDocumentService;
using mg::core::mission::cad::MissionValidationReport;

namespace {

std::string severity_name(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Info: return "info";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Error: return "error";
    }
    return "error";
}

std::string diagnostics_array_json(const MissionValidationReport& report) {
    mg::core::telemetry::JsonBuilder diagnostics;
    diagnostics.begin_array();
    for (const MissionDiagnostic& diagnostic : report.diagnostics) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("severity", severity_name(diagnostic.severity));
        item.field("code", diagnostic.code);
        item.field("message", diagnostic.message);
        if (!diagnostic.entity_id.empty()) {
            item.field("entity_id", diagnostic.entity_id);
        }
        if (!diagnostic.field.empty()) {
            item.field("field", diagnostic.field);
        }
        item.end_object();
        diagnostics.append_json(item.str());
    }
    diagnostics.end_array();
    return diagnostics.str();
}

} // namespace

std::shared_ptr<MissionDocumentService> ensure_document(mg_core_t* core) {
    const auto* vehicle = get_active_vehicle(core);
    const int id = vehicle ? vehicle->vehicle_id() : -1;
    std::set<int> live_ids;
    for (const auto& live : core->vehicles.get_all_vehicles_shared()) {
        if (live) live_ids.insert(live->vehicle_id());
    }
    mg::core::platform::LockGuard lock(core->mission_mtx);
    for (auto it = core->vehicle_mission_documents.begin();
         it != core->vehicle_mission_documents.end();) {
        if (live_ids.count(it->first) == 0) it = core->vehicle_mission_documents.erase(it);
        else ++it;
    }
    if (id > 0) {
        auto& document = core->vehicle_mission_documents[id];
        if (!document) {
            // Preserve an offline draft for the first connection, but copy it:
            // later edits must never mutate the offline or another vehicle plan.
            if (core->vehicle_mission_documents.size() == 1 && core->mission_document) {
                document = std::make_shared<MissionDocumentService>(core->mission_document->snapshot());
            } else {
                document = std::make_shared<MissionDocumentService>();
            }
        }
        return document;
    }
    if (!core->mission_document) {
        core->mission_document = std::make_shared<MissionDocumentService>();
    }
    return core->mission_document;
}

void store_document(mg_core_t* core,
                    const std::shared_ptr<MissionDocumentService>& service) {
    const auto* vehicle = get_active_vehicle(core);
    const int id = vehicle ? vehicle->vehicle_id() : -1;
    mg::core::platform::LockGuard lock(core->mission_mtx);
    if (id > 0) core->vehicle_mission_documents[id] = service;
    else core->mission_document = service;
}

std::string string_array_json(const std::vector<std::string>& values) {
    cJSON* array = cJSON_CreateArray();
    for (const std::string& value : values) {
        cJSON_AddItemToArray(array, cJSON_CreateString(value.c_str()));
    }
    char* text = cJSON_PrintUnformatted(array);
    cJSON_Delete(array);
    if (!text) return "[]";
    std::string json(text);
    cJSON_free(text);
    return json;
}

std::string document_snapshot_json(
    const std::shared_ptr<MissionDocumentService>& service,
    bool include_operation_state,
    const std::vector<std::string>& warnings) {
    const auto document = service->snapshot();
    const std::string document_json =
        mg::core::mission::cad::serialize_mission_document(document);

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("revision", static_cast<unsigned long long>(document.revision));
    if (include_operation_state) {
        jb.field("can_undo", service->can_undo());
        jb.field("can_redo", service->can_redo());
    }
    jb.raw_field("document", document_json);
    jb.raw_field("warnings", string_array_json(warnings));
    jb.end_object();
    return jb.str();
}

char* validation_failure_json(
    const char* error,
    const MissionValidationReport& report) {
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    write_error(jb, MG_ERROR_INVALID_PARAM,
                error ? error : "mission validation failed");
    jb.field("valid", false);
    jb.raw_field("diagnostics", diagnostics_array_json(report));
    jb.end_object();
    return to_cstr(jb.str());
}

} // namespace mg::bridge::mission_detail
