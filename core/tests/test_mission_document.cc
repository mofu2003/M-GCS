#define MGCORE_EXPORTS

#include "ffi_api.h"
#include "core/mission/cad/mission_document.h"
#include "core/mission/cad/mission_interchange.h"
#include "core/mission/cad/mission_command_catalog.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

using namespace mg::core::mission;
using namespace mg::core::mission::cad;

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(condition, message) do { \
    if (condition) { \
        ++g_pass; \
        fprintf(stderr, "  [PASS] %s\n", message); \
    } else { \
        ++g_fail; \
        fprintf(stderr, "  [FAIL] %s\n", message); \
    } \
} while (0)

static bool report_has_code(const MissionValidationReport& report,
                            const std::string& code) {
    for (const auto& diagnostic : report.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

static MissionDocument valid_document() {
    MissionDocument document;
    document.metadata.id = "mission-test";
    document.metadata.name = "Test Mission";
    document.home.set = true;
    document.home.latitude = 39.9;
    document.home.longitude = 116.4;
    document.home.altitude_m = 20.0f;
    document.defaults.altitude_m = 50.0f;
    document.defaults.speed_ms = 10.0f;
    document.rally_points.push_back({39.9, 116.4, 30.0f});

    MissionEntity takeoff;
    takeoff.id = "takeoff-1";
    takeoff.type = "takeoff";
    takeoff.command = static_cast<uint16_t>(NavCommand::NavTakeoff);
    takeoff.altitude_m = 30.0f;
    document.entities.push_back(takeoff);

    MissionEntity waypoint;
    waypoint.id = "waypoint-1";
    waypoint.type = "waypoint";
    waypoint.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    waypoint.latitude = 39.9001;
    waypoint.longitude = 116.4001;
    waypoint.altitude_m = 50.0f;
    waypoint.params = {0.0f, 2.0f, 0.0f, 0.0f};
    document.entities.push_back(waypoint);

    MissionEntity land;
    land.id = "land-1";
    land.type = "land";
    land.command = static_cast<uint16_t>(NavCommand::NavLand);
    land.latitude = 39.9002;
    land.longitude = 116.4002;
    land.altitude_m = 0.0f;
    document.entities.push_back(land);
    return document;
}

static void test_document_editing_history() {
    fprintf(stderr, "\n[Group] Mission document editing history\n");
    MissionDocumentService service;

    MissionEntity takeoff;
    takeoff.type = "takeoff";
    takeoff.command = static_cast<uint16_t>(NavCommand::NavTakeoff);
    takeoff.altitude_m = 30.0f;
    auto result = service.add_entity(takeoff);
    CHECK(result.success, "add takeoff succeeds");
    CHECK(result.revision == 2, "add increments revision");
    CHECK(result.document.entities.size() == 1, "document contains takeoff");
    CHECK(!result.document.entities.front().id.empty(), "generated entity id is present");

    MissionEntity waypoint;
    waypoint.type = "waypoint";
    waypoint.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    waypoint.latitude = 39.9;
    waypoint.longitude = 116.4;
    waypoint.altitude_m = 50.0f;
    result = service.add_entity(waypoint, result.revision);
    CHECK(result.success, "add waypoint succeeds");
    CHECK(result.document.entities.size() == 2, "document contains two entities");
    CHECK(result.document.entities[1].order == 1, "entity order is normalized");

    auto waypoint_entity = result.document.entities[1];
    waypoint_entity.altitude_m = 80.0f;
    result = service.update_entity(waypoint_entity, result.revision);
    CHECK(result.success, "update waypoint succeeds");
    CHECK(result.document.entities[1].altitude_m == 80.0f, "update changes altitude");

    result = service.move_entity(result.document.entities[1].id, 0, result.revision);
    CHECK(result.success, "move waypoint succeeds");
    CHECK(result.document.entities[0].type == "waypoint", "move changes entity order");

    result = service.undo();
    CHECK(result.success, "undo succeeds");
    CHECK(result.document.entities[0].type == "takeoff", "undo restores previous order");

    result = service.redo();
    CHECK(result.success, "redo succeeds");
    CHECK(result.document.entities[0].type == "waypoint", "redo restores moved order");

    const uint64_t revision = result.revision;
    result = service.add_entity(waypoint, revision - 1);
    CHECK(!result.success, "stale revision is rejected");

    MissionDocumentService branch;
    auto branch_result = branch.add_entity(takeoff);
    CHECK(branch_result.revision == 2, "branch starts at revision two");
    branch_result = branch.undo();
    CHECK(branch_result.revision == 3, "undo advances revision monotonically");
    branch_result = branch.add_entity(waypoint, branch_result.revision);
    CHECK(branch_result.revision == 4, "edit after undo advances revision");
    branch_result = branch.add_entity(waypoint, 2);
    CHECK(!branch_result.success, "revision from abandoned branch cannot be reused");
}

static void test_document_roundtrip() {
    fprintf(stderr, "\n[Group] Mission document JSON roundtrip\n");
    const MissionDocument original = valid_document();
    const std::string json = serialize_mission_document(original);
    CHECK(json.find("\"schema_version\":1") != std::string::npos, "schema version is serialized");
    CHECK(json.find("\"Test Mission\"") != std::string::npos, "metadata is serialized");

    MissionDocument restored;
    std::string error;
    CHECK(
        deserialize_mission_document(json, restored, error),
        "document JSON deserializes");
    CHECK(error.empty(), "deserialize reports no error");
    CHECK(restored.metadata.id == original.metadata.id, "metadata id roundtrips");
    CHECK(restored.home.set, "home flag roundtrips");
    CHECK(restored.rally_points.size() == 1 &&
              std::fabs(restored.rally_points[0].latitude - 39.9) < 1e-6,
          "rally points roundtrip through document JSON");
    CHECK(restored.entities.size() == original.entities.size(), "entity count roundtrips");
    CHECK(restored.entities[1].latitude == original.entities[1].latitude, "latitude roundtrips");

    const std::string reserialized = serialize_mission_document(restored);
    CHECK(json == reserialized, "serialization is deterministic");

    MissionDocument wide_revision = valid_document();
    wide_revision.revision = 5000000000ULL;
    const std::string wide_json = serialize_mission_document(wide_revision);
    MissionDocument wide_restored;
    CHECK(
        deserialize_mission_document(wide_json, wide_restored, error),
        "64-bit revision deserializes");
    CHECK(
        wide_restored.revision == wide_revision.revision,
        "64-bit revision roundtrips");

    MissionDocument invalid_entity;
    CHECK(
        !deserialize_mission_document(
            R"({"schema_version":1,"entities":[{"id":"bad","command":16,"frame":300}]})",
            invalid_entity,
            error),
        "out-of-range frame is rejected during deserialization");
    CHECK(
        !deserialize_mission_document(
            R"({"schema_version":1,"revision":9007199254740992})",
            invalid_entity,
            error),
        "revision beyond JSON safe integer range is rejected");
}

static void test_validation_compile_and_estimate() {
    fprintf(stderr, "\n[Group] Mission validation, compile and estimate\n");
    MissionDocument document = valid_document();
    auto report = MissionDocumentService::validate_document(document);
    CHECK(report.valid, "valid document passes validation");

    MissionEntity disabled;
    disabled.id = "disabled-1";
    disabled.type = "command";
    disabled.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    disabled.params[1] = 8.0f;
    disabled.enabled = false;
    document.entities.push_back(disabled);

    auto compiled = MissionDocumentService::compile_document(document);
    CHECK(compiled.valid, "valid document compiles");
    CHECK(compiled.plan.items.size() == 3, "disabled entity is excluded from compiled plan");
    CHECK(compiled.source_entity_ids.size() == 3, "source entity ids match compiled plan");
    CHECK(compiled.plan.items[0].command == static_cast<uint16_t>(NavCommand::NavTakeoff), "first command compiles");
    CHECK(compiled.plan.items[0].seq == 0, "compiled sequence starts at zero");

    const auto estimate = MissionDocumentService::estimate_document(document);
    CHECK(estimate.enabled_entity_count == 3, "estimate counts enabled entities");
    CHECK(estimate.total_distance_m > 0.0, "estimate reports distance");
    CHECK(estimate.estimated_time_s > 0.0, "estimate reports time");

    MissionDocument invalid;
    invalid.entities.push_back(MissionEntity{});
    invalid.entities.front().id = "bad-lat";
    invalid.entities.front().command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    invalid.entities.front().latitude = 120.0;
    invalid.entities.front().longitude = 116.0;
    report = MissionDocumentService::validate_document(invalid);
    CHECK(!report.valid, "invalid latitude fails validation");
    CHECK(report.has_errors(), "invalid validation report has errors");
    compiled = MissionDocumentService::compile_document(invalid);
    CHECK(!compiled.valid, "invalid document does not compile");

    MissionDocument loiter = valid_document();
    loiter.entities[1].type = "loiter";
    loiter.entities[1].command =
        static_cast<uint16_t>(NavCommand::NavLoiterTurns);
    loiter.entities[1].params = {2.0f, 0.0f, 100.0f, 0.0f};
    report = MissionDocumentService::validate_document(loiter);
    CHECK(report.valid, "loiter radius uses MAVLink param3");
    loiter.entities[1].params[2] = 0.0f;
    report = MissionDocumentService::validate_document(loiter);
    CHECK(!report.valid, "zero loiter radius fails catalog validation");

    MissionDocument changing_speed;
    changing_speed.defaults.speed_ms = 10.0f;
    auto add_point = [&](double lat, bool add_speed, float speed) {
        if (add_speed) {
            MissionEntity speed_change;
            speed_change.id = "speed-" + std::to_string(lat);
            speed_change.type = "command";
            speed_change.command =
                static_cast<uint16_t>(NavCommand::DoSetSpeed);
            speed_change.params[1] = speed;
            changing_speed.entities.push_back(speed_change);
        }
        MissionEntity point;
        point.id = "point-" + std::to_string(lat);
        point.type = "waypoint";
        point.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
        point.latitude = lat;
        point.longitude = 116.4;
        point.altitude_m = 50.0f;
        changing_speed.entities.push_back(point);
    };
    add_point(39.9, false, 0.0f);
    add_point(39.901, true, 10.0f);
    add_point(39.902, true, 5.0f);
    const auto changing_estimate =
        MissionDocumentService::estimate_document(changing_speed);
    CHECK(
        changing_estimate.estimated_time_s <
            changing_estimate.total_distance_m / 5.0,
        "estimate applies speed per segment");

    MissionDocument vertical_rate;
    vertical_rate.defaults.speed_ms = 10.0f;
    MissionEntity vertical_speed;
    vertical_speed.id = "vertical-speed";
    vertical_speed.type = "command";
    vertical_speed.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    vertical_speed.params = {2.0f, 5.0f, 0.0f, 0.0f};
    vertical_rate.entities.push_back(vertical_speed);
    MissionEntity vertical_start;
    vertical_start.id = "vertical-start";
    vertical_start.type = "waypoint";
    vertical_start.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    vertical_start.latitude = 39.9;
    vertical_start.longitude = 116.4;
    vertical_start.altitude_m = 50.0f;
    vertical_rate.entities.push_back(vertical_start);
    MissionEntity vertical_end = vertical_start;
    vertical_end.id = "vertical-end";
    vertical_end.latitude = 39.901;
    vertical_rate.entities.push_back(vertical_end);
    const auto vertical_estimate =
        MissionDocumentService::estimate_document(vertical_rate);
    CHECK(
        std::abs(
            vertical_estimate.estimated_time_s -
            vertical_estimate.total_distance_m / 10.0) < 0.001,
        "climb rate does not replace horizontal cruise speed");

    MissionDocument command_validation;
    MissionEntity camera_stop;
    camera_stop.id = "camera-stop";
    camera_stop.type = "command";
    camera_stop.command =
        static_cast<uint16_t>(NavCommand::DoSetCamTrigDist);
    camera_stop.params[0] = 0.0f;
    command_validation.entities.push_back(camera_stop);
    report = MissionDocumentService::validate_document(command_validation);
    CHECK(report.valid, "camera trigger distance zero is valid");

    MissionEntity servo;
    servo.id = "servo";
    servo.type = "command";
    servo.command = static_cast<uint16_t>(NavCommand::DoSetServo);
    servo.params[0] = 1.5f;
    servo.params[1] = 1500.0f;
    command_validation.entities.push_back(servo);
    report = MissionDocumentService::validate_document(command_validation);
    CHECK(!report.valid, "integer parameter rejects fractional value");

    command_validation.entities.pop_back();
    MissionEntity speed_valid;
    speed_valid.id = "speed-valid";
    speed_valid.type = "command";
    speed_valid.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    speed_valid.params = {1.0f, 8.0f, 0.0f, 0.0f};
    command_validation.entities.push_back(speed_valid);
    report = MissionDocumentService::validate_document(command_validation);
    CHECK(report.valid, "valid speed type and speed pass catalog validation");
    command_validation.entities.back().params[0] = 4.0f;
    report = MissionDocumentService::validate_document(command_validation);
    CHECK(!report.valid, "enum parameter rejects unsupported value");
}

static void test_ffi_contract() {
    fprintf(stderr, "\n[Group] Mission document FFI contract\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create succeeds");

    char* result = mg_mission_doc_create(
        core,
        R"({"metadata":{"name":"FFI Mission","vehicle_type":"copter"}})");
    CHECK(result != nullptr, "mission_doc_create returns JSON");
    CHECK(result && strstr(result, "\"success\":true"), "mission_doc_create succeeds");
    CHECK(result && strstr(result, "\"FFI Mission\""), "mission_doc_create applies metadata");
    if (result) mg_free_string(result);

    const char* add_takeoff =
        R"({"operation":"add","expected_revision":1,"entity":{"type":"takeoff","command":22,"alt_m":25}})";
    result = mg_mission_doc_apply(core, add_takeoff);
    CHECK(result && strstr(result, "\"success\":true"), "FFI add takeoff succeeds");
    if (result) mg_free_string(result);

    const char* add_waypoint =
        R"({"operation":"add","expected_revision":2,"entity":{"type":"waypoint","command":16,"lat":39.9,"lon":116.4,"alt_m":50,"params":[0,2,0,0]}})";
    result = mg_mission_doc_apply(core, add_waypoint);
    CHECK(result && strstr(result, "\"success\":true"), "FFI add waypoint succeeds");
    if (result) mg_free_string(result);

    result = mg_mission_doc_apply(core, add_takeoff);
    CHECK(result && strstr(result, "\"success\":false"), "FFI stale revision fails");
    CHECK(result && strstr(result, "\"code\":-5"), "FFI stale revision uses invalid-state code");
    if (result) mg_free_string(result);

    result = mg_mission_doc_validate(core);
    CHECK(result && strstr(result, "\"success\":true"), "FFI validate returns envelope");
    CHECK(result && strstr(result, "\"home.missing\""), "FFI validate reports missing home");
    if (result) mg_free_string(result);

    result = mg_mission_doc_compile(core);
    CHECK(result && strstr(result, "\"valid\":true"), "FFI compile succeeds");
    CHECK(result && strstr(result, "\"count\":2"), "FFI compile returns two items");
    if (result) mg_free_string(result);

    result = mg_mission_doc_estimate(core);
    CHECK(result && strstr(result, "\"enabled_entity_count\":2"), "FFI estimate counts entities");
    if (result) mg_free_string(result);

    result = mg_mission_get_command_catalog(core);
    CHECK(result && strstr(result, "\"schema_version\":1"), "FFI command catalog returns schema");
    CHECK(result && strstr(result, "\"NAV_WAYPOINT\""), "FFI command catalog includes waypoint");
    CHECK(result && strstr(result, "\"accept_radius\""), "FFI command catalog includes parameter metadata");
    CHECK(result && strstr(result, "\"index\":3"), "FFI command catalog exposes MAVLink parameter index");
    if (result) mg_free_string(result);

    result = mg_mission_doc_undo(core);
    CHECK(result && strstr(result, "\"success\":true"), "FFI undo succeeds");
    if (result) mg_free_string(result);

    result = mg_mission_doc_redo(core);
    CHECK(result && strstr(result, "\"success\":true"), "FFI redo succeeds");
    if (result) mg_free_string(result);

    result = mg_mission_doc_export(core);
    CHECK(result && strstr(result, "\"schema_version\":1"), "FFI export returns document JSON");
    if (result) mg_free_string(result);

    result = mg_mission_doc_import(core, "{\"schema_version\":1}");
    CHECK(result && strstr(result, "\"success\":true"), "FFI import accepts an empty document");
    if (result) mg_free_string(result);

    const auto file_path =
        std::filesystem::temp_directory_path() / "mgcs_mission_document_test.json";
    result = mg_mission_doc_create(core, R"({"metadata":{"name":"Saved Mission"}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_save(core, file_path.string().c_str());
    CHECK(result && strstr(result, "\"success\":true"), "FFI save writes mission document");
    if (result) mg_free_string(result);

    result = mg_mission_doc_load(core, file_path.string().c_str());
    CHECK(result && strstr(result, "\"Saved Mission\""), "FFI load restores mission document");
    if (result) mg_free_string(result);

    result = mg_mission_doc_create(core, R"({"metadata":{"name":"Overwritten Mission"}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_save(core, file_path.string().c_str());
    CHECK(result && strstr(result, "\"success\":true"), "FFI overwrite save succeeds");
    if (result) mg_free_string(result);
    result = mg_mission_doc_load(core, file_path.string().c_str());
    CHECK(
        result && strstr(result, "\"Overwritten Mission\""),
        "FFI overwrite save preserves the new document");
    if (result) mg_free_string(result);

    result = mg_mission_doc_load(core, "/path/that/does/not/exist.mgcad.json");
    CHECK(
        result && strstr(result, "\"code\":-14"),
        "FFI missing mission document uses not-found code");
    if (result) mg_free_string(result);

    std::error_code remove_error;
    std::filesystem::remove(file_path, remove_error);

    mg_destroy(core);
}

// 参数化测绘实体：文档只存设计意图，编译期由 SurveyCalculator 展开。
static void test_parameterized_survey_entities() {
    fprintf(stderr, "\n[Group] Parameterized survey entities\n");

    MissionDocument document = valid_document();
    document.entities.clear();

    MissionEntity survey;
    survey.id = "survey-1";
    survey.type = "survey_grid";
    survey.latitude = 39.9;
    survey.longitude = 116.4;
    survey.survey.width_m = 200.0;
    survey.survey.length_m = 300.0;
    survey.survey.altitude_m = 80.0f;
    survey.survey.speed_ms = 8.0f;
    survey.survey.angle_deg = 30.0;
    document.entities.push_back(survey);

    MissionValidationReport report =
        MissionDocumentService::validate_document(document);
    CHECK(!report.has_errors(),
          "survey_grid entity validates without the old not-implemented error");

    MissionCompileResult compiled =
        MissionDocumentService::compile_document(document);
    CHECK(compiled.valid, "survey_grid compiles");
    CHECK(compiled.plan.items.size() > 2,
          "survey_grid expands into multiple mission items");
    CHECK(compiled.source_entity_ids.size() == compiled.plan.items.size(),
          "every expanded item maps back to the source entity");
    bool all_from_survey = true;
    for (const auto& id : compiled.source_entity_ids) {
        if (id != "survey-1") all_from_survey = false;
    }
    CHECK(all_from_survey, "expanded items all reference the survey entity");
    bool sequenced = true;
    for (size_t i = 0; i < compiled.plan.items.size(); ++i) {
        if (compiled.plan.items[i].seq != i) sequenced = false;
    }
    CHECK(sequenced, "expanded items are re-sequenced contiguously");

    MissionEstimate estimate =
        MissionDocumentService::estimate_document(document);
    CHECK(estimate.total_distance_m > 0.0, "survey estimate has distance");
    CHECK(estimate.waypoint_count > 0, "survey estimate counts waypoints");
    CHECK(estimate.photo_count > 0, "survey estimate counts photos");

    // 序列化往返必须保留 survey 参数，否则前端保存后参数会丢。
    const std::string json = serialize_mission_document(document);
    MissionDocument reloaded;
    std::string error;
    CHECK(deserialize_mission_document(json, reloaded, error),
          "survey document round-trips through JSON");
    CHECK(reloaded.entities.size() == 1 &&
              std::fabs(reloaded.entities[0].survey.width_m - 200.0) < 1e-6 &&
              std::fabs(reloaded.entities[0].survey.length_m - 300.0) < 1e-6 &&
              std::fabs(reloaded.entities[0].survey.angle_deg - 30.0) < 1e-6,
          "survey parameters survive serialization");

    // 走廊实体：起终点必须都能通过校验，终点缺失要报错。
    MissionDocument corridor_document = valid_document();
    corridor_document.entities.clear();
    MissionEntity corridor;
    corridor.id = "corridor-1";
    corridor.type = "corridor";
    corridor.latitude = 39.9;
    corridor.longitude = 116.4;
    corridor.survey.end_latitude = 39.905;
    corridor.survey.end_longitude = 116.4;
    corridor.survey.corridor_width_m = 60.0;
    corridor.survey.altitude_m = 60.0f;
    corridor_document.entities.push_back(corridor);

    report = MissionDocumentService::validate_document(corridor_document);
    CHECK(!report.has_errors(), "corridor entity validates");
    compiled = MissionDocumentService::compile_document(corridor_document);
    CHECK(compiled.valid && compiled.plan.items.size() > 2,
          "corridor expands into mission items");

    MissionEntity broken = corridor;
    broken.id = "corridor-2";
    broken.survey.end_latitude = std::numeric_limits<double>::quiet_NaN();
    MissionDocument broken_document = valid_document();
    broken_document.entities.clear();
    broken_document.entities.push_back(broken);
    report = MissionDocumentService::validate_document(broken_document);
    CHECK(report.has_errors(), "corridor without an end point is rejected");
    compiled = MissionDocumentService::compile_document(broken_document);
    CHECK(!compiled.valid, "invalid corridor does not compile");

    MissionEntity bad_grid = survey;
    bad_grid.id = "survey-bad";
    bad_grid.survey.width_m = 0.0;
    MissionDocument bad_document = valid_document();
    bad_document.entities.clear();
    bad_document.entities.push_back(bad_grid);
    report = MissionDocumentService::validate_document(bad_document);
    CHECK(report.has_errors(), "zero-width survey is rejected");
}

// 前端走的是 FFI 路径：add 一个 survey 实体 -> validate -> compile -> export。
static void test_ffi_survey_entity_roundtrip() {
    fprintf(stderr, "\n[Group] Survey entity through FFI\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create succeeds (survey)");

    char* result = mg_mission_doc_create(
        core, R"({"metadata":{"name":"Survey Mission"}})");
    if (result) mg_free_string(result);

    const char* add_survey =
        R"({"operation":"add","expected_revision":1,"entity":{"type":"survey_grid",)"
        R"("lat":39.9,"lon":116.4,"survey":{"width_m":200,"length_m":200,)"
        R"("alt_m":70,"speed_ms":7,"angle_deg":15}}})";
    result = mg_mission_doc_apply(core, add_survey);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI add survey_grid succeeds");
    if (result) mg_free_string(result);

    result = mg_mission_doc_validate(core);
    CHECK(result && strstr(result, "\"valid\":true"),
          "FFI validates the survey document");
    if (result) mg_free_string(result);

    result = mg_mission_doc_compile(core);
    CHECK(result && strstr(result, "\"valid\":true"),
          "FFI compiles the survey document");
    CHECK(result && strstr(result, "\"source_entity_ids\""),
          "compile returns the source entity mapping");
    if (result) mg_free_string(result);

    result = mg_mission_doc_export(core);
    CHECK(result && strstr(result, "\"survey\""),
          "exported document keeps the survey spec");
    if (result) mg_free_string(result);

    mg_destroy(core);
}

// .waypoints 互操作：导出格式对齐 Mission Planner 的写出实现，且能原样读回。
static void test_waypoints_interchange() {
    fprintf(stderr, "\n[Group] .waypoints interchange\n");
    MissionDocument document = valid_document();
    const MissionCompileResult compiled =
        MissionDocumentService::compile_document(document);
    CHECK(compiled.valid, "document compiles for waypoints export");

    std::string error;
    const std::string text = export_waypoints_text(document, compiled.plan, error);
    CHECK(error.empty(), "waypoints export reports no error");
    CHECK(text.rfind("QGC WPL 110", 0) == 0,
          "waypoints export starts with the QGC WPL header");
    CHECK(text.find("\n0\t1\t0\t16\t") != std::string::npos,
          "waypoints export writes the MP-style home row at seq 0");
    CHECK(text.find("39.9000000") != std::string::npos,
          "waypoints export writes home latitude in degrees");

    MissionDocument imported;
    imported.metadata.name = "Imported";
    CHECK(import_waypoints_text(text, imported, error),
          "exported waypoints text re-imports");
    CHECK(imported.home.set && std::fabs(imported.home.latitude - 39.9) < 1e-6,
          "import restores home from the home row");
    CHECK(imported.entities.size() == compiled.plan.items.size(),
          "import restores one entity per mission item");
    CHECK(!imported.entities.empty() &&
              imported.entities[0].command ==
                  static_cast<uint16_t>(NavCommand::NavTakeoff),
          "import keeps the first mission command");

    const MissionValidationReport report =
        MissionDocumentService::validate_document(imported);
    CHECK(!report.has_errors(), "imported document validates");

    MissionDocument dummy;
    error.clear();
    CHECK(!import_waypoints_text("not a waypoint file\n", dummy, error),
          "missing QGC WPL header is rejected");
    CHECK(!error.empty(), "rejected import reports a reason");
    error.clear();
    CHECK(!import_waypoints_text("QGC WPL 110\n", dummy, error),
          "header without rows is rejected");
}

static void test_ffi_waypoints_interchange() {
    fprintf(stderr, "\n[Group] .waypoints interchange through FFI\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create succeeds (interchange)");

    char* result = mg_mission_doc_create(
        core, R"({"metadata":{"name":"Interchange"}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_apply(
        core,
        R"({"operation":"add","expected_revision":1,"entity":{"type":"waypoint","command":16,"lat":39.9,"lon":116.4,"alt_m":50}})");
    if (result) mg_free_string(result);

    result = mg_mission_doc_export_format(core, "waypoints");
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI export_format succeeds");
    CHECK(result && strstr(result, "QGC WPL 110"),
          "FFI export_format returns QGC WPL text");
    if (result) mg_free_string(result);

    result = mg_mission_doc_export_format(core, "kml");
    CHECK(result && strstr(result, "\"success\":false"),
          "FFI export_format rejects an unsupported format");
    if (result) mg_free_string(result);

    const char* import_text =
        "QGC WPL 110\n"
        "0\t1\t0\t16\t0\t0\t0\t0\t39.9000000\t116.4000000\t20.000000\t1\n"
        "1\t0\t3\t16\t0\t2\t0\t0\t39.9050000\t116.4050000\t50.000000\t1\n";
    result = mg_mission_doc_import_format(core, "waypoints", import_text);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI import_format accepts a QGC WPL file");
    CHECK(result && strstr(result, "\"waypoint-1\""),
          "FFI import_format brings in the mission entity");
    CHECK(result && strstr(result, "\"lat\":39.90"),
          "FFI import_format keeps the imported latitude");
    if (result) mg_free_string(result);

    result = mg_mission_doc_import_format(core, "waypoints", "garbage");
    CHECK(result && strstr(result, "\"success\":false"),
          "FFI import_format rejects a malformed file");
    if (result) mg_free_string(result);

    mg_destroy(core);
}

// Helpers for asserting the exact QGC ComplexItem JSON shape instead of just
// checking that the field names appear somewhere in the text.
static cJSON* json_at(cJSON* object, const char* key) {
    return object ? cJSON_GetObjectItemCaseSensitive(object, key) : nullptr;
}

static size_t json_array_size(cJSON* array) {
    return (array && cJSON_IsArray(array))
        ? static_cast<size_t>(cJSON_GetArraySize(array))
        : 0;
}

static bool json_float_near(cJSON* array, int index, double expected,
                            double epsilon) {
    cJSON* item = array ? cJSON_GetArrayItem(array, index) : nullptr;
    return item && cJSON_IsNumber(item) &&
           std::fabs(item->valuedouble - expected) < epsilon;
}

// 本地创建的 survey_grid / corridor 必须导出成 QGC 可重新打开的
// ComplexItem，而不是报错丢任务。这里同时校验 JSON 结构和展开任务项的
// 精度，确保 QGC 载入后看到的是同一个测绘区域和同一条航线。
static void test_qgc_complex_item_export() {
    fprintf(stderr, "\n[Group] QGC ComplexItem export\n");
    std::vector<std::string> warnings;
    std::string error;

    MissionDocument survey_document = valid_document();
    survey_document.entities.clear();
    MissionEntity survey;
    survey.id = "survey-1";
    survey.type = "survey_grid";
    survey.latitude = 39.9;
    survey.longitude = 116.4;
    survey.survey.width_m = 200.0;
    survey.survey.length_m = 300.0;
    survey.survey.altitude_m = 80.0f;
    survey.survey.speed_ms = 8.0f;
    survey.survey.angle_deg = 30.0;
    survey_document.entities.push_back(survey);

    const std::string survey_text =
        export_qgc_plan_json(survey_document, warnings, error);
    CHECK(error.empty(), "survey_grid exports to QGC plan without error");
    CHECK(!survey_text.empty(), "survey_grid export produced JSON");

    cJSON* survey_root = cJSON_Parse(survey_text.c_str());
    cJSON* survey_mission = json_at(survey_root, "mission");
    cJSON* survey_items = json_at(survey_mission, "items");
    CHECK(survey_items && json_array_size(survey_items) == 1,
          "survey_grid becomes a single complex mission item");
    cJSON* survey_item = survey_items ? cJSON_GetArrayItem(survey_items, 0) : nullptr;
    cJSON* survey_type = json_at(survey_item, "complexItemType");
    CHECK(survey_type && cJSON_IsString(survey_type) &&
              std::string(survey_type->valuestring) == "survey",
          "survey_grid complex item type is survey");
    cJSON* survey_polygon = json_at(survey_item, "polygon");
    CHECK(json_array_size(survey_polygon) == 4,
          "survey_grid exports a four-corner polygon");
    cJSON* survey_transect = json_at(survey_item, "TransectStyleComplexItem");
    cJSON* survey_embedded_items = json_at(survey_transect, "Items");
    const MissionCompileResult survey_compiled =
        MissionDocumentService::compile_document(survey_document);
    CHECK(survey_embedded_items &&
              json_array_size(survey_embedded_items) ==
                  survey_compiled.plan.items.size(),
          "survey_grid embeds every compiled mission item");
    CHECK(survey_embedded_items &&
              json_float_near(
                  json_at(cJSON_GetArrayItem(survey_embedded_items, 0), "params"),
                  4, survey_compiled.plan.items[0].x / 1e7, 1e-6),
          "embedded survey items keep their coordinates");
    cJSON_Delete(survey_root);

    MissionDocument survey_reimport;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(survey_text, survey_reimport, warnings, error),
          "exported survey complex item re-imports");
    CHECK(survey_reimport.complex_items.size() == 1 &&
              survey_reimport.complex_items[0].item_type == "survey",
          "re-imported survey keeps its complex item type");
    std::vector<MissionEntity> survey_expanded;
    error.clear();
    CHECK(!survey_reimport.complex_items.empty() &&
              expand_complex_item_entities(
                  survey_reimport.complex_items[0], survey_expanded, error),
          "re-imported survey complex item expands");
    CHECK(survey_expanded.size() == survey_compiled.plan.items.size(),
          "survey round-trip preserves the expanded item count");
    bool survey_coordinates_match = survey_expanded.size() == survey_compiled.plan.items.size();
    for (size_t i = 0; i < survey_expanded.size() && survey_coordinates_match; ++i) {
        survey_coordinates_match =
            std::fabs(survey_expanded[i].latitude -
                      survey_compiled.plan.items[i].x / 1e7) < 1e-6 &&
            std::fabs(survey_expanded[i].longitude -
                      survey_compiled.plan.items[i].y / 1e7) < 1e-6;
    }
    CHECK(survey_coordinates_match,
          "survey round-trip preserves expanded coordinates");

    MissionDocument corridor_document = valid_document();
    corridor_document.entities.clear();
    MissionEntity corridor;
    corridor.id = "corridor-1";
    corridor.type = "corridor";
    corridor.latitude = 39.9;
    corridor.longitude = 116.4;
    corridor.survey.end_latitude = 39.91;
    corridor.survey.end_longitude = 116.4;
    corridor.survey.corridor_width_m = 60.0;
    corridor.survey.altitude_m = 60.0f;
    corridor_document.entities.push_back(corridor);

    warnings.clear();
    error.clear();
    const std::string corridor_text =
        export_qgc_plan_json(corridor_document, warnings, error);
    CHECK(error.empty(), "corridor exports to QGC plan without error");

    cJSON* corridor_root = cJSON_Parse(corridor_text.c_str());
    cJSON* corridor_items = json_at(json_at(corridor_root, "mission"), "items");
    cJSON* corridor_item =
        corridor_items ? cJSON_GetArrayItem(corridor_items, 0) : nullptr;
    cJSON* corridor_type = json_at(corridor_item, "complexItemType");
    CHECK(corridor_type && cJSON_IsString(corridor_type) &&
              std::string(corridor_type->valuestring) == "CorridorScan",
          "corridor complex item type is CorridorScan");
    CHECK(json_array_size(json_at(corridor_item, "polyline")) == 2,
          "corridor exports the start/end polyline");
    cJSON_Delete(corridor_root);

    MissionDocument corridor_reimport;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(corridor_text, corridor_reimport, warnings, error),
          "exported corridor complex item re-imports");
    CHECK(corridor_reimport.complex_items.size() == 1 &&
              corridor_reimport.complex_items[0].item_type == "CorridorScan",
          "re-imported corridor keeps its complex item type");
}

static void test_qgc_plan_interchange() {
    fprintf(stderr, "\n[Group] QGC .plan interchange\n");
    const MissionDocument document = valid_document();
    std::vector<std::string> warnings;
    std::string error;
    const std::string text = export_qgc_plan_json(document, warnings, error);
    CHECK(error.empty(), "QGC plan export reports no error");
    CHECK(text.find("\"fileType\"") != std::string::npos,
          "QGC plan export writes fileType");
    CHECK(text.find("\"Plan\"") != std::string::npos,
          "QGC plan export marks the file as Plan");
    CHECK(text.find("\"params\"") != std::string::npos,
          "QGC plan export writes SimpleItem params");
    CHECK(text.find("\"plannedHomePosition\"") != std::string::npos,
          "QGC plan export writes the planned home position");

    MissionDocument imported;
    imported.metadata.name = "Imported QGC Plan";
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(text, imported, warnings, error),
          "exported QGC plan re-imports");
    CHECK(imported.metadata.name == "Imported QGC Plan",
          "QGC plan import preserves the local document name");
    CHECK(imported.home.set && std::fabs(imported.home.latitude - 39.9) < 1e-6,
          "QGC plan import restores the planned home position");
    CHECK(imported.entities.size() == document.entities.size(),
          "QGC plan import restores every simple entity");
    CHECK(imported.entities[0].command == static_cast<uint16_t>(NavCommand::NavTakeoff),
          "QGC plan import restores the command order");
    CHECK(imported.entities[1].params[1] == 2.0f,
          "QGC plan import restores command parameters");
    CHECK(!MissionDocumentService::validate_document(imported).has_errors(),
          "round-tripped QGC plan validates");

    const std::string legacy_plan = R"({
        "fileType": "Plan",
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{
                "type": "SimpleItem",
                "frame": 3,
                "command": 16,
                "autoContinue": true,
                "doJumpId": 1,
                "coordinate": [39.905, 116.405, 50],
                "params": [0, 2, 0, 0]
            }]
        }
    })";
    MissionDocument legacy;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(legacy_plan, legacy, warnings, error),
          "legacy QGC coordinate-style plan imports");
    CHECK(legacy.entities.size() == 1 &&
              std::fabs(legacy.entities[0].latitude - 39.905) < 1e-6,
          "legacy QGC coordinate is preserved");

    const std::string complex_plan = R"({
        "fileType": "Plan",
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{
                "type": "ComplexItem",
                "complexItemType": "Survey",
                "version": 1,
                "polygon": [[39.9, 116.4], [39.91, 116.41], [39.92, 116.4]],
                "gridAngle": 0
            }]
        }
    })";
    MissionDocument complex;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(complex_plan, complex, warnings, error),
          "complex QGC item imports as a formal complex item");
    CHECK(complex.extensions.count("qgc_plan.complex_items") == 0,
          "complex QGC item no longer leaks into the extension store");
    CHECK(complex.complex_items.size() == 1,
          "complex QGC item is stored in the formal model");
    CHECK(!complex.complex_items.empty() &&
              complex.complex_items[0].item_type == "Survey",
          "complex item type is inferred from complexItemType");
    CHECK(!complex.complex_items.empty() &&
              complex.complex_items[0].json.find("gridAngle") != std::string::npos,
          "complex item retains its raw QGC JSON payload");

    const std::string complex_doc = serialize_mission_document(complex);
    MissionDocument complex_round;
    warnings.clear();
    error.clear();
    CHECK(deserialize_mission_document(complex_doc, complex_round, error),
          "mission document with complex item round-trips");
    CHECK(complex_round.complex_items.size() == 1 &&
              complex_round.complex_items[0].item_type == "Survey",
          "complex item survives mission document round-trip");

    const std::string complex_export = export_qgc_plan_json(complex, warnings, error);
    CHECK(complex_export.find("\"complexItemType\"") != std::string::npos &&
              complex_export.find("Survey") != std::string::npos,
          "complex item is restored as a QGC ComplexItem on export");
    MissionDocument complex_reimport;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(complex_export, complex_reimport, warnings, error) &&
              complex_reimport.complex_items.size() == 1 &&
              complex_reimport.complex_items[0].item_type == "Survey",
          "exported ComplexItem re-imports with its type intact");

    const std::string interleaved_plan = R"({
        "fileType": "Plan",
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [
                {"type": "SimpleItem", "frame": 3, "command": 16,
                 "autoContinue": true, "doJumpId": 1,
                 "params": [0, 0, 0, 0, 39.9, 116.4, 50]},
                {"type": "ComplexItem", "complexItemType": "Survey",
                 "version": 1, "polygon": [[39.9, 116.4], [39.91, 116.41]]},
                {"type": "SimpleItem", "frame": 3, "command": 16,
                 "autoContinue": true, "doJumpId": 3,
                 "params": [0, 0, 0, 0, 39.91, 116.41, 50]}
            ]
        }
    })";
    MissionDocument interleaved;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(interleaved_plan, interleaved, warnings, error),
          "interleaved QGC plan imports");
    CHECK(interleaved.entities.size() == 2 &&
              interleaved.complex_items.size() == 1,
          "interleaved plan keeps two simple entities and one complex item");
    CHECK(interleaved.complex_items[0].entity_insert_index == 1,
          "complex item records its position between entities");

    const std::string interleaved_doc = serialize_mission_document(interleaved);
    MissionDocument interleaved_round;
    error.clear();
    CHECK(deserialize_mission_document(interleaved_doc, interleaved_round, error) &&
              interleaved_round.complex_items.size() == 1 &&
              interleaved_round.complex_items[0].entity_insert_index == 1,
          "complex item insert index round-trips through the document JSON");

    const std::string interleaved_export =
        export_qgc_plan_json(interleaved, warnings, error);
    const size_t first_simple = interleaved_export.find("\"SimpleItem\"");
    const size_t survey_position = interleaved_export.find("Survey");
    const size_t last_simple = interleaved_export.rfind("\"SimpleItem\"");
    CHECK(first_simple != std::string::npos && survey_position != std::string::npos &&
              last_simple != std::string::npos &&
              first_simple < survey_position && survey_position < last_simple,
          "complex item is exported between the two simple items");

    const auto interleaved_validation =
        MissionDocumentService::validate_document(interleaved);
    bool complex_error = false;
    for (const auto& diagnostic : interleaved_validation.diagnostics) {
        if (diagnostic.code == "complex_item.unsupported") {
            complex_error = true;
        }
    }
    CHECK(!interleaved_validation.valid && complex_error,
          "complex items block validation so upload cannot silently drop them");
    const auto interleaved_compile =
        MissionDocumentService::compile_document(interleaved);
    CHECK(!interleaved_compile.valid,
          "document with complex items cannot be compiled for upload");

    const std::string fence_plan = R"({
        "fileType": "Plan",
        "geoFence": {
            "version": 2,
            "polygons": [{
                "version": 1,
                "inclusion": false,
                "polygon": [[39.9, 116.4], [39.91, 116.41], [39.92, 116.4]]
            }],
            "circles": [{
                "version": 1,
                "inclusion": true,
                "circle": {"center": [39.9, 116.4], "radius": 120}
            }],
            "breachReturn": [39.9, 116.4, 35]
        },
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": []
        }
    })";
    MissionDocument fenced;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(fence_plan, fenced, warnings, error),
          "QGC GeoFence imports into the formal document model");
    CHECK(fenced.geo_fence.polygons.size() == 1 &&
              fenced.geo_fence.polygons[0].vertices.size() == 3 &&
              !fenced.geo_fence.polygons[0].inclusion,
          "QGC GeoFence polygon inclusion and vertices are preserved");
    CHECK(fenced.geo_fence.circles.size() == 1 &&
              std::fabs(fenced.geo_fence.circles[0].radius_m - 120.0) < 1e-6,
          "QGC GeoFence circle is preserved");
    CHECK(fenced.geo_fence.breach_return_set,
          "QGC GeoFence breach-return point is preserved");
    const std::string fence_export = export_qgc_plan_json(fenced, warnings, error);
    CHECK(fence_export.find("\"geoFence\"") != std::string::npos &&
              fence_export.find("39.91") != std::string::npos &&
              fence_export.find("\"radius\"") != std::string::npos,
          "formal GeoFence is restored on QGC export");
    const std::string fence_document_json = serialize_mission_document(fenced);
    MissionDocument fence_restored;
    CHECK(deserialize_mission_document(fence_document_json, fence_restored, error),
          "formal GeoFence round-trips through mission document JSON");
    CHECK(fence_restored.geo_fence.polygons.size() == 1 &&
              fence_restored.geo_fence.circles.size() == 1 &&
              fence_restored.geo_fence.breach_return_set,
          "formal GeoFence document fields survive JSON round-trip");

    const std::string rally_plan = R"({
        "fileType": "Plan",
        "rallyPoints": {"version": 2, "points": [[39.9, 116.4, 30], [39.91, 116.41, 40]]},
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": []
        }
    })";
    MissionDocument rallied;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(rally_plan, rallied, warnings, error),
          "QGC Rally points import into the formal document model");
    CHECK(rallied.rally_points.size() == 2 &&
              std::fabs(rallied.rally_points[0].latitude - 39.9) < 1e-6 &&
              std::fabs(rallied.rally_points[1].longitude - 116.41) < 1e-6,
          "Rally point coordinates are preserved");
    const std::string rally_export = export_qgc_plan_json(rallied, warnings, error);
    CHECK(rally_export.find("\"rallyPoints\"") != std::string::npos &&
              rally_export.find("39.91") != std::string::npos,
          "formal Rally points are restored on QGC export");

    MissionDocument survey_document;
    MissionEntity survey;
    survey.id = "survey-1";
    survey.type = "survey_grid";
    survey.command = 0;
    survey_document.entities.push_back(survey);
    warnings.clear();
    error.clear();
    const std::string survey_export =
        export_qgc_plan_json(survey_document, warnings, error);
    CHECK(error.empty() &&
              survey_export.find("\"complexItemType\"") != std::string::npos &&
              survey_export.find("\"survey\"") != std::string::npos,
          "parameterized survey export becomes a QGC complex item");
}

