#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

static std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

static std::filesystem::path repo_root() {
#ifdef MGCORE_TEST_ROOT
    return std::filesystem::path(MGCORE_TEST_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

static void test_deep_bridge_modules_do_not_include_internal_grab_bag() {
    fprintf(stderr, "\n[Group] bridge module independence\n");
    const auto root = repo_root();
    const auto vehicle_session = read_file(root / "bridge/ffi/ffi_vehicle_session.cc");
    const auto mission_bridge = read_file(root / "bridge/ffi/ffi_mission_bridge.cc");

    CHECK(vehicle_session.find("#include \"ffi_internal.h\"") == std::string::npos,
          "vehicle session module does not include ffi_internal.h");
    CHECK(mission_bridge.find("#include \"ffi_internal.h\"") == std::string::npos,
          "mission bridge module does not include ffi_internal.h");
}

static void test_mission_shaping_is_not_implemented_in_ffi_internal() {
    fprintf(stderr, "\n[Group] mission shaping locality\n");
    const auto internal = read_file(repo_root() / "bridge/ffi/ffi_internal.h");

    CHECK(internal.find("ffi_build_ardupilot_home_item") == std::string::npos,
          "ArduPilot home item construction is not implemented in ffi_internal.h");
    CHECK(internal.find("ffi_looks_like_ardupilot_home_item") == std::string::npos,
          "ArduPilot home item detection is not implemented in ffi_internal.h");
}

static void test_mission_ffi_entrypoints_have_a_local_module() {
    fprintf(stderr, "\n[Group] mission FFI locality\n");
    const auto commands = read_file(repo_root() / "bridge/ffi/ffi_commands.cc");

    CHECK(commands.find("MGCORE_API char* mg_mission_upload") == std::string::npos,
          "mission upload entrypoint is not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_mission_get_plan") == std::string::npos,
          "mission plan entrypoint is not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_mission_get_progress") == std::string::npos,
          "mission progress entrypoint is not implemented in ffi_commands.cc");
}

static void test_catch_all_commands_file_keeps_shrinking() {
    fprintf(stderr, "\n[Group] catch-all command file shrinkage\n");
    const auto commands = read_file(repo_root() / "bridge/ffi/ffi_commands.cc");

    CHECK(commands.find("MGCORE_API char* mg_survey_generate") == std::string::npos,
          "survey entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_terrain_query") == std::string::npos,
          "terrain entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_sitl_start") == std::string::npos,
          "SITL entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_camera_take_photo") == std::string::npos,
          "camera entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_log_request_list") == std::string::npos,
          "log entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_preflight_check") == std::string::npos,
          "preflight entrypoint is not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_battery_failsafe_configure") == std::string::npos,
          "failsafe entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.size() < 22000,
          "ffi_commands.cc stays below the catch-all size budget");
}

static void test_v2_wrappers_are_grouped_by_telemetry_domain() {
    fprintf(stderr, "\n[Group] v2 wrapper locality\n");
    const auto wrappers = read_file(repo_root() / "bridge/ffi/ffi_v2_wrappers.cc");

    CHECK(wrappers.find("MGCORE_API char* mg_vehicle_get_attitude") == std::string::npos,
          "attitude v2 entrypoint is not implemented in ffi_v2_wrappers.cc");
    CHECK(wrappers.find("MGCORE_API char* mg_vehicle_get_battery") == std::string::npos,
          "battery v2 entrypoint is not implemented in ffi_v2_wrappers.cc");
    CHECK(wrappers.find("MGCORE_API char* mg_vehicle_get_prearm_check") == std::string::npos,
          "prearm v2 entrypoint is not implemented in ffi_v2_wrappers.cc");
    CHECK(wrappers.size() < 5000,
          "ffi_v2_wrappers.cc stays below the v2 dispatch size budget");
}

static void test_vehicle_ffi_is_split_by_domain() {
    fprintf(stderr, "\n[Group] vehicle FFI locality\n");
    const auto vehicle = read_file(repo_root() / "bridge/ffi/ffi_vehicle.cc");

    CHECK(vehicle.find("MGCORE_API char* mg_sensor_decode_device_id") == std::string::npos,
          "sensor decode entrypoints are not implemented in ffi_vehicle.cc");
    CHECK(vehicle.find("MGCORE_API char* mg_sensors_identify_all") == std::string::npos,
          "sensor identification entrypoints are not implemented in ffi_vehicle.cc");
    CHECK(vehicle.find("MGCORE_API char* mg_vehicle_get_flight_time") == std::string::npos,
          "vehicle info entrypoints are not implemented in ffi_vehicle.cc");
    CHECK(vehicle.find("MGCORE_API char* mg_vehicle_get_adsb_traffic") == std::string::npos,
          "ADSB entrypoints are not implemented in ffi_vehicle.cc");
    CHECK(vehicle.find("MGCORE_API char* mg_vehicle_get_distance_sensors") == std::string::npos,
          "distance sensor entrypoints are not implemented in ffi_vehicle.cc");
    CHECK(vehicle.size() < 7000,
          "ffi_vehicle.cc stays below the vehicle catch-all size budget");
}

