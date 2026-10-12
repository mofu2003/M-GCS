#define MGCORE_EXPORTS

#include "ffi_api.h"
#include "core/mission/cad/mission_document.h"
#include "core/mission/cad/mission_interchange.h"
#include "core/mission/cad/mission_command_catalog.h"

#include "cJSON.h"
#include "core/util/json.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include "mgcs_test_check.h"

using namespace mg::core::mission;
using namespace mg::core::mission::cad;

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


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
    // 半径 0 合法：MAVLink 只规定正值=顺时针 / 负值=逆时针，QGC 的
    // MavCmdInfoCommon.json 给 param3 userMin=-100 且不要求为正；多旋翼更是
    // 完全不读这个字段（ArduCopter do_loiter_unlimited 用 WP_LOITER_RAD，
    // QGC 在 MavCmdInfoMultiRotor.json 对 17 号命令写了 paramRemove:3）。
    loiter.entities[1].params[2] = 0.0f;
    report = MissionDocumentService::validate_document(loiter);
    CHECK(report.valid, "zero loiter radius is accepted (fly-controller default)");
    // 负值表示反向盘旋，同样合法。
    loiter.entities[1].params[2] = -60.0f;
    report = MissionDocumentService::validate_document(loiter);
    CHECK(report.valid, "negative loiter radius selects counter-clockwise");
    // 超出 QGC 的 userMin/userMax 才拒绝。
    loiter.entities[1].params[2] = -500.0f;
    report = MissionDocumentService::validate_document(loiter);
    CHECK(!report.valid, "loiter radius below the catalog minimum is rejected");
    // 非有限值仍然是错误。
    loiter.entities[1].params[2] = std::numeric_limits<float>::quiet_NaN();
    report = MissionDocumentService::validate_document(loiter);
    CHECK(!report.valid, "non-finite loiter radius is rejected");

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

    const auto file_path = std::filesystem::temp_directory_path() /
                            mgcs_test::unique_temp_name("mission_document_test.json");
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