static void test_mission_format_recommendation() {
    fprintf(stderr, "\n[Group] Mission file format recommendation\n");
    MissionDocument ardupilot = valid_document();
    CHECK(
        recommended_mission_interchange_format(ardupilot) == "waypoints",
        "ArduPilot simple missions recommend waypoints");

    ardupilot.metadata.firmware_profile = "px4";
    CHECK(
        recommended_mission_interchange_format(ardupilot) == "plan",
        "PX4 missions recommend plan");

    MissionDocument survey = valid_document();
    MissionEntity complex;
    complex.id = "survey-recommendation";
    complex.type = "survey_grid";
    complex.command = 0;
    survey.entities.push_back(std::move(complex));
    CHECK(
        recommended_mission_interchange_format(survey) == "plan",
        "parameterized survey missions recommend plan");

    MissionDocument unknown = valid_document();
    unknown.metadata.firmware_profile = "unknown";
    CHECK(
        recommended_mission_interchange_format(unknown) == "waypoints",
        "unknown firmware defaults to waypoints");
}

static void test_ffi_qgc_plan_interchange() {
    fprintf(stderr, "\n[Group] QGC .plan interchange through FFI\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create succeeds (QGC plan)");

    char* result = mg_mission_doc_create(
        core, R"({"metadata":{"name":"QGC Plan"}})");
    if (result) mg_free_string(result);
    result = mg_mission_doc_apply(
        core,
        R"({"operation":"add","expected_revision":1,"entity":{"type":"waypoint","command":16,"lat":39.9,"lon":116.4,"alt_m":50}})");
    if (result) mg_free_string(result);

    result = mg_mission_doc_export_format(core, "plan");
    CHECK(result && strstr(result, "\"format\":\"plan\""),
          "FFI QGC plan export reports plan format");
    CHECK(result && strstr(result, "\\\"fileType\\\""),
          "FFI QGC plan export returns Plan JSON");
    if (result) mg_free_string(result);

    const char* import_text = R"({
        "fileType": "Plan",
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{
                "type": "SimpleItem",
                "frame": 3,
                "command": 16,
                "autoContinue": true,
                "params": [0, 2, 0, 0, 39.905, 116.405, 50]
            }]
        }
    })";
    result = mg_mission_doc_import_format(core, "plan", import_text);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI QGC plan import succeeds");
    CHECK(result && strstr(result, "\"plan-item-1\""),
          "FFI QGC plan import creates stable item ids");
    CHECK(result && strstr(result, "\"warnings\""),
          "FFI QGC plan import reports warnings envelope");
    if (result) mg_free_string(result);

    mg_destroy(core);
}

