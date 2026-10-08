#define MGCORE_EXPORTS

#include "ffi_mission_internal.h"

#include "core/mission/cad/mission_interchange.h"

#include <memory>
#include <string>
#include <vector>

namespace {

using mg::core::mission::cad::MissionDocument;
using mg::core::mission::cad::MissionDocumentService;
using mg::bridge::mission_detail::document_snapshot_json;
using mg::bridge::mission_detail::ensure_document;
using mg::bridge::mission_detail::string_array_json;
using mg::bridge::mission_detail::validation_failure_json;

} // namespace

MGCORE_API char* mg_mission_doc_export_format(mg_core_t* core, const char* format) {
    CHECK_CORE(core);
    if (!format || format[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty interchange format");
    }
    if (!mg::core::mission::cad::is_supported_interchange_format(format)) {
        return fail(MG_ERROR_INVALID_PARAM,
                    "unsupported interchange format (supported: waypoints, plan)");
    }

    const auto service = ensure_document(core);
    const MissionDocument document = service->snapshot();
    const std::string format_name(format);
    const bool qgc_plan =
        format_name == "plan" || format_name == "qgc_plan" || format_name == "qgc.plan";
    std::string error;
    std::vector<std::string> warnings;
    std::string text;
    int item_count = 0;
    if (qgc_plan) {
        const auto validation = service->validate();
        if (validation.has_errors()) {
            return validation_failure_json(
                "mission document cannot be exported because validation failed",
                validation);
        }
        text = mg::core::mission::cad::export_qgc_plan_json(
            document, warnings, error);
        item_count = static_cast<int>(document.entities.size());
    } else {
        // Export the compiled plan so the file is exactly what would be uploaded.
        const auto compiled = service->compile();
        if (compiled.report.has_errors()) {
            return validation_failure_json(
                "mission document cannot be exported because validation failed",
                compiled.report);
        }
        text = mg::core::mission::cad::export_waypoints_text(
            document, compiled.plan, error);
        item_count = static_cast<int>(compiled.plan.items.size());
    }
    if (!error.empty()) {
        return fail(MG_ERROR_INTERNAL, error.c_str());
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("format", qgc_plan ? "plan" : "waypoints");
    jb.field("item_count", item_count);
    jb.field("text", text);
    jb.raw_field("warnings", string_array_json(warnings));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_import_format(
    mg_core_t* core,
    const char* format,
    const char* text) {
    CHECK_CORE(core);
    if (!format || format[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty interchange format");
    }
    if (!mg::core::mission::cad::is_supported_interchange_format(format)) {
        return fail(MG_ERROR_INVALID_PARAM,
                    "unsupported interchange format (supported: waypoints, plan)");
    }
    if (!text || text[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty mission file text");
    }

    // Import on top of the current document so metadata/defaults/revision survive.
    MissionDocument document = ensure_document(core)->snapshot();
    std::vector<std::string> warnings;
    std::string error;
    const std::string format_name(format);
    const bool qgc_plan =
        format_name == "plan" || format_name == "qgc_plan" || format_name == "qgc.plan";
    const bool imported = qgc_plan
        ? mg::core::mission::cad::import_qgc_plan_json(
              text, document, warnings, error)
        : mg::core::mission::cad::import_waypoints_text(text, document, error);
    if (!imported) {
        return fail(MG_ERROR_INVALID_PARAM, error.c_str());
    }

    auto service = std::make_shared<MissionDocumentService>(std::move(document));
    mg::bridge::mission_detail::store_document(core, service);
    return to_cstr(document_snapshot_json(service, true, warnings));
}