/// 命令 → 实体类型必须覆盖 NAV_WAYPOINT。历史缺陷：该 switch 漏了 case 16，
/// 普通航点被导入成 type="command"，与命令目录对该命令期待的 "waypoint" 冲突，
/// 且校验对 "command" 放行，静默掩盖类型错误。
static void test_entity_type_for_command() {
    fprintf(stderr, "\n[Group] Command to entity type\n");
    CHECK(std::string(mission_entity_type_for_command(
              static_cast<uint16_t>(NavCommand::NavWaypoint))) == "waypoint",
          "NAV_WAYPOINT -> waypoint");
    CHECK(std::string(mission_entity_type_for_command(
              static_cast<uint16_t>(NavCommand::NavTakeoff))) == "takeoff",
          "NAV_TAKEOFF -> takeoff");
    CHECK(std::string(mission_entity_type_for_command(
              static_cast<uint16_t>(NavCommand::NavLand))) == "land",
          "NAV_LAND -> land");
    CHECK(std::string(mission_entity_type_for_command(
              static_cast<uint16_t>(NavCommand::NavReturnToLaunch))) == "rtl",
          "NAV_RETURN_TO_LAUNCH -> rtl");
    CHECK(std::string(mission_entity_type_for_command(
              static_cast<uint16_t>(NavCommand::DoSetSpeed))) == "command",
          "non-navigation command stays 'command'");
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



static void test_qgc_plan_preserves_unknown_fields() {
    fprintf(stderr, "\n[Group] QGC .plan unknown-field round-trip\n");

    const std::string plan = R"({
        "fileType": "Plan",
        "groundStation": "QGroundControl",
        "version": 1,
        "customTopLevel": {"keep": true, "note": "vendor block"},
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "cruiseSpeed": 12,
            "hoverSpeed": 4.5,
            "globalPlanAltitudeMode": 1,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{
                "type": "SimpleItem",
                "frame": 3,
                "command": 16,
                "autoContinue": true,
                "doJumpId": 1,
                "altitudeMode": 1,
                "altitude": 75.5,
                "AMSLAltAboveTerrain": 120.0,
                "vendorTag": "keep-me",
                "params": [0, 0, 0, 0, 39.9, 116.4, 50]
            }]
        }
    })";
    MissionDocument document;
    std::vector<std::string> warnings;
    std::string error;
    CHECK(import_qgc_plan_json(plan, document, warnings, error),
          "QGC plan with unknown fields imports");
    CHECK(document.entities.size() == 1,
          "QGC plan with unknown fields yields one entity");
    if (document.entities.size() == 1) {
        const std::string& extras = document.entities[0].extensions_json;
        CHECK(extras.find("altitudeMode") != std::string::npos &&
                  extras.find("AMSLAltAboveTerrain") != std::string::npos &&
                  extras.find("vendorTag") != std::string::npos,
              "per-item unknown fields are captured on the entity");
        CHECK(extras.find("\"frame\"") == std::string::npos &&
                  extras.find("\"params\"") == std::string::npos &&
                  extras.find("\"doJumpId\"") == std::string::npos,
              "captured fields exclude the keys M-GCS models natively");
    }
    CHECK(document.extensions.count("qgc_plan.mission_fields") == 1 &&
              document.extensions["qgc_plan.mission_fields"].find("hoverSpeed") !=
                  std::string::npos &&
              document.extensions["qgc_plan.mission_fields"]
                      .find("globalPlanAltitudeMode") != std::string::npos,
          "unknown mission fields are preserved");
    CHECK(document.extensions.count("qgc_plan.root_fields") == 1 &&
              document.extensions["qgc_plan.root_fields"].find("customTopLevel") !=
                  std::string::npos,
          "unknown top-level sections are preserved");

    const std::string native = serialize_mission_document(document);
    MissionDocument native_round;
    error.clear();
    CHECK(deserialize_mission_document(native, native_round, error) &&
              native_round.entities.size() == 1 &&
              native_round.entities[0].extensions_json.find("altitudeMode") !=
                  std::string::npos &&
              native_round.extensions.count("qgc_plan.mission_fields") == 1,
          "unknown fields survive the native mission document round-trip");

    warnings.clear();
    error.clear();
    const std::string exported = export_qgc_plan_json(document, warnings, error);
    CHECK(!exported.empty(), "QGC plan with unknown fields exports");
    CHECK(exported.find("\"altitudeMode\"") != std::string::npos &&
              exported.find("\"AMSLAltAboveTerrain\"") != std::string::npos &&
              exported.find("keep-me") != std::string::npos,
          "per-item unknown fields are re-emitted on export");
    CHECK(exported.find("customTopLevel") != std::string::npos,
          "unknown top-level section is re-emitted on export");
    CHECK(exported.find("4.5") != std::string::npos,
          "original hoverSpeed value is re-emitted on export");
    const size_t first_hover = exported.find("\"hoverSpeed\"");
    CHECK(first_hover != std::string::npos &&
              exported.find("\"hoverSpeed\"", first_hover + 1) == std::string::npos,
          "hoverSpeed is emitted exactly once");

    MissionDocument reimport;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(exported, reimport, warnings, error) &&
              reimport.entities.size() == 1 &&
              reimport.entities[0].extensions_json.find("keep-me") !=
                  std::string::npos &&
              reimport.extensions["qgc_plan.mission_fields"].find("hoverSpeed") !=
                  std::string::npos,
          "unknown fields survive export then re-import");

    const std::string v3_plan = R"({
        "fileType": "Plan",
        "mission": {
            "version": 3,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{"type": "SimpleItem", "frame": 3, "command": 16,
                       "autoContinue": true, "doJumpId": 1,
                       "params": [0, 0, 0, 0, 39.9, 116.4, 50]}]
        }
    })";
    MissionDocument v3;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(v3_plan, v3, warnings, error) &&
              v3.entities.size() == 1,
          "unknown QGC mission version still imports");
    bool version_warning = false;
    for (const auto& warning : warnings) {
        if (warning.find("mission version") != std::string::npos) {
            version_warning = true;
        }
    }
    CHECK(version_warning,
          "unknown QGC mission version is reported as a warning");
}