static void test_geofence_rally_validation() {
    fprintf(stderr, "\n[Group] GeoFence and rally geometry validation\n");

    CHECK(MissionDocumentService::validate_document(valid_document()).valid,
          "document with a well-formed rally point passes validation");

    MissionDocument bad_rally = valid_document();
    bad_rally.rally_points[0].latitude =
        std::numeric_limits<double>::quiet_NaN();
    auto report = MissionDocumentService::validate_document(bad_rally);
    CHECK(!report.valid && report_has_code(report, "rally.latitude"),
          "rally point with NaN latitude fails validation");

    MissionDocument bad_polygon = valid_document();
    MissionGeoFencePolygon polygon;
    polygon.vertices = {{39.9, 116.4}, {39.91, 116.41}};
    bad_polygon.geo_fence.polygons.push_back(polygon);
    report = MissionDocumentService::validate_document(bad_polygon);
    CHECK(!report.valid && report_has_code(report, "geo_fence.polygon_vertices"),
          "polygon with fewer than three vertices fails validation");

    MissionDocument bad_circle = valid_document();
    MissionGeoFenceCircle circle;
    circle.center = {39.9, 116.4};
    circle.radius_m = 0.0;
    bad_circle.geo_fence.circles.push_back(circle);
    report = MissionDocumentService::validate_document(bad_circle);
    CHECK(!report.valid && report_has_code(report, "geo_fence.circle_radius"),
          "circle with non-positive radius fails validation");

    MissionDocument bad_breach = valid_document();
    bad_breach.geo_fence.breach_return_set = true;
    bad_breach.geo_fence.breach_return = {200.0, 116.4};
    bad_breach.geo_fence.breach_return_altitude_m = 30.0f;
    report = MissionDocumentService::validate_document(bad_breach);
    CHECK(!report.valid && report_has_code(report, "geo_fence.breach_return"),
          "breach-return point outside the latitude range fails validation");

    MissionDocument good_fence = valid_document();
    MissionGeoFencePolygon good_polygon;
    good_polygon.vertices = {{39.9, 116.4}, {39.91, 116.41}, {39.92, 116.4}};
    good_fence.geo_fence.polygons.push_back(good_polygon);
    MissionGeoFenceCircle good_circle;
    good_circle.center = {39.9, 116.4};
    good_circle.radius_m = 120.0;
    good_fence.geo_fence.circles.push_back(good_circle);
    CHECK(MissionDocumentService::validate_document(good_fence).valid,
          "well-formed polygon and circle fence passes validation");
}

