#define MGCORE_EXPORTS

#include "ffi_mission_internal.h"
#include "ffi_internal.h"
#include "mission_json_serializers.h"

#include "core/mission/cad/mission_document.h"
#include "core/mission/cad/mission_command_catalog.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

namespace {

const mg::core::mission::cad::MissionCommandDescriptor* command_descriptor(
    uint16_t command) {
    const auto& catalog = mg::core::mission::cad::mission_command_catalog();
    const auto it = std::find_if(
        catalog.begin(),
        catalog.end(),
        [command](const auto& descriptor) {
            return descriptor.command == command;
        });
    return it == catalog.end() ? nullptr : &*it;
}

bool command_has_parameter(
    const mg::core::mission::cad::MissionCommandDescriptor* descriptor,
    uint8_t index) {
    if (!descriptor) return true;
    return std::any_of(
        descriptor->parameters.begin(),
        descriptor->parameters.end(),
        [index](const auto& parameter) { return parameter.index == index; });
}

bool mission_float_matches(
    float expected,
    float actual,
    double tolerance,
    bool unset_matches_zero = false) {
    const bool expected_finite = std::isfinite(expected);
    const bool actual_finite = std::isfinite(actual);
    if (!expected_finite || !actual_finite) {
        if (!expected_finite && !actual_finite) return true;
        const auto finite_value = expected_finite ? expected : actual;
        return unset_matches_zero && std::abs(finite_value) <= tolerance;
    }
    return std::abs(expected - actual) <= tolerance;
}

using mg::core::mission::cad::DiagnosticSeverity;
using mg::core::mission::cad::MissionDiagnostic;
using mg::core::mission::cad::MissionDocument;
using mg::core::mission::cad::MissionDocumentService;
using mg::core::mission::cad::MissionEntity;
using mg::core::mission::cad::MissionOperationResult;
using mg::bridge::mission_detail::document_snapshot_json;
using mg::bridge::mission_detail::ensure_document;
using mg::bridge::mission_detail::validation_failure_json;

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

    const auto document = service->snapshot();
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

