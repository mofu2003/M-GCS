#define MGCORE_EXPORTS

#include "ffi_internal.h"
#include "mission_json_serializers.h"

#include "core/mission/cad/mission_document.h"
#include "core/mission/cad/mission_interchange.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace {

using mg::core::mission::cad::DiagnosticSeverity;
using mg::core::mission::cad::MissionDiagnostic;
using mg::core::mission::cad::MissionDocument;
using mg::core::mission::cad::MissionDocumentService;
using mg::core::mission::cad::MissionEntity;
using mg::core::mission::cad::MissionOperationResult;

std::shared_ptr<MissionDocumentService> ensure_document(mg_core_t* core) {
    mg::core::platform::LockGuard lock(core->mission_mtx);
    if (!core->mission_document) {
        core->mission_document = std::make_shared<MissionDocumentService>();
    }
    return core->mission_document;
}

std::string get_string(cJSON* object, const char* key, const std::string& fallback = {}) {
    cJSON* item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    return item && cJSON_IsString(item) && item->valuestring
        ? std::string(item->valuestring)
        : fallback;
}

uint64_t get_uint64(cJSON* object, const char* key) {
    cJSON* item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    if (!item || !cJSON_IsNumber(item) || item->valuedouble < 0) return 0;
    return static_cast<uint64_t>(item->valuedouble);
}

int get_int(cJSON* object, const char* key, int fallback = 0) {
    cJSON* item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
    return item && cJSON_IsNumber(item) ? item->valueint : fallback;
}