static void test_geofence_compile() {
    fprintf(stderr, "\n[Group] GeoFence mission compilation\n");

    MissionGeoFence fence;
    MissionGeoFencePolygon excluded;
    excluded.inclusion = false;
    excluded.vertices = {{39.9, 116.4}, {39.91, 116.41}, {39.92, 116.4}};
    fence.polygons.push_back(excluded);

    MissionGeoFenceCircle included;
    included.inclusion = true;
    included.center = {39.9, 116.4};
    included.radius_m = 120.0;
    fence.circles.push_back(included);

    fence.breach_return_set = true;
    fence.breach_return = {39.9, 116.4};
    fence.breach_return_altitude_m = 35.0f;

    MissionPlan plan;
    std::string error;
    CHECK(compile_geo_fence_plan(fence, plan, error),
          "well-formed fence compiles to a fence mission");
    CHECK(plan.items.size() == 5,
          "fence compiles to three vertices, one circle and one return point");
    if (plan.items.size() == 5) {
        CHECK(plan.items[0].command == 5002 && plan.items[0].frame == 0 &&
                  plan.items[0].param1 == 3.0f &&
                  plan.items[0].mission_type == 1,
              "polygon vertex compiles as exclusion with the vertex count");
        CHECK(plan.items[2].command == 5002 && plan.items[2].seq == 2,
              "polygon vertices keep a contiguous sequence");
        CHECK(plan.items[3].command == 5003 &&
                  std::fabs(plan.items[3].param1 - 120.0f) < 1e-3f,
              "circle compiles as inclusion with its radius");
        CHECK(plan.items[4].command == 5000 && plan.items[4].frame == 3 &&
                  std::fabs(plan.items[4].z - 35.0f) < 1e-3f,
              "breach return compiles with relative altitude");
        CHECK(plan.items[3].seq == 3 && plan.items[4].seq == 4,
              "fence sequence numbers are contiguous");
    }

    MissionGeoFence empty_fence;
    MissionPlan empty_plan;
    CHECK(!compile_geo_fence_plan(empty_fence, empty_plan, error) &&
              !error.empty(),
          "empty fence cannot be compiled");

    MissionGeoFence short_polygon;
    MissionGeoFencePolygon line;
    line.vertices = {{39.9, 116.4}, {39.91, 116.41}};
    short_polygon.polygons.push_back(line);
    CHECK(!compile_geo_fence_plan(short_polygon, empty_plan, error),
          "polygon with fewer than three vertices cannot be compiled");
}