static void test_frame_mission_command_import() {
    fprintf(stderr, "\n[Group] MAV_FRAME_MISSION command import\n");

    // QGC 的 DO_CHANGE_SPEED 使用 MAV_FRAME_MISSION：x/y 是 param5/param6，
    // 不是经纬度。回读飞控任务时必须忽略这两个字段，否则会被画成假航点。
    MissionItem speed_item{};
    speed_item.seq = 0;
    speed_item.frame = 2;  // MAV_FRAME_MISSION
    speed_item.command = static_cast<uint16_t>(NavCommand::DoSetSpeed);
    speed_item.param1 = 0.0f;
    speed_item.param2 = 8.0f;
    speed_item.param3 = 0.0f;
    speed_item.param4 = 0.0f;
    speed_item.x = 123;
    speed_item.y = 456;
    speed_item.z = 7.0f;

    // ArduPilot 可能把不支持坐标的命令统一以 GLOBAL(0) 回读；这时仍应按
    // 命令语义判断，不能把 0,0 画成真实位置。
    MissionItem normalized_item{};
    normalized_item.seq = 1;
    normalized_item.frame = 0;
    normalized_item.command =
        static_cast<uint16_t>(NavCommand::DoSetSpeed);
    normalized_item.param2 = 9.0f;
    normalized_item.x = 0;
    normalized_item.y = 0;
    normalized_item.z = 2.0f;

    MissionPlan transport;
    transport.valid = true;
    transport.items.push_back(speed_item);
    transport.items.push_back(normalized_item);
    MissionDocument imported;
    std::string error;
    CHECK(import_mission_plan_into_document(transport, imported, error),
          "MAV_FRAME_MISSION command imports successfully");
    CHECK(imported.entities.size() == 2,
          "MAV_FRAME_MISSION commands keep their task entities");
    CHECK(imported.entities.size() == 2 &&
              std::isnan(imported.entities[0].latitude) &&
              std::isnan(imported.entities[0].longitude),
          "MAV_FRAME_MISSION x/y are not treated as latitude/longitude");
    CHECK(imported.entities.size() == 2 &&
              imported.entities[0].altitude_m == 7.0f,
          "MAV_FRAME_MISSION z is preserved as param7/altitude storage");
    CHECK(imported.entities.size() == 2 &&
              std::isnan(imported.entities[1].latitude) &&
              std::isnan(imported.entities[1].longitude),
          "GLOBAL frame with a non-location command is not treated as a position");

    const MissionCompileResult compiled =
        MissionDocumentService::compile_document(imported);
    CHECK(compiled.valid && compiled.plan.items.size() == 2,
          "MAV_FRAME_MISSION commands compile without fake positions");
    CHECK(compiled.plan.items.size() == 2 &&
              compiled.plan.items[0].x == 0 && compiled.plan.items[0].y == 0 &&
              compiled.plan.items[1].x == 0 && compiled.plan.items[1].y == 0,
          "non-location commands compile with zero raw param5/param6");

    MissionItem takeoff_item{};
    takeoff_item.frame = 3;
    takeoff_item.command = static_cast<uint16_t>(NavCommand::NavTakeoff);
    takeoff_item.x = 399000000;
    takeoff_item.y = 1164000000;
    takeoff_item.z = 30.0f;
    MissionPlan takeoff_transport;
    takeoff_transport.valid = true;
    takeoff_transport.items.push_back(takeoff_item);
    MissionDocument takeoff_document;
    CHECK(import_mission_plan_into_document(
              takeoff_transport, takeoff_document, error) &&
              takeoff_document.entities.size() == 1 &&
              std::fabs(takeoff_document.entities[0].latitude - 39.9) < 1e-6,
          "fixed-wing NAV_TAKEOFF keeps its geographic position");
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

    const std::string command_plan = R"({
        "fileType": "Plan",
        "mission": {
            "version": 2,
            "firmwareType": 3,
            "vehicleType": 2,
            "plannedHomePosition": [39.9, 116.4, 20],
            "items": [{
                "type": "SimpleItem",
                "frame": 2,
                "command": 205,
                "autoContinue": true,
                "doJumpId": 1,
                "coordinate": [0, 0, 2],
                "params": [0, 0, 0, 0, 0, 0, 2]
            }]
        }
    })";
    MissionDocument command_document;
    warnings.clear();
    error.clear();
    CHECK(import_qgc_plan_json(
              command_plan, command_document, warnings, error),
          "QGC MAV_FRAME_MISSION command imports");
    CHECK(command_document.entities.size() == 1 &&
              std::isnan(command_document.entities[0].latitude) &&
              std::isnan(command_document.entities[0].longitude),
          "QGC MAV_FRAME_MISSION command does not become coordinate 0,0");
    CHECK(command_document.entities.size() == 1 &&
              command_document.entities[0].altitude_m == 2.0f,
          "QGC MAV_FRAME_MISSION param7 is preserved");

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
    CHECK(fence_document_json.find("\"vertices\":[[39.9,116.4]") !=
              std::string::npos,
          "fence polygon vertices serialize as coordinate arrays");
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
}

