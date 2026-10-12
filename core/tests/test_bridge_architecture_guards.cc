#include <cstdio>
#include <map>
#include <set>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


static std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    // 读不到文件时明确记一次失败：这些守卫全靠文本匹配，文件改名/搬走后
    // 空字符串会让“不应该包含 X”这类断言悄悄通过，守卫就废了。
    if (!in.is_open()) {
        g_fail++;
        fprintf(stderr, "  [FAIL] cannot open %s (guard would be vacuous)\n",
                path.string().c_str());
        return {};
    }
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
    CHECK(commands.find("MGCORE_API char* mg_onboard_log_request_list") == std::string::npos,
          "log entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_preflight_check") == std::string::npos,
          "preflight entrypoint is not implemented in ffi_commands.cc");
    CHECK(commands.find("MGCORE_API char* mg_battery_failsafe_configure") == std::string::npos,
          "failsafe entrypoints are not implemented in ffi_commands.cc");
    CHECK(commands.size() < 22000,
          "ffi_commands.cc stays below the catch-all size budget");
}

static void test_response_wrappers_are_grouped_by_telemetry_domain() {
    fprintf(stderr, "\n[Group] response wrapper locality\n");
    const auto wrappers = read_file(repo_root() / "bridge/ffi/ffi_telemetry_summary.cc");

    CHECK(wrappers.find("MGCORE_API char* mg_vehicle_get_attitude") == std::string::npos,
          "attitude response entrypoint is not implemented in ffi_telemetry_summary.cc");
    CHECK(wrappers.find("MGCORE_API char* mg_vehicle_get_battery") == std::string::npos,
          "battery response entrypoint is not implemented in ffi_telemetry_summary.cc");
    CHECK(wrappers.find("MGCORE_API char* mg_vehicle_get_prearm_check") == std::string::npos,
          "prearm response entrypoint is not implemented in ffi_telemetry_summary.cc");
    CHECK(wrappers.size() < 5000,
          "ffi_telemetry_summary.cc stays below the response dispatch size budget");
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

static void test_telemetry_cache_delegates_freshness_tracking() {
    fprintf(stderr, "\n[Group] telemetry cache locality\n");
    // 实体已下沉到 core；bridge 下只剩转发头，所以这里必须读 core 路径，
    // 否则断言会在转发头上空转通过。
    const auto telemetry_cache =
        read_file(repo_root() / "core/include/core/telemetry/telemetry_cache.h");

    CHECK(!telemetry_cache.empty(), "telemetry_cache.h was actually read");

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
    const auto serializers =
        read_file(repo_root() / "core/include/core/telemetry/json_serializers.h");

    CHECK(!serializers.empty(), "json_serializers.h was actually read");

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

/// bridge/ffi 下的头文件名。core 只要用裸文件名 include 到其中任何一个，
// 就说明它依赖了上层——而 `bridge/ffi` 正好在 core 的公共 include 路径上，
// 所以这种写法能编译通过，必须由本检查拦住（KD-20）。
static const char* const kBridgeHeaderNames[] = {
    "ffi_api.h", "ffi_api_core.h", "ffi_api_connection.h", "ffi_api_telemetry.h",
    "ffi_api_firmware.h", "ffi_api_params.h", "ffi_api_mission.h",
    "ffi_api_flight.h", "ffi_api_configuration.h", "ffi_api_geofence.h",
    "ffi_api_logging.h", "ffi_api_payload.h", "ffi_api_environment.h",
    "ffi_internal.h", "ffi_core_context.h", "ffi_response.h",
    "ffi_vehicle_session.h", "ffi_mission_bridge.h",
};

/// 返回命中的证据（空串表示没有）。抽成函数是为了能对样本自测：
/// 一个只会漏报的检查比没有检查更危险。
static std::string core_includes_bridge_evidence(const std::string& text) {
    for (const auto* prefix : {"#include \"", "#include <"}) {
        for (const auto* name : kBridgeHeaderNames) {
            const std::string needle = std::string(prefix) + name;
            if (text.find(needle) != std::string::npos) return needle;
        }
    }
    // 显式路径形式（bridge/ffi/xxx.h 或 bridge/xxx.h）同样要拦。
    for (const auto* prefix : {"#include \"", "#include <"}) {
        if (text.find(std::string(prefix) + "bridge/") != std::string::npos) {
            return std::string(prefix) + "bridge/...";
        }
    }
    return {};
}

// ARCH-001：依赖方向必须是 flutter -> bridge -> core -> third_party。
// core 里任何文件都不允许 include bridge/ 下的头文件。
/// A2：core 的模块之间不得形成依赖环。模块 = core/include/core/<module>/。
/// 修复 KD-21 之前存在 firmware -> transport 的回边（firmware_flasher.h 里为了
/// 一个指针参数 include 了 connection_manager.h）。
static bool find_module_cycle(
    const std::string& node,
    const std::map<std::string, std::set<std::string>>& graph,
    std::map<std::string, int>& color,
    std::vector<std::string>& stack,
    std::string& cycle) {
    color[node] = 1;
    stack.push_back(node);
    const auto it = graph.find(node);
    if (it != graph.end()) {
        for (const auto& next : it->second) {
            if (color[next] == 1) {
                std::string path;
                bool started = false;
                for (const auto& step : stack) {
                    if (step == next) started = true;
                    if (started) path += step + " -> ";
                }
                cycle = path + next;
                return true;
            }
            if (color[next] == 0 && find_module_cycle(next, graph, color, stack, cycle)) {
                return true;
            }
        }
    }
    stack.pop_back();
    color[node] = 2;
    return false;
}

static void test_core_modules_have_no_dependency_cycles() {
    fprintf(stderr, "\n[Group] core module dependency graph\n");
    const auto include_root = repo_root() / "core/include/core";

    std::map<std::string, std::set<std::string>> graph;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(include_root)) {
        if (!entry.is_regular_file()) continue;
        const auto extension = entry.path().extension().string();
        if (extension != ".h" && extension != ".hpp") continue;
        const auto relative =
            std::filesystem::relative(entry.path(), include_root);
        const std::string owner = relative.begin()->string();
        graph.try_emplace(owner);

        std::istringstream in(read_file(entry.path()));
        std::string line;
        while (std::getline(in, line)) {
            const auto marker = line.find("#include \"core/");
            if (marker == std::string::npos) continue;
            const std::string rest = line.substr(marker + 15);
            const auto slash = rest.find('/');
            if (slash == std::string::npos) continue;
            const std::string dependency = rest.substr(0, slash);
            if (dependency == owner) continue;
            if (!std::filesystem::is_directory(include_root / dependency)) continue;
            graph[owner].insert(dependency);
        }
    }

    std::map<std::string, int> color;
    std::vector<std::string> stack;
    std::string cycle;
    bool cyclic = false;
    for (const auto& [module, _] : graph) {
        if (color[module] == 0 && find_module_cycle(module, graph, color, stack, cycle)) {
            cyclic = true;
            break;
        }
    }
    if (cyclic) {
        fprintf(stderr, "    core module cycle: %s\n", cycle.c_str());
    }
    CHECK(!cyclic, "core modules form a DAG (no transport/mavlink/firmware cycle)");

    // 方向本身也要对：mavlink 不得反向依赖 transport，firmware 不得依赖 transport。
    CHECK(graph["mavlink"].count("transport") == 0,
          "mavlink does not depend on transport");
    CHECK(graph["firmware"].count("transport") == 0,
          "firmware does not depend on transport");
}

static void test_core_does_not_include_bridge_headers() {
    fprintf(stderr, "\n[Group] core does not depend on bridge\n");
    const auto root = repo_root();

    // 先自测检测器：漏报的检查等于没有检查。
    CHECK(!core_includes_bridge_evidence("#include \"core/vehicle/vehicle.h\"").empty() == false,
          "detector ignores core-owned headers");
    CHECK(!core_includes_bridge_evidence("#include \"ffi_api.h\"").empty(),
          "detector catches a bare bridge header name");
    CHECK(!core_includes_bridge_evidence("#include <bridge/ffi/ffi_api.h>").empty(),
          "detector catches an explicit bridge path");

    std::vector<std::string> offenders;
    // 只检查 core 库本体（include + src）。core/tests 按设计同时引用两侧，
    // 不受依赖方向约束。
    for (const auto* relative_dir : {"core/include", "core/src"}) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root / relative_dir)) {
            if (!entry.is_regular_file()) continue;
            const auto path = entry.path();
            const auto extension = path.extension().string();
            if (extension != ".h" && extension != ".hpp" && extension != ".cc") {
                continue;
            }
            const auto text = read_file(path);
            if (!core_includes_bridge_evidence(text).empty()) {
                offenders.push_back(
                    std::filesystem::relative(path, root).string());
            }
        }
    }

    for (const auto& offender : offenders) {
        fprintf(stderr, "    core includes bridge header: %s\n",
                offender.c_str());
    }
    CHECK(offenders.empty(),
          "no core library file includes a bridge/ header");

    // TelemetryCache 现在归属 core，Vehicle 直接引用 core 路径。
    const auto vehicle = read_file(root / "core/include/core/vehicle/vehicle.h");
    CHECK(vehicle.find("#include \"core/telemetry/telemetry_cache.h\"") !=
              std::string::npos,
          "Vehicle includes the core-owned telemetry cache");
}

int main() {
    fprintf(stderr, "=== Bridge Architecture Guard Tests ===\n");
    test_deep_bridge_modules_do_not_include_internal_grab_bag();
    test_mission_shaping_is_not_implemented_in_ffi_internal();
    test_mission_ffi_entrypoints_have_a_local_module();
    test_catch_all_commands_file_keeps_shrinking();
    test_response_wrappers_are_grouped_by_telemetry_domain();
    test_vehicle_ffi_is_split_by_domain();
    test_telemetry_cache_delegates_freshness_tracking();
    test_json_serializers_stay_focused_on_telemetry_shapes();
    test_ffi_api_is_an_umbrella_not_a_grab_bag();
    test_core_does_not_include_bridge_headers();
    test_core_modules_have_no_dependency_cycles();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