static void test_mission_management_stays_focused_on_mission_editing() {
    fprintf(stderr, "\n[Group] mission management locality\n");
    const auto mission_management = read_file(repo_root() / "bridge/ffi/ffi_mission_management.cc");

    CHECK(mission_management.find("MGCORE_API char* mg_flight_get_available_modes") == std::string::npos,
          "flight mode availability entrypoint is not implemented in ffi_mission_management.cc");
    CHECK(mission_management.find("MGCORE_API char* mg_flight_get_mode_status") == std::string::npos,
          "flight mode status entrypoint is not implemented in ffi_mission_management.cc");
    CHECK(mission_management.find("MGCORE_API char* mg_flight_get_mode_history") == std::string::npos,
          "flight mode history entrypoint is not implemented in ffi_mission_management.cc");
    CHECK(mission_management.find("MGCORE_API char* mg_flight_get_takeoff_altitude") == std::string::npos,
          "flight takeoff altitude entrypoint is not implemented in ffi_mission_management.cc");
    CHECK(mission_management.find("MGCORE_API char* mg_flight_get_loiter_radius") == std::string::npos,
          "flight loiter radius entrypoint is not implemented in ffi_mission_management.cc");
    CHECK(mission_management.size() < 11000,
          "ffi_mission_management.cc stays below the mission editing size budget");
}

static void test_telemetry_cache_delegates_freshness_tracking() {
    fprintf(stderr, "\n[Group] telemetry cache locality\n");
    const auto telemetry_cache = read_file(repo_root() / "bridge/ffi/telemetry_cache.h");

    CHECK(telemetry_cache.find("std::map<TelemetryType, uint64_t> last_update_ms_") == std::string::npos,
          "TelemetryCache does not own raw freshness timestamp storage");
    CHECK(telemetry_cache.find("std::chrono::steady_clock::now") == std::string::npos,
          "TelemetryCache does not calculate freshness timestamps inline");
    CHECK(telemetry_cache.find("std::map") == std::string::npos,
          "TelemetryCache no longer needs map machinery for freshness tracking");
    CHECK(telemetry_cache.size() < 17000,
          "telemetry_cache.h stays below the cache locality size budget");
}

static void test_json_serializers_stay_focused_on_telemetry_shapes() {
    fprintf(stderr, "\n[Group] JSON serializer locality\n");
    const auto serializers = read_file(repo_root() / "bridge/ffi/json_serializers.h");

    CHECK(serializers.find("core/mission/mission_types.h") == std::string::npos,
          "core telemetry serializers do not depend on mission domain types");
    CHECK(serializers.find("mission_item_json") == std::string::npos,
          "mission serializers are not implemented in json_serializers.h");
    CHECK(serializers.find("param_detail_json") == std::string::npos,
          "parameter serializers are not implemented in json_serializers.h");
    CHECK(serializers.find("sensor_decode_device_json") == std::string::npos,
          "sensor decode serializers are not implemented in json_serializers.h");
    CHECK(serializers.size() < 19000,
          "json_serializers.h stays below the telemetry serializer size budget");
}

static void test_ffi_api_is_an_umbrella_not_a_grab_bag() {
    fprintf(stderr, "\n[Group] FFI API header locality\n");
    const auto api = read_file(repo_root() / "bridge/ffi/ffi_api.h");

    CHECK(api.find("MGCORE_API char* mg_vehicle_get_summary") == std::string::npos,
          "ffi_api.h does not directly declare vehicle telemetry entrypoints");
    CHECK(api.find("MGCORE_API char* mg_mission_upload") == std::string::npos,
          "ffi_api.h does not directly declare mission entrypoints");
    CHECK(api.find("MGCORE_API char* mg_command_arm") == std::string::npos,
          "ffi_api.h does not directly declare flight command entrypoints");
    CHECK(api.find("#include \"ffi_api_telemetry.h\"") != std::string::npos,
          "ffi_api.h includes the telemetry API module");
    CHECK(api.find("#include \"ffi_api_mission.h\"") != std::string::npos,
          "ffi_api.h includes the mission API module");
    CHECK(api.size() < 1800,
          "ffi_api.h stays small as an umbrella header");
}

int main() {
    fprintf(stderr, "=== Bridge Architecture Guard Tests ===\n");
    test_deep_bridge_modules_do_not_include_internal_grab_bag();
    test_mission_shaping_is_not_implemented_in_ffi_internal();
    test_mission_ffi_entrypoints_have_a_local_module();
    test_catch_all_commands_file_keeps_shrinking();
    test_v2_wrappers_are_grouped_by_telemetry_domain();
    test_vehicle_ffi_is_split_by_domain();
    test_mission_management_stays_focused_on_mission_editing();
    test_telemetry_cache_delegates_freshness_tracking();
    test_json_serializers_stay_focused_on_telemetry_shapes();
    test_ffi_api_is_an_umbrella_not_a_grab_bag();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