static void test_mission_format_recommendation() {
    fprintf(stderr, "\n[Group] Mission file format recommendation\n");
    MissionDocument ardupilot = valid_document();
    CHECK(
        recommended_mission_interchange_format(ardupilot) == "waypoints",
        "ArduPilot simple missions recommend waypoints");

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
    // 文档声明 VTOL 机型：能力表把 84/85 限定为 VTOL，默认的 copter 会被拒绝。
    document.metadata.vehicle_type = "vtol";
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
    CHECK(!report_has_code(report, "mission.first_item"),
          "VTOL takeoff is accepted as the first mission item");
    CHECK(!report_has_code(report, "mission.last_item"),
          "VTOL land is accepted as the last mission item");

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

/// ArduPilot 任务命令能力表：命令 × 机型矩阵、自由函数和文档/计划校验。
static void test_mission_vehicle_capability() {
    fprintf(stderr, "\n[Group] Mission vehicle capability table\n");

    using namespace mission_vehicle;

    const auto& catalog = mission_command_catalog();
    const auto vehicles_of = [&catalog](uint16_t command) -> uint16_t {
        for (const auto& entry : catalog) {
            if (entry.command == command) return entry.vehicles;
        }
        return 0xFFFFu;  // 送到目录里不存在时回一个不可能的值，便于断言失败
    };

    // 目录标注：四类机型共享的导航命令、只支持 VTOL 的 84/85。
    const uint16_t all = kCopter | kPlane | kVtol | kRover;
    CHECK(vehicles_of(static_cast<uint16_t>(NavCommand::NavWaypoint)) == all,
          "NAV_WAYPOINT is annotated for copter/plane/vtol/rover");
    CHECK((vehicles_of(static_cast<uint16_t>(NavCommand::NavTakeoff)) & kCopter) != 0 &&
              (vehicles_of(static_cast<uint16_t>(NavCommand::NavTakeoff)) & kRover) == 0,
          "NAV_TAKEOFF excludes rover");
    CHECK((vehicles_of(static_cast<uint16_t>(NavCommand::NavLand)) & kRover) == 0 &&
              (vehicles_of(static_cast<uint16_t>(NavCommand::NavLand)) & kCopter) != 0,
          "NAV_LAND excludes rover");
    CHECK(vehicles_of(static_cast<uint16_t>(NavCommand::NavVtolTakeoff)) == kVtol,
          "NAV_VTOL_TAKEOFF is annotated VTOL only");
    CHECK(vehicles_of(static_cast<uint16_t>(NavCommand::NavVtolLand)) == kVtol,
          "NAV_VTOL_LAND is annotated VTOL only");

    // 自由函数：别名归一、未知不拦、未在目录不拦。
    CHECK(!mission_command_supports_vehicle(21, "rover"),
          "mission_command_supports_vehicle rejects NAV_LAND on rover");
    CHECK(!mission_command_supports_vehicle(21, "Rover"),
          "vehicle class comparison is case-insensitive");
    CHECK(mission_command_supports_vehicle(16, "rover"),
          "mission_command_supports_vehicle accepts NAV_WAYPOINT on rover");
    CHECK(!mission_command_supports_vehicle(84, "FixedWing"),
          "NAV_VTOL_TAKEOFF is rejected on a fixed wing");
    CHECK(mission_command_supports_vehicle(84, "vtol"),
          "NAV_VTOL_TAKEOFF is accepted on a VTOL");
    CHECK(mission_command_supports_vehicle(21, ""),
          "an unknown vehicle class does not reject a command");
    CHECK(mission_command_supports_vehicle(21, "submarine"),
          "an unmodelled vehicle class (Sub) does not reject a command");
    CHECK(mission_command_supports_vehicle(9999, "rover"),
          "a command outside the catalog is left to the catalog check");

    // 文档级校验：Rover 文档里的 NAV_LAND 被拒绝，NAV_WAYPOINT 保留。
    {
        MissionDocument rover = valid_document();
        rover.metadata.vehicle_type = "rover";
        const auto report = MissionDocumentService::validate_document(rover);
        CHECK(!report.valid,
              "a rover document with NAV_LAND fails validation");
        CHECK(report_has_code(report, "entity.command_vehicle_unsupported"),
              "rover document reports entity.command_vehicle_unsupported");
        bool waypoint_flagged = false;
        for (const auto& diagnostic : report.diagnostics) {
            if (diagnostic.code == "entity.command_vehicle_unsupported" &&
                diagnostic.entity_id == "waypoint-1") {
                waypoint_flagged = true;
            }
        }
        CHECK(!waypoint_flagged,
              "NAV_WAYPOINT is not flagged on a rover");
    }

    // 未知固件文档不套用 ArduPilot 能力表。
    {
        MissionDocument unknown = valid_document();
        unknown.metadata.firmware_profile = "unknown";
        unknown.metadata.vehicle_type = "rover";
        const auto report = MissionDocumentService::validate_document(unknown);
        CHECK(!report_has_code(report, "entity.command_vehicle_unsupported"),
              "an unknown-firmware document is not judged by the ArduPilot capability table");
    }

    // NAV_LOITER_UNLIM(17) 按设计不在 13 条命令目录里（docs/mission-capability-table.md:55：
    // 「NAV_LOITER_UNLIM(17) 未单列，其无限圈语义由飞控按 NAV_LOITER_TURNS 处理」），
    // 因此它会被 command_unsupported 拦下——这是既定行为，此处钉住它，
    // 避免有人误以为 17 号命令可以绕过目录直接上传。
    {
        MissionDocument unlim = valid_document();
        unlim.entities[1].type = "loiter";
        unlim.entities[1].command =
            static_cast<uint16_t>(NavCommand::NavLoiterUnlim);
        unlim.entities[1].params = {0.0f, 0.0f, 0.0f, 0.0f};

        const auto report = MissionDocumentService::validate_document(unlim);
        CHECK(!report.valid, "NAV_LOITER_UNLIM is not in the command catalog");
        CHECK(report_has_code(report, "entity.command_unsupported"),
              "NAV_LOITER_UNLIM reports entity.command_unsupported");
    }

    // 目录内的盘旋命令（18/19）接受半径 0 与负值：MAVLink 定义正值=顺时针、
    // 负值=逆时针（common.xml MAV_CMD_NAV_LOITER_*），QGC 的 MavCmdInfoCommon.json
    // 给 param3 userMin=-100 且不要求为正。历史实现把下限写成 0.01，
    // 会拒绝 QGC/MP 导出的合法任务。
    {
        MissionDocument loiter = valid_document();
        loiter.entities[1].type = "loiter";
        loiter.entities[1].command =
            static_cast<uint16_t>(NavCommand::NavLoiterTurns);
        loiter.entities[1].params = {2.0f, 0.0f, 0.0f, 0.0f};

        auto report = MissionDocumentService::validate_document(loiter);
        CHECK(report.valid, "NAV_LOITER_TURNS accepts radius 0");

        loiter.entities[1].params[2] = -80.0f;
        report = MissionDocumentService::validate_document(loiter);
        CHECK(report.valid, "NAV_LOITER_TURNS accepts a negative (CCW) radius");

        loiter.entities[1].params[2] = 100.0f;
        report = MissionDocumentService::validate_document(loiter);
        CHECK(report.valid, "NAV_LOITER_TURNS accepts the QGC upper bound");

        loiter.entities[1].params[2] = -500.0f;
        report = MissionDocumentService::validate_document(loiter);
        CHECK(!report.valid, "a radius outside the QGC range is rejected");
    }

    // 上传前的运行时门禁：直接对已编译计划做机型检查。
    {
        MissionPlan plan;
        plan.items.resize(2);
        plan.items[0].seq = 0;
        plan.items[0].command = static_cast<uint16_t>(NavCommand::NavWaypoint);
        plan.items[1].seq = 1;
        plan.items[1].command = static_cast<uint16_t>(NavCommand::NavLand);

        const auto rover_report = validate_plan_for_vehicle(plan, "rover");
        CHECK(rover_report.has_errors() &&
                  report_has_code(rover_report, "mission.command_vehicle_unsupported"),
              "validate_plan_for_vehicle rejects NAV_LAND on the online rover");

        const auto unknown_report = validate_plan_for_vehicle(plan, "");
        CHECK(unknown_report.diagnostics.empty(),
              "validate_plan_for_vehicle produces no diagnostics for an unknown vehicle");

        const auto copter_report = validate_plan_for_vehicle(plan, "copter");
        CHECK(!copter_report.has_errors(),
              "validate_plan_for_vehicle accepts waypoint+land on a copter");
    }

    // 参数级机型适用性：ArduPilot 按机型复用 15 字节的任务结构
    // （AP_Mission.cpp:1092-1116、:1649-1659），回读时被丢弃的字段不能算差异。
    {
        const uint16_t waypoint = static_cast<uint16_t>(NavCommand::NavWaypoint);

        CHECK(mission_command_parameter_applies(waypoint, 1, "copter") &&
                  mission_command_parameter_applies(waypoint, 1, "rover") &&
                  !mission_command_parameter_applies(waypoint, 1, "plane") &&
                  !mission_command_parameter_applies(waypoint, 1, "vtol"),
              "waypoint hold time is stored on copter/rover but not on plane/VTOL");

        CHECK(!mission_command_parameter_applies(waypoint, 2, "copter") &&
                  !mission_command_parameter_applies(waypoint, 2, "rover") &&
                  mission_command_parameter_applies(waypoint, 2, "plane") &&
                  mission_command_parameter_applies(waypoint, 2, "vtol"),
              "waypoint acceptance radius is stored only on plane/VTOL");

        CHECK(!mission_command_parameter_applies(waypoint, 3, "copter") &&
                  mission_command_parameter_applies(waypoint, 3, "vtol"),
              "waypoint pass radius is stored only on plane/VTOL");

        CHECK(!mission_command_parameter_applies(waypoint, 4, "copter") &&
                  !mission_command_parameter_applies(waypoint, 4, "plane") &&
                  !mission_command_parameter_applies(waypoint, 4, "rover"),
              "no ArduPilot build stores the waypoint yaw field");
        CHECK(mission_command_parameter_applies(waypoint, 4, ""),
              "unknown firmware (unnormalised vehicle class) keeps the waypoint yaw field");

        CHECK(mission_command_parameter_applies(waypoint, 1, "") &&
                  mission_command_parameter_applies(waypoint, 2, ""),
              "an unknown vehicle class does not downgrade any parameter");

        const uint16_t takeoff = static_cast<uint16_t>(NavCommand::NavTakeoff);
        CHECK(mission_command_parameter_applies(takeoff, 1, "copter") &&
                  mission_command_parameter_applies(takeoff, 1, "plane"),
              "parameters without a vehicle annotation apply everywhere");
        CHECK(mission_command_parameter_applies(waypoint, 9, "copter"),
              "a parameter outside the catalog is not judged");
        CHECK(mission_command_parameter_applies(9999, 1, "copter"),
              "a command outside the catalog is not judged");
    }

}

/// RTL（MAV_CMD_NAV_RETURN_TO_LAUNCH = 20）的高度必须参与校验：
/// mission_compile.cc:188-190 无条件把 entity.altitude_m 写进任务项（只有非有限值
/// 才回落默认高度），所以"有限但非法"的高度（负数）会原样下发。旧实现里
/// position_command() 不含 RTL，三项校验被整体跳过。
/// 坐标则只在"确实给了"时校验：AP_Mission::stored_in_location() 不含 RTL，
/// 从飞控下载回来的 RTL 坐标是 NaN（mission_document.cc 同名判断），把"没给坐标"
/// 当成错误会误报每一份正常下载的任务。
static void test_rtl_altitude_and_coordinates_are_validated() {
    fprintf(stderr, "\n[Group] RTL altitude and coordinate validation\n");

    auto make_rtl_document = []() {
        MissionDocument document = valid_document();
        MissionEntity rtl;
        rtl.id = "rtl-1";
        rtl.type = "rtl";
        rtl.command = static_cast<uint16_t>(NavCommand::NavReturnToLaunch);
        rtl.altitude_m = 50.0f;
        document.entities.push_back(rtl);
        return document;
    };

    // 基线：没有坐标的 RTL 合法——这正是从飞控下载回来的形态。
    {
        MissionDocument document = make_rtl_document();
        CHECK(std::isnan(document.entities.back().latitude) &&
                  std::isnan(document.entities.back().longitude),
              "the RTL fixture really carries no coordinate (NaN)");
        auto report = MissionDocumentService::validate_document(document);
        CHECK(report.valid, "RTL without coordinates stays valid");
    }

    // 验收点：有限但非法的高度必须被拒绝。
    {
        MissionDocument document = make_rtl_document();
        document.entities.back().altitude_m = -5.0f;
        auto report = MissionDocumentService::validate_document(document);
        CHECK(!report.valid, "negative RTL altitude fails validation");
        CHECK(report_has_code(report, "entity.altitude"),
              "negative RTL altitude reports entity.altitude");
        bool blamed_rtl = false;
        for (const auto& diagnostic : report.diagnostics) {
            if (diagnostic.code == "entity.altitude" &&
                diagnostic.entity_id == "rtl-1") {
                blamed_rtl = true;
            }
        }
        CHECK(blamed_rtl, "the altitude diagnostic points at the RTL entity");
    }

    // 编译口径与校验口径一致：RTL 的高度会进任务项，所以非法值必须挡在编译之前。
    {
        MissionDocument document = make_rtl_document();
        auto compiled = MissionDocumentService::compile_document(document);
        CHECK(compiled.valid && !compiled.plan.items.empty() &&
                  compiled.plan.items.back().z == 50.0f,
              "a legal RTL altitude is written into the compiled plan");

        document.entities.back().altitude_m = -5.0f;
        auto rejected = MissionDocumentService::compile_document(document);
        CHECK(!rejected.valid && rejected.plan.items.empty(),
              "a negative RTL altitude is rejected before it reaches the plan");
    }

    // 提供了却越界的坐标必须被拒绝。
    {
        MissionDocument document = make_rtl_document();
        document.entities.back().latitude = 120.0;
        document.entities.back().longitude = 116.0;
        auto report = MissionDocumentService::validate_document(document);
        CHECK(!report.valid, "out-of-range RTL latitude fails validation");
        CHECK(report_has_code(report, "entity.latitude"),
              "out-of-range RTL latitude reports entity.latitude");
    }
    {
        MissionDocument document = make_rtl_document();
        document.entities.back().latitude = 39.9;
        document.entities.back().longitude = 200.0;
        auto report = MissionDocumentService::validate_document(document);
        CHECK(!report.valid, "out-of-range RTL longitude fails validation");
        CHECK(report_has_code(report, "entity.longitude"),
              "out-of-range RTL longitude reports entity.longitude");
    }
}

int main() {
    fprintf(stderr, "=== Mission Document Tests ===\n");
    test_document_editing_history();
    test_document_roundtrip();
    test_validation_compile_and_estimate();
    test_rtl_altitude_and_coordinates_are_validated();
    test_geofence_rally_validation();
    test_geofence_compile();
    test_fence_rally_plan_roundtrip();
    test_ffi_geofence_compile();
    test_rally_compile();
    test_ffi_rally_compile();
    test_ffi_contract();
    test_waypoints_interchange();
    test_entity_type_for_command();
    test_ffi_waypoints_interchange();
    test_frame_mission_command_import();
    test_qgc_plan_interchange();
    test_qgc_plan_preserves_unknown_fields();
    test_mission_format_recommendation();
    test_vtol_takeoff_and_land_commands();
    test_mission_vehicle_capability();
    test_ffi_qgc_plan_interchange();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