static void test_fence_rally_plan_roundtrip() {
    fprintf(stderr, "\n[Group] Fence and rally plan round-trip\n");

    MissionGeoFence fence;
    MissionGeoFencePolygon included;
    included.inclusion = true;
    included.vertices = {{39.9, 116.4}, {39.91, 116.41}, {39.92, 116.4}};
    fence.polygons.push_back(included);
    MissionGeoFencePolygon excluded;
    excluded.inclusion = false;
    excluded.vertices = {{39.8, 116.3}, {39.81, 116.31}, {39.82, 116.3}};
    fence.polygons.push_back(excluded);
    MissionGeoFenceCircle circle;
    circle.inclusion = false;
    circle.center = {39.9, 116.4};
    circle.radius_m = 90.0;
    fence.circles.push_back(circle);
    fence.breach_return_set = true;
    fence.breach_return = {39.9, 116.4};
    fence.breach_return_altitude_m = 35.0f;

    MissionPlan plan;
    std::string error;
    CHECK(compile_geo_fence_plan(fence, plan, error),
          "two polygons, a circle and a return point compile");

    MissionGeoFence restored;
    CHECK(geo_fence_from_plan(plan, restored, error),
          "fence plan parses back into the document model");
    CHECK(restored.polygons.size() == 2 && restored.circles.size() == 1 &&
              restored.breach_return_set,
          "fence round-trip keeps two polygons, one circle and the return point");
    if (restored.polygons.size() == 2) {
        CHECK(restored.polygons[0].inclusion && !restored.polygons[1].inclusion &&
                  restored.polygons[0].vertices.size() == 3,
              "polygon inclusion flags and vertices survive the round-trip");
        CHECK(std::fabs(restored.polygons[1].vertices[1].latitude - 39.81) < 1e-6,
              "polygon coordinates survive the round-trip");
    }
    if (restored.circles.size() == 1) {
        CHECK(!restored.circles[0].inclusion &&
                  std::fabs(restored.circles[0].radius_m - 90.0) < 1e-3,
              "circle inclusion flag and radius survive the round-trip");
    }

    // 损坏的 fence 计划必须被拒绝，而不是产生半个多边形。
    MissionPlan truncated;
    truncated.items.push_back(plan.items[0]);
    truncated.items.push_back(plan.items[1]);
    MissionGeoFence partial;
    CHECK(!geo_fence_from_plan(truncated, partial, error) && !error.empty(),
          "incomplete polygon at the end of the list is rejected");

    MissionPlan mixed = plan;
    mixed.items[1].param1 = 4.0f;
    CHECK(!geo_fence_from_plan(mixed, partial, error),
          "polygon vertex count changing mid-polygon is rejected");

    MissionPlan unsupported;
    MissionItem bogus{};
    bogus.command = 16;
    unsupported.items.push_back(bogus);
    CHECK(!geo_fence_from_plan(unsupported, partial, error),
          "unsupported fence command is rejected");

    std::vector<MissionRallyPoint> rally = {
        {39.9, 116.4, 30.0f},
        {39.91, 116.41, 40.0f},
    };
    MissionPlan rally_plan;
    CHECK(compile_rally_plan(rally, rally_plan, error),
          "rally points compile for the round-trip");
    std::vector<MissionRallyPoint> rally_restored;
    CHECK(rally_points_from_plan(rally_plan, rally_restored, error) &&
              rally_restored.size() == 2 &&
              std::fabs(rally_restored[1].altitude_m - 40.0f) < 1e-3f,
          "rally points survive the plan round-trip");

    MissionPlan bad_rally;
    bad_rally.items.push_back(bogus);
    CHECK(!rally_points_from_plan(bad_rally, rally_restored, error),
          "unsupported rally command is rejected");
}