    auto plan = ffi_prepare_mission_upload(core, compiled.plan, &document.home);
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

MGCORE_API char* mg_mission_doc_compile_fence(mg_core_t* core) {
    CHECK_CORE(core);

    const auto document = ensure_document(core)->snapshot();
    mg::core::mission::MissionPlan plan;
    std::string error;
    if (!compile_geo_fence_plan(document.geo_fence, plan, error)) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            error.empty() ? "geo fence cannot be compiled" : error.c_str());
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("item_count", static_cast<int>(plan.items.size()));
    jb.raw_field("plan", mg::bridge::mission_plan_json(plan));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_upload_fence(
    mg_core_t* core,
    unsigned long long expected_revision) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    const auto service = ensure_document(core);
    if (expected_revision == 0) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            "expected_revision is required for geofence upload");
    }
    if (service->revision() != expected_revision) {
        mg_set_last_error(MG_ERROR_INVALID_STATE);
        mg::bridge::JsonBuilder failure;
        failure.begin_object();
        failure.field("success", false);
        failure.field(
            "error",
            "revision mismatch: expected " + std::to_string(expected_revision) +
                ", current " + std::to_string(service->revision()));
        failure.field("code", static_cast<int>(MG_ERROR_INVALID_STATE));
        failure.field(
            "revision",
            static_cast<unsigned long long>(service->revision()));
        failure.end_object();
        return to_cstr(failure.str());
    }

    const auto document = service->snapshot();
    mg::core::mission::MissionPlan plan;
    std::string error;
    if (!compile_geo_fence_plan(document.geo_fence, plan, error)) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            error.empty() ? "geo fence cannot be compiled" : error.c_str());
    }

    mg::core::platform::LockGuard lock(core->mission_mtx);
    auto* mavlink = get_active_mavlink(core);
    if (!mavlink ||
        !mavlink->fence_upload(
            active_target_sys(core),
            active_target_comp(core),
            plan)) {
        return fail(MG_ERROR_SEND_FAILED, "geofence upload failed to start");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field(
        "revision",
        static_cast<unsigned long long>(expected_revision));
    jb.field("state", "uploading");
    jb.field("item_count", static_cast<int>(plan.items.size()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_clear_fence(mg_core_t* core) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    mg::core::platform::LockGuard lock(core->mission_mtx);
    auto* mavlink = get_active_mavlink(core);
    if (!mavlink ||
        !mavlink->fence_clear(
            active_target_sys(core),
            active_target_comp(core))) {
        return fail(MG_ERROR_SEND_FAILED, "geofence clear failed to start");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("state", "clearing");
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_compile_rally(mg_core_t* core) {
    CHECK_CORE(core);

    const auto document = ensure_document(core)->snapshot();
    mg::core::mission::MissionPlan plan;
    std::string error;
    if (!compile_rally_plan(document.rally_points, plan, error)) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            error.empty() ? "rally points cannot be compiled" : error.c_str());
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("item_count", static_cast<int>(plan.items.size()));
    jb.raw_field("plan", mg::bridge::mission_plan_json(plan));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_upload_rally(
    mg_core_t* core,
    unsigned long long expected_revision) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    const auto service = ensure_document(core);
    if (expected_revision == 0) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            "expected_revision is required for rally upload");
    }
    if (service->revision() != expected_revision) {
        mg_set_last_error(MG_ERROR_INVALID_STATE);
        mg::bridge::JsonBuilder failure;
        failure.begin_object();
        failure.field("success", false);
        failure.field(
            "error",
            "revision mismatch: expected " + std::to_string(expected_revision) +
                ", current " + std::to_string(service->revision()));
        failure.field("code", static_cast<int>(MG_ERROR_INVALID_STATE));
        failure.field(
            "revision",
            static_cast<unsigned long long>(service->revision()));
        failure.end_object();
        return to_cstr(failure.str());
    }

    const auto document = service->snapshot();
    mg::core::mission::MissionPlan plan;
    std::string error;
    if (!compile_rally_plan(document.rally_points, plan, error)) {
        return fail(
            MG_ERROR_INVALID_PARAM,
            error.empty() ? "rally points cannot be compiled" : error.c_str());
    }

    mg::core::platform::LockGuard lock(core->mission_mtx);
    auto* mavlink = get_active_mavlink(core);
    if (!mavlink ||
        !mavlink->rally_upload(
            active_target_sys(core),
            active_target_comp(core),
            plan)) {
        return fail(MG_ERROR_SEND_FAILED, "rally upload failed to start");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field(
        "revision",
        static_cast<unsigned long long>(expected_revision));
    jb.field("state", "uploading");
    jb.field("item_count", static_cast<int>(plan.items.size()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_mission_doc_clear_rally(mg_core_t* core) {
    CHECK_CORE(core); CHECK_CONNECTED(core);

    mg::core::platform::LockGuard lock(core->mission_mtx);
    auto* mavlink = get_active_mavlink(core);
    if (!mavlink ||
        !mavlink->rally_clear(
            active_target_sys(core),
            active_target_comp(core))) {
        return fail(MG_ERROR_SEND_FAILED, "rally clear failed to start");
    }

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("state", "clearing");
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

    // 只替换实体，保留 metadata / defaults / revision 语义；Home 从原始
    // ArduPilot 第 0 项同步出来，而不是把它当成普通任务项导入。
    MissionDocument document = ensure_document(core)->snapshot();
    const auto presented_plan = ffi_present_mission_plan(
        core,
        snapshot.plan,
        &document.home);
    std::string error;
    if (!mg::core::mission::cad::import_mission_plan_into_document(
            presented_plan,
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

    const auto document = ensure_document(core)->snapshot();
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
    mg::core::mission::cad::MissionHome vehicle_home;
    bool vehicle_home_present = false;
    const auto vehicle_plan = ffi_present_mission_plan(
        core,
        snapshot.plan,
        &vehicle_home,
        &vehicle_home_present);
    const double tolerance = std::max(0.0, tolerance_m);

    mg::bridge::JsonBuilder differences;
    differences.begin_array();
    mg::bridge::JsonBuilder normalizations;
    normalizations.begin_array();
    int difference_count = 0;
    int normalization_count = 0;
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
    auto add_normalization = [&](
                                  int index,
                                  const char* field,
                                  const std::string& expected,
                                  const std::string& actual,
                                  const char* reason) {
        ++normalization_count;
        mg::bridge::JsonBuilder item;
        item.begin_object();
        item.field("index", index);
        item.field("field", field);
        item.field("expected", expected);
        item.field("actual", actual);
        item.field("reason", reason);
        item.end_object();
        normalizations.append_json(item.str());
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

    if (document.home.set && mg::bridge::ffi::mission_uses_home_item(core)) {
        if (!vehicle_home_present || !vehicle_home.set) {
            add_difference(-1, "home", "set", "missing");
        } else {
            const int expected_lat_e7 = static_cast<int>(
                std::llround(document.home.latitude * 1e7));
            const int expected_lon_e7 = static_cast<int>(
                std::llround(document.home.longitude * 1e7));
            const int actual_lat_e7 = static_cast<int>(
                std::llround(vehicle_home.latitude * 1e7));
            const int actual_lon_e7 = static_cast<int>(
                std::llround(vehicle_home.longitude * 1e7));
            if (std::abs(expected_lat_e7 - actual_lat_e7) > 1) {
                add_difference(
                    -1,
                    "home.latitude",
                    std::to_string(document.home.latitude),
                    std::to_string(vehicle_home.latitude));
            }
            if (std::abs(expected_lon_e7 - actual_lon_e7) > 1) {
                add_difference(
                    -1,
                    "home.longitude",
                    std::to_string(document.home.longitude),
                    std::to_string(vehicle_home.longitude));
            }
            if (std::abs(document.home.altitude_m - vehicle_home.altitude_m) >
                static_cast<float>(tolerance)) {
                add_difference(
                    -1,
                    "home.altitude_m",
                    std::to_string(document.home.altitude_m),
                    std::to_string(vehicle_home.altitude_m));
            }
        }
    }

    const int compare_count = std::min(count, vehicle_count);
    for (int index = 0; index < compare_count; ++index) {
        const auto& expected = compiled.plan.items[static_cast<size_t>(index)];
        const auto& actual = vehicle_plan.items[static_cast<size_t>(index)];
        const auto* descriptor = command_descriptor(expected.command);
        if (expected.command != actual.command) {
            add_difference(
                index,
                "command",
                std::to_string(expected.command),
                std::to_string(actual.command));
        }
        if (expected.frame != actual.frame) {
            const auto normalized_actual_frame =
                mg::core::mission::cad::normalize_mission_frame(actual.frame);
            if (expected.frame != normalized_actual_frame) {
                add_difference(
                    index,
                    "frame",
                    std::to_string(expected.frame),
                    std::to_string(normalized_actual_frame));
            } else {
                add_normalization(
                    index,
                    "frame",
                    std::to_string(expected.frame),
                    std::to_string(actual.frame),
                    "等价的 MAVLink _INT 坐标帧");
            }
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
        // Takeoff/RTL and DO/CONDITION commands do not carry a meaningful
        // geographic position. ArduPilot may write zero or a placeholder
        // coordinate back for those items; QGC treats them as non-coordinate
        // mission items as well.
        if (descriptor && descriptor->requires_position && expected.x != actual.x) {
            add_difference(
                index,
                "x",
                std::to_string(expected.x),
                std::to_string(actual.x));
        } else if (descriptor && !descriptor->requires_position && expected.x != actual.x) {
            add_normalization(
                index,
                "x",
                std::to_string(expected.x),
                std::to_string(actual.x),
                "非位置命令的坐标不参与任务执行");
        }
        if (descriptor && descriptor->requires_position && expected.y != actual.y) {
            add_difference(
                index,
                "y",
                std::to_string(expected.y),
                std::to_string(actual.y));
        } else if (descriptor && !descriptor->requires_position && expected.y != actual.y) {
            add_normalization(
                index,
                "y",
                std::to_string(expected.y),
                std::to_string(actual.y),
                "非位置命令的坐标不参与任务执行");
        }
        if (descriptor && descriptor->requires_altitude &&
            !mission_float_matches(expected.z, actual.z, tolerance)) {
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
            if (command_has_parameter(descriptor, static_cast<uint8_t>(param + 1)) &&
                !mission_float_matches(
                    expected_value,
                    actual_value,
                    tolerance,
                    true)) {
                add_difference(
                    index,
                    ("param" + std::to_string(param + 1)).c_str(),
                    std::to_string(expected_value),
                    std::to_string(actual_value));
            } else if (command_has_parameter(
                           descriptor,
                           static_cast<uint8_t>(param + 1)) &&
                       ((!std::isfinite(expected_value) &&
                         std::isfinite(actual_value) &&
                         std::abs(actual_value) <= tolerance) ||
                        (std::isfinite(expected_value) &&
                         !std::isfinite(actual_value) &&
                         std::abs(expected_value) <= tolerance))) {
                add_normalization(
                    index,
                    ("param" + std::to_string(param + 1)).c_str(),
                    std::to_string(expected_value),
                    std::to_string(actual_value),
                    "NaN 与 0 都表示未设置");
            }
        }
    }
    differences.end_array();
    normalizations.end_array();

    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("matches", difference_count == 0);
    jb.field("difference_count", difference_count);
    jb.field("expected_count", count);
    jb.field("vehicle_count", vehicle_count);
    jb.raw_field("differences", differences.str());
    jb.field("normalization_count", normalization_count);
    jb.raw_field("normalizations", normalizations.str());
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