std::string severity_name(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Info: return "info";
        case DiagnosticSeverity::Warning: return "warning";
        case DiagnosticSeverity::Error: return "error";
    }
    return "error";
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
    const std::vector<std::string>& warnings = {}) {
    const auto document = service->snapshot();
    const std::string document_json =
        mg::core::mission::cad::serialize_mission_document(document);

    mg::bridge::JsonBuilder jb;
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

bool parse_entity_json(
    cJSON* entity_json,
    MissionEntity& entity,
    std::string& error) {
    if (!entity_json || !cJSON_IsObject(entity_json)) {
        error = "entity must be an object";
        return false;
    }
    char* entity_text = cJSON_PrintUnformatted(entity_json);
    if (!entity_text) {
        error = "failed to serialize entity JSON";
        return false;
    }
    const std::string wrapper =
        std::string("{\"schema_version\":1,\"entities\":[") +
        entity_text + "]}";
    cJSON_free(entity_text);

    MissionDocument document;
    if (!mg::core::mission::cad::deserialize_mission_document(
            wrapper,
            document,
            error)) {
        return false;
    }
    if (document.entities.size() != 1) {
        error = "entity JSON did not produce exactly one entity";
        return false;
    }
    entity = std::move(document.entities.front());
    return true;
}

bool parse_document_json(
    cJSON* document_json,
    MissionDocument& document,
    std::string& error) {
    if (!document_json || !cJSON_IsObject(document_json)) {
        error = "document must be an object";
        return false;
    }
    char* document_text = cJSON_PrintUnformatted(document_json);
    if (!document_text) {
        error = "failed to serialize document JSON";
        return false;
    }
    const bool parsed = mg::core::mission::cad::deserialize_mission_document(
        document_text,
        document,
        error);
    cJSON_free(document_text);
    return parsed;
}

char* operation_result_json(const MissionOperationResult& result) {
    if (!result.success) {
        mg_error_t error = MG_ERROR_INVALID_PARAM;
        switch (result.error_code) {
            case mg::core::mission::cad::MissionOperationError::InvalidState:
                error = MG_ERROR_INVALID_STATE;
                break;
            case mg::core::mission::cad::MissionOperationError::NotFound:
                error = MG_ERROR_NOT_FOUND;
                break;
            case mg::core::mission::cad::MissionOperationError::Conflict:
                error = MG_ERROR_INVALID_STATE;
                break;
            case mg::core::mission::cad::MissionOperationError::Internal:
                error = MG_ERROR_INTERNAL;
                break;
            case mg::core::mission::cad::MissionOperationError::InvalidParam:
            case mg::core::mission::cad::MissionOperationError::None:
                break;
        }
        mg_set_last_error(error);
        mg::bridge::JsonBuilder failure;
        failure.begin_object();
        failure.field("success", false);
        failure.field("error", result.error);
        failure.field("code", static_cast<int>(error));
        failure.field(
            "revision",
            static_cast<unsigned long long>(result.revision));
        failure.end_object();
        return to_cstr(failure.str());
    }

    const std::string document_json =
        mg::core::mission::cad::serialize_mission_document(result.document);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("revision", static_cast<unsigned long long>(result.revision));
    jb.raw_field("document", document_json);
    jb.end_object();
    return to_cstr(jb.str());
}

std::string diagnostics_array_json(
    const mg::core::mission::cad::MissionValidationReport& report);

char* validation_report_json(
    const mg::core::mission::cad::MissionValidationReport& report) {
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("valid", report.valid);

    jb.raw_field("diagnostics", diagnostics_array_json(report));
    jb.end_object();
    return to_cstr(jb.str());
}

char* validation_failure_json(
    const char* error,
    const mg::core::mission::cad::MissionValidationReport& report) {
    mg_set_last_error(MG_ERROR_INVALID_PARAM);
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", false);
    jb.field("error", error ? error : "mission validation failed");
    jb.field("code", static_cast<int>(MG_ERROR_INVALID_PARAM));
    jb.field("valid", false);
    jb.raw_field("diagnostics", diagnostics_array_json(report));
    jb.end_object();
    return to_cstr(jb.str());
}

std::string diagnostics_array_json(
    const mg::core::mission::cad::MissionValidationReport& report) {
    mg::bridge::JsonBuilder diagnostics;
    diagnostics.begin_array();
    for (const MissionDiagnostic& diagnostic : report.diagnostics) {
        mg::bridge::JsonBuilder item;
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

MGCORE_API char* mg_mission_doc_create(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);

    MissionDocument document;
    if (json_config && json_config[0] != '\0') {
        std::string error;
        if (!mg::core::mission::cad::deserialize_mission_document(
                json_config,
                document,
                error)) {
            return fail(MG_ERROR_INVALID_PARAM, error.c_str());
        }
    } else {
        document.metadata.name = "Untitled Mission";
    }
    if (document.metadata.name.empty()) {
        document.metadata.name = "Untitled Mission";
    }

    auto service = std::make_shared<MissionDocumentService>(std::move(document));
    {
        mg::core::platform::LockGuard lock(core->mission_mtx);
        core->mission_document = service;
    }
    return to_cstr(document_snapshot_json(service, true));
}

MGCORE_API char* mg_mission_doc_get(mg_core_t* core) {
    CHECK_CORE(core);
    const auto service = ensure_document(core);
    return to_cstr(document_snapshot_json(service, true));
}

MGCORE_API char* mg_mission_doc_apply(mg_core_t* core, const char* json_command) {
    CHECK_CORE(core);
    if (!json_command || json_command[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty mission document command");
    }

    cJSON* root = cJSON_Parse(json_command);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "invalid mission document command");
    }

    const auto service = ensure_document(core);
    const std::string operation = get_string(root, "operation");
    const uint64_t expected_revision = get_uint64(root, "expected_revision");
    MissionOperationResult result;

    if (operation == "add" || operation == "update") {
        MissionEntity entity;
        std::string error;
        if (!parse_entity_json(
                cJSON_GetObjectItemCaseSensitive(root, "entity"),
                entity,
                error)) {
            cJSON_Delete(root);
            return fail(MG_ERROR_INVALID_PARAM, error.c_str());
        }
        result = operation == "add"
            ? service->add_entity(std::move(entity), expected_revision)
            : service->update_entity(std::move(entity), expected_revision);
    } else if (operation == "remove") {
        result = service->remove_entity(
            get_string(root, "entity_id"),
            expected_revision);
    } else if (operation == "move") {
        result = service->move_entity(
            get_string(root, "entity_id"),
            static_cast<uint32_t>(std::max(0, get_int(root, "to_index"))),
            expected_revision);
    } else if (operation == "clear") {
        result = service->clear(expected_revision);
    } else if (operation == "replace") {
        MissionDocument document;
        std::string error;
        if (!parse_document_json(
                cJSON_GetObjectItemCaseSensitive(root, "document"),
                document,
                error)) {
            cJSON_Delete(root);
            return fail(MG_ERROR_INVALID_PARAM, error.c_str());
        }
        result = service->replace_document(std::move(document), expected_revision);
    } else {
        cJSON_Delete(root);
        return fail(MG_ERROR_INVALID_PARAM, "unknown mission document operation");
    }

    cJSON_Delete(root);
    return operation_result_json(result);
}

MGCORE_API char* mg_mission_doc_undo(mg_core_t* core) {
    CHECK_CORE(core);
    return operation_result_json(ensure_document(core)->undo());
}

MGCORE_API char* mg_mission_doc_redo(mg_core_t* core) {
    CHECK_CORE(core);
    return operation_result_json(ensure_document(core)->redo());
}

MGCORE_API char* mg_mission_doc_validate(mg_core_t* core) {
    CHECK_CORE(core);
    return validation_report_json(ensure_document(core)->validate());
}

MGCORE_API char* mg_mission_doc_compile(mg_core_t* core) {
    CHECK_CORE(core);
    const auto result = ensure_document(core)->compile();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("valid", result.valid);
    jb.field(
        "document_revision",
        static_cast<unsigned long long>(result.document_revision));
    jb.raw_field("plan", mg::bridge::mission_plan_json(result.plan));

    mg::bridge::JsonBuilder ids;
    ids.begin_array();
    for (const auto& entity_id : result.source_entity_ids) {
        mg::bridge::JsonBuilder id;
        id.begin_object();
        id.field("id", entity_id);
        id.end_object();
        ids.append_json(id.str());
    }
    ids.end_array();
    jb.raw_field("source_entity_ids", ids.str());

    jb.raw_field("diagnostics", diagnostics_array_json(result.report));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_upload(
    mg_core_t* core,
    unsigned long long expected_revision) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    const auto service = ensure_document(core);
    if (expected_revision == 0) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            "expected_revision is required for mission upload");
    }

    std::string revision_error;
    const auto compiled = service->compile_at_revision(
        expected_revision,
        revision_error);
    if (!revision_error.empty()) {
        mg_set_last_error(MG_ERROR_INVALID_STATE);
        mg::bridge::JsonBuilder failure;
        failure.begin_object();
        failure.field("success", false);
        failure.field("error", revision_error);
        failure.field("code", static_cast<int>(MG_ERROR_INVALID_STATE));
        failure.field(
            "revision",
            static_cast<unsigned long long>(compiled.document_revision));
        failure.end_object();
        return to_cstr(failure.str());
    }
    if (!compiled.valid || compiled.plan.items.empty()) {
        return validation_failure_json(
            "mission document cannot be uploaded because validation failed",
            compiled.report);
    }

    auto plan = ffi_prepare_mission_upload(core, compiled.plan);
    mg::core::platform::LockGuard lock(core->mission_mtx);
    auto* mavlink = get_active_mavlink(core);
    if (!mavlink ||
        !mavlink->mission_upload(
            active_target_sys(core),
            active_target_comp(core),
            plan)) {
        return fail(MG_ERROR_SEND_FAILED, "mission upload failed to start");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field(
        "revision",
        static_cast<unsigned long long>(compiled.document_revision));
    jb.field("state", "uploading");
    jb.field(
        "document_item_count",
        static_cast<int>(compiled.plan.items.size()));
    jb.field("transport_item_count", static_cast<int>(plan.items.size()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_import_from_vehicle(mg_core_t* core) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    auto* mavlink = get_active_mavlink(core);
    if (!mavlink) {
        return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle connected");
    }
    const auto snapshot = mavlink->mission_get_plan_snapshot();
    const auto provenance = snapshot.provenance;
    if (provenance.last_completed_download_revision == 0 ||
        provenance.last_completed_download_revision !=
            provenance.plan_revision) {
        mg_set_last_error(MG_ERROR_INVALID_STATE);
        mg::bridge::JsonBuilder error;
        error.begin_object();
        error.field("success", false);
        error.field(
            "error",
            "vehicle mission must be downloaded successfully before importing");
        error.field("code", static_cast<int>(MG_ERROR_INVALID_STATE));
        error.field(
            "plan_revision",
            static_cast<unsigned long long>(provenance.plan_revision));
        error.end_object();
        return to_cstr(error.str());
    }

    // 只替换实体，保留 metadata / defaults / home 与 revision 语义。
    MissionDocument document = ensure_document(core)->snapshot();
    std::string error;
    if (!mg::core::mission::cad::import_mission_plan_into_document(
            snapshot.plan,
            document,
            error)) {
        return fail(MG_ERROR_INVALID_STATE, error.c_str());
    }

    auto service = std::make_shared<MissionDocumentService>(std::move(document));
    {
        mg::core::platform::LockGuard lock(core->mission_mtx);
        core->mission_document = service;
    }
    return to_cstr(document_snapshot_json(service, true));
}

MGCORE_API char* mg_mission_doc_verify(mg_core_t* core, double tolerance_m) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    const auto compiled = ensure_document(core)->compile();
    if (!compiled.valid || compiled.plan.items.empty()) {
        return validation_failure_json(
            "mission document cannot be verified because validation failed",
            compiled.report);
    }

    auto* mavlink = get_active_mavlink(core);
    if (!mavlink) {
        return fail(MG_ERROR_NOT_CONNECTED, "no active vehicle connected");
    }
    const auto snapshot = mavlink->mission_get_plan_snapshot();
    const auto provenance = snapshot.provenance;
    if (provenance.last_completed_download_revision == 0 ||
        provenance.last_completed_download_revision !=
            provenance.plan_revision) {
        mg_set_last_error(MG_ERROR_INVALID_STATE);
        mg::bridge::JsonBuilder error;
        error.begin_object();
        error.field("success", false);
        error.field(
            "error",
            "vehicle mission must be downloaded successfully before verification");
        error.field("code", static_cast<int>(MG_ERROR_INVALID_STATE));
        error.field(
            "plan_revision",
            static_cast<unsigned long long>(provenance.plan_revision));
        error.field(
            "last_completed_download_revision",
            static_cast<unsigned long long>(
                provenance.last_completed_download_revision));
        error.end_object();
        return to_cstr(error.str());
    }
    const auto vehicle_plan =
        ffi_present_mission_plan(core, snapshot.plan);
    const double tolerance = std::max(0.0, tolerance_m);

    mg::bridge::JsonBuilder differences;
    differences.begin_array();
    int difference_count = 0;
    auto add_difference = [&](
                              int index,
                              const char* field,
                              const std::string& expected,
                              const std::string& actual) {
        ++difference_count;
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("index", index);
        item.field("field", field);
        item.field("expected", expected);
        item.field("actual", actual);
        item.end_object();
        differences.append_json(item.str());
    };

    const int count = static_cast<int>(compiled.plan.items.size());
    const int vehicle_count = static_cast<int>(vehicle_plan.items.size());
    if (count != vehicle_count) {
        add_difference(
            -1,
            "count",
            std::to_string(count),
            std::to_string(vehicle_count));
    }

    const int compare_count = std::min(count, vehicle_count);
    for (int index = 0; index < compare_count; ++index) {
        const auto& expected = compiled.plan.items[static_cast<size_t>(index)];
        const auto& actual = vehicle_plan.items[static_cast<size_t>(index)];
        if (expected.command != actual.command) {
            add_difference(
                index,
                "command",
                std::to_string(expected.command),
                std::to_string(actual.command));
        }
        if (expected.frame != actual.frame) {
            add_difference(
                index,
                "frame",
                std::to_string(expected.frame),
                std::to_string(actual.frame));
        }
        if (expected.autocontinue != actual.autocontinue) {
            add_difference(
                index,
                "autocontinue",
                std::to_string(expected.autocontinue),
                std::to_string(actual.autocontinue));
        }
        if (expected.mission_type != actual.mission_type) {
            add_difference(
                index,
                "mission_type",
                std::to_string(expected.mission_type),
                std::to_string(actual.mission_type));
        }
        if (expected.x != actual.x) {
            add_difference(
                index,
                "x",
                std::to_string(expected.x),
                std::to_string(actual.x));
        }
        if (expected.y != actual.y) {
            add_difference(
                index,
                "y",
                std::to_string(expected.y),
                std::to_string(actual.y));
        }
        if (std::abs(expected.z - actual.z) > tolerance) {
            add_difference(
                index,
                "z",
                std::to_string(expected.z),
                std::to_string(actual.z));
        }
        for (int param = 0; param < 4; ++param) {
            const float expected_value = param == 0
                ? expected.param1
                : param == 1 ? expected.param2
                : param == 2 ? expected.param3
                             : expected.param4;
            const float actual_value = param == 0
                ? actual.param1
                : param == 1 ? actual.param2
                : param == 2 ? actual.param3
                             : actual.param4;
            if (std::abs(expected_value - actual_value) >
                static_cast<float>(tolerance)) {
                add_difference(
                    index,
                    ("param" + std::to_string(param + 1)).c_str(),
                    std::to_string(expected_value),
                    std::to_string(actual_value));
            }
        }
    }
    differences.end_array();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("matches", difference_count == 0);
    jb.field("difference_count", difference_count);
    jb.field("expected_count", count);
    jb.field("vehicle_count", vehicle_count);
    jb.raw_field("differences", differences.str());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_estimate(mg_core_t* core) {
    CHECK_CORE(core);
    const auto estimate = ensure_document(core)->estimate();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("total_distance_m", estimate.total_distance_m);
    jb.field("estimated_time_s", estimate.estimated_time_s);
    jb.field("min_altitude_m", estimate.min_altitude_m);
    jb.field("max_altitude_m", estimate.max_altitude_m);
    jb.field("photo_count", estimate.photo_count);
    jb.field("waypoint_count", estimate.waypoint_count);
    jb.field("command_count", estimate.command_count);
    jb.field("enabled_entity_count", estimate.enabled_entity_count);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_export(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(mg::core::mission::cad::serialize_mission_document(
        ensure_document(core)->snapshot()));
}

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
        // 导出用编译结果，保证写出的文件就是会上传的那份任务。
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

    // 在现有文档上导入，保留 metadata / defaults / revision。
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
    {
        mg::core::platform::LockGuard lock(core->mission_mtx);
        core->mission_document = service;
    }
    return to_cstr(document_snapshot_json(service, true, warnings));
}

MGCORE_API char* mg_mission_doc_import(mg_core_t* core, const char* json_document) {
    CHECK_CORE(core);
    if (!json_document || json_document[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "empty mission document JSON");
    }
    MissionDocument document;
    std::string error;
    if (!mg::core::mission::cad::deserialize_mission_document(
            json_document,
            document,
            error)) {
        return fail(MG_ERROR_INVALID_PARAM, error.c_str());
    }
    auto service = std::make_shared<MissionDocumentService>(std::move(document));
    {
        mg::core::platform::LockGuard lock(core->mission_mtx);
        core->mission_document = service;
    }
    return to_cstr(document_snapshot_json(service, true));
}

MGCORE_API char* mg_mission_doc_save(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "file path is empty");
    }
    const auto document = ensure_document(core)->snapshot();
    std::string error;
    if (!mg::core::mission::cad::save_mission_document(
            filepath,
            document,
            error)) {
        return fail(MG_ERROR_INTERNAL, error.c_str());
    }
    return ok();
}

MGCORE_API char* mg_mission_doc_load(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "file path is empty");
    }
    if (!std::filesystem::exists(filepath)) {
        return fail(MG_ERROR_NOT_FOUND, "mission document file not found");
    }
    MissionDocument document;
    std::string error;
    if (!mg::core::mission::cad::load_mission_document(
            filepath,
            document,
            error)) {
        return fail(MG_ERROR_INVALID_PARAM, error.c_str());
    }
    auto service = std::make_shared<MissionDocumentService>(std::move(document));
    {
        mg::core::platform::LockGuard lock(core->mission_mtx);
        core->mission_document = service;
    }
    return to_cstr(document_snapshot_json(service, true));
}