static void test_complex_item_expansion() {
    fprintf(stderr, "\n[Group] QGC ComplexItem expansion\n");

    // QGC saves the computed mission items inside the complex item. M-GCS reuses
    // them instead of re-deriving the survey geometry.
    MissionDocument document;
    document.metadata.id = "complex-expand";
    document.home.set = true;
    document.home.latitude = 39.9;
    document.home.longitude = 116.4;
    document.defaults.altitude_m = 50.0f;
    document.defaults.speed_ms = 10.0f;

    MissionEntity waypoint;
    waypoint.id = "waypoint-1";
    waypoint.type = "waypoint";
    waypoint.command = static_cast<uint16_t>(NavCommand::NavWaypoint);
    waypoint.latitude = 39.9001;
    waypoint.longitude = 116.4001;
    waypoint.altitude_m = 50.0f;
    document.entities.push_back(waypoint);

    MissionComplexItem survey;
    survey.id = "plan-complex-item-2";
    survey.item_type = "Survey";
    survey.entity_insert_index = 1;
    survey.json = R"({
        "type": "ComplexItem",
        "complexItemType": "Survey",
        "TransectStyleComplexItem": {
            "version": 2,
            "Items": [
                {"type": "SimpleItem", "frame": 3, "command": 16,
                 "autoContinue": true, "doJumpId": 1,
                 "params": [0, 2, 0, 0, 39.91, 116.41, 60]},
                {"type": "SimpleItem", "frame": 3, "command": 16,
                 "autoContinue": true, "doJumpId": 2,
                 "params": [0, 2, 0, 0, 39.92, 116.42, 60]}
            ]
        }
    })";
    document.complex_items.push_back(survey);

    std::vector<MissionEntity> expanded;
    std::string error;
    CHECK(expand_complex_item_entities(survey, expanded, error) &&
              expanded.size() == 2,
          "complex item expands into its stored mission items");
    if (expanded.size() == 2) {
        CHECK(expanded[0].id == "plan-complex-item-2-item-1" &&
                  std::fabs(expanded[1].latitude - 39.92) < 1e-9,
              "expanded items keep stable ids and coordinates");
    }

    const auto report = MissionDocumentService::validate_document(document);
    CHECK(report.valid, "document with an expandable complex item is valid");
    CHECK(report_has_code(report, "complex_item.expanded"),
          "expanded complex item is reported as read-only");
    CHECK(!report_has_code(report, "complex_item.unsupported"),
          "expandable complex item no longer blocks validation");

    const auto compiled = MissionDocumentService::compile_document(document);
    CHECK(compiled.valid && compiled.plan.items.size() == 3,
          "compile includes the expanded complex item items");
    CHECK(compiled.source_entity_ids.size() == 3 &&
              compiled.source_entity_ids[1] == "plan-complex-item-2" &&
              compiled.source_entity_ids[2] == "plan-complex-item-2",
          "expanded items report the complex item as their source");

    const auto estimate = MissionDocumentService::estimate_document(document);
    CHECK(estimate.waypoint_count == 3,
          "estimate counts the expanded complex item waypoints");

    MissionComplexItem broken;
    broken.id = "plan-complex-item-3";
    broken.item_type = "Survey";
    broken.json = R"({
        "type": "ComplexItem",
        "complexItemType": "Survey",
        "polygon": [[39.9, 116.4], [39.91, 116.41], [39.92, 116.4]]
    })";
    CHECK(!expand_complex_item_entities(broken, expanded, error),
          "complex item without TransectStyleComplexItem is rejected");

    MissionDocument unsupported = document;
    unsupported.complex_items[0].json = R"({
        "TransectStyleComplexItem": {
            "Items": [{"type": "SimpleItem", "frame": 3, "command": 9999,
                       "autoContinue": true,
                       "params": [0, 0, 0, 0, 39.9, 116.4, 50]}]
        }
    })";
    const auto unsupported_report =
        MissionDocumentService::validate_document(unsupported);
    CHECK(!unsupported_report.valid &&
              report_has_code(unsupported_report,
                              "complex_item.unsupported_command"),
          "expanded command outside the catalog fails validation");
}

static void test_ffi_geofence_compile() {
    fprintf(stderr, "\n[Group] GeoFence compile through FFI\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create succeeds (fence)");

    char* result =
        mg_mission_doc_create(core, R"({"metadata":{"name":"Fence"}})");
    if (result) mg_free_string(result);

    const char* fence_plan = R"({
        "fileType": "Plan",
        "geoFence": {
            "version": 2,
            "polygons": [{
                "version": 1,
                "inclusion": true,
                "polygon": [[39.9, 116.4], [39.91, 116.41], [39.92, 116.4]]
            }],
            "circles": [{
                "version": 1,
                "inclusion": false,
                "circle": {"center": [39.9, 116.4], "radius": 90}
            }]
        },
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": []
        }
    })";
    result = mg_mission_doc_import_format(core, "plan", fence_plan);
    if (result) mg_free_string(result);

    result = mg_mission_doc_compile_fence(core);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI fence compile succeeds for a valid fence");
    CHECK(result && strstr(result, "\"item_count\":4"),
          "FFI fence compile reports three vertices plus one circle");
    CHECK(result && strstr(result, "5001") && strstr(result, "5004"),
          "FFI fence compile emits inclusion and exclusion commands");
    if (result) mg_free_string(result);

    mg_destroy(core);
}

static void test_rally_compile() {
    fprintf(stderr, "\n[Group] Rally mission compilation\n");

    std::vector<MissionRallyPoint> points = {
        {39.9, 116.4, 30.0f},
        {39.91, 116.41, 40.0f},
    };
    MissionPlan plan;
    std::string error;
    CHECK(compile_rally_plan(points, plan, error),
          "rally points compile to a rally mission");
    CHECK(plan.items.size() == 2, "each rally point becomes one item");
    if (plan.items.size() == 2) {
        CHECK(plan.items[0].command == 5100 && plan.items[0].frame == 3 &&
                  plan.items[0].mission_type == 2,
              "rally item uses NAV_RALLY_POINT, relative frame and rally type");
        CHECK(std::fabs(plan.items[1].z - 40.0f) < 1e-3f &&
                  plan.items[1].seq == 1,
              "rally altitude and sequence are encoded");
    }

    std::vector<MissionRallyPoint> empty;
    CHECK(!compile_rally_plan(empty, plan, error) && !error.empty(),
          "empty rally list cannot be compiled");

    std::vector<MissionRallyPoint> invalid = {{200.0, 116.4, 30.0f}};
    CHECK(!compile_rally_plan(invalid, plan, error),
          "rally point outside the latitude range cannot be compiled");
}

static void test_ffi_rally_compile() {
    fprintf(stderr, "\n[Group] Rally compile through FFI\n");
    mg_core_t* core = mg_create();
    CHECK(core != nullptr, "mg_create succeeds (rally)");

    char* result =
        mg_mission_doc_create(core, R"({"metadata":{"name":"Rally"}})");
    if (result) mg_free_string(result);

    const char* rally_plan = R"({
        "fileType": "Plan",
        "rallyPoints": {
            "version": 2,
            "points": [[39.9, 116.4, 30], [39.91, 116.41, 40]]
        },
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": []
        }
    })";
    result = mg_mission_doc_import_format(core, "plan", rally_plan);
    if (result) mg_free_string(result);

    result = mg_mission_doc_compile_rally(core);
    CHECK(result && strstr(result, "\"success\":true"),
          "FFI rally compile succeeds for valid points");
    CHECK(result && strstr(result, "\"item_count\":2") &&
              strstr(result, "5100"),
          "FFI rally compile reports two NAV_RALLY_POINT items");
    if (result) mg_free_string(result);

    mg_destroy(core);
}

/// VTOL 起飞/降落（MAV_CMD 84/85）：QGC 按机型在 22/84 之间选命令，
/// 我们之前目录里根本没有这两个，用户找不到。
static void test_vtol_takeoff_and_land_commands() {
    fprintf(stderr, "\n[Group] VTOL takeoff / land commands\n");

    const auto& catalog = mission_command_catalog();
    const auto find = [&catalog](uint16_t command) {
        for (const auto& entry : catalog) {
            if (entry.command == command) return &entry;
        }
        return static_cast<const MissionCommandDescriptor*>(nullptr);
    };

    const auto* takeoff = find(84);
    CHECK(takeoff != nullptr && takeoff->name == "NAV_VTOL_TAKEOFF" &&
              takeoff->entity_type == "vtol_takeoff",
          "catalog exposes NAV_VTOL_TAKEOFF with a vtol_takeoff entity type");
    CHECK(takeoff != nullptr && takeoff->requires_position &&
              takeoff->requires_altitude,
          "VTOL takeoff requires a position and an altitude");
    if (takeoff != nullptr) {
        bool has_transition_heading = false;
        for (const auto& parameter : takeoff->parameters) {
            if (parameter.index == 2 && parameter.options.size() == 5) {
                has_transition_heading = true;
            }
        }
        CHECK(has_transition_heading,
              "VTOL takeoff param2 carries the MAVLink transition-heading options");
    }

    const auto* land = find(85);
    CHECK(land != nullptr && land->name == "NAV_VTOL_LAND" &&
              land->entity_type == "vtol_land",
          "catalog exposes NAV_VTOL_LAND with a vtol_land entity type");
    if (land != nullptr) {
        bool has_land_options = false;
        for (const auto& parameter : land->parameters) {
            if (parameter.index == 1 && parameter.options.size() == 3) {
                has_land_options = true;
            }
        }
        CHECK(has_land_options,
              "VTOL land param1 carries the MAVLink land options");
    }

    // 一个完整的 VTOL 任务：垂直起飞 → 航点 → 垂直降落。
    MissionDocument document = valid_document();
    document.entities.clear();

    MissionEntity vertical_takeoff;
    vertical_takeoff.id = "vtol-takeoff";
    vertical_takeoff.type = "vtol_takeoff";
    vertical_takeoff.command = 84;
    vertical_takeoff.frame = static_cast<uint8_t>(CoordFrame::GlobalRelAlt);
    vertical_takeoff.latitude = 31.2304;
    vertical_takeoff.longitude = 121.4737;
    vertical_takeoff.altitude_m = 60.0f;
    vertical_takeoff.params = {0.0f, 0.0f, 0.0f, 0.0f};
    document.entities.push_back(vertical_takeoff);

    MissionEntity waypoint;
    waypoint.id = "wp-1";
    waypoint.type = "waypoint";
    waypoint.command = 16;
    waypoint.latitude = 31.2404;
    waypoint.longitude = 121.4837;
    waypoint.altitude_m = 60.0f;
    waypoint.params = {0.0f, 2.0f, 0.0f, 0.0f};
    document.entities.push_back(waypoint);

    MissionEntity vertical_land;
    vertical_land.id = "vtol-land";
    vertical_land.type = "vtol_land";
    vertical_land.command = 85;
    vertical_land.latitude = 31.2304;
    vertical_land.longitude = 121.4737;
    vertical_land.altitude_m = 0.0f;
    vertical_land.params = {0.0f, 0.0f, 0.0f, 0.0f};
    document.entities.push_back(vertical_land);

    const auto report = MissionDocumentService::validate_document(document);
    CHECK(report.valid, "VTOL takeoff / waypoint / VTOL land validates");
    CHECK(!report_has_code(report, "entity.command_unsupported"),
          "VTOL commands are in the catalog so they are not flagged unsupported");
    CHECK(!report_has_code(report, "entity.type_command_mismatch"),
          "vtol_takeoff/vtol_land entity types match their commands");

    const auto compiled = MissionDocumentService::compile_document(document);
    CHECK(compiled.valid && compiled.plan.items.size() == 3,
          "VTOL mission compiles to three items");
    if (compiled.plan.items.size() == 3) {
        CHECK(compiled.plan.items[0].command == 84,
              "first item compiles as NAV_VTOL_TAKEOFF");
        CHECK(compiled.plan.items[2].command == 85,
              "last item compiles as NAV_VTOL_LAND");
    }
}

int main() {
    fprintf(stderr, "=== Mission Document Tests ===\n");
    test_document_editing_history();
    test_document_roundtrip();
    test_validation_compile_and_estimate();
    test_geofence_rally_validation();
    test_geofence_compile();
    test_fence_rally_plan_roundtrip();
    test_complex_item_expansion();
    test_ffi_geofence_compile();
    test_rally_compile();
    test_ffi_rally_compile();
    test_parameterized_survey_entities();
    test_ffi_contract();
    test_ffi_survey_entity_roundtrip();
    test_waypoints_interchange();
    test_ffi_waypoints_interchange();
    test_qgc_plan_interchange();
    test_qgc_complex_item_export();
    test_mission_format_recommendation();
    test_vtol_takeoff_and_land_commands();
    test_ffi_qgc_plan_interchange();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
