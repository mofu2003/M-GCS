/**
 * test_sim_params.cc - Parameter protocol simulation tests
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(msg, cond) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while (0)

struct TestEnv : MockVehicleTestEnv {
    explicit TestEnv(MockFlightController::Config cfg = {})
        : MockVehicleTestEnv(cfg, true) {}
};

static int parse_progress_received(const char* json) {
    const char* p = strstr(json, "\"received\":");
    if (!p) return -1;
    return atoi(p + 11);
}

static int parse_progress_total(const char* json) {
    const char* p = strstr(json, "\"total\":");
    if (!p) return -1;
    return atoi(p + 8);
}

static float parse_param_value(const char* json) {
    const char* p = strstr(json, "\"value\":");
    if (!p) return 0.0f;
    return static_cast<float>(atof(p + 8));
}

static void inject_param_value(TestEnv& env,
                               uint8_t sysid,
                               uint8_t compid,
                               const std::string& name,
                               float value,
                               uint8_t type,
                               uint16_t count,
                               uint16_t index) {
    char param_id[16] = {};
    const size_t copy_len = name.size() < sizeof(param_id) - 1
        ? name.size()
        : sizeof(param_id) - 1;
    std::memcpy(param_id, name.data(), copy_len);

    mavlink_message_t message;
    mavlink_msg_param_value_pack(sysid, compid, &message,
                                 param_id, value, type, count, index);
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN] = {};
    const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
    env.link->inject_incoming(buffer, length);
}

static bool diagnostics_has_state(TestEnv& env, const char* state) {
    char* diagnostics = mg_system_get_diagnostics(env.core);
    const bool found = diagnostics != nullptr && strstr(diagnostics, state) != nullptr;
    if (diagnostics) mg_free_string(diagnostics);
    return found;
}

static bool load_all_params(TestEnv& env);

static void test_param_value_write_validation() {
    fprintf(stderr, "\n[Group] Parameter write reply validation\n");
    TestEnv env;
    env.tick(1100);
    if (!load_all_params(env)) return;

    // Drop the mock's automatic echo so the test can inject controlled
    // PARAM_VALUE replies.
    env.mock->set_param_drop(1);
    char* set_res = mg_params_set(env.core, "WPNAV_SPEED", 10.0f, 0);
    CHECK("controlled write request is sent",
          set_res != nullptr && strstr(set_res, "true") != nullptr);
    if (set_res) mg_free_string(set_res);
    env.tick(50);

    inject_param_value(env, 2, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("reply from another system does not confirm write",
          diagnostics_has_state(env, "\"state\":\"pending\""));

    inject_param_value(env, 1, 2, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("reply from another component does not confirm write",
          diagnostics_has_state(env, "\"state\":\"pending\""));

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_INT32, 36, 0);
    CHECK("reply with a different parameter type does not confirm write",
          diagnostics_has_state(env, "\"state\":\"pending\""));

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 9.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("same-name reply with wrong value does not confirm write",
          diagnostics_has_state(env, "\"state\":\"pending\""));

    char* cached = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    CHECK("mismatched replies do not update the active cache",
          cached != nullptr && parse_param_value(cached) > 4.9f &&
              parse_param_value(cached) < 5.1f);
    if (cached) mg_free_string(cached);

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("matching source, component, type and value confirms write",
          diagnostics_has_state(env, "\"state\":\"confirmed\""));

    // A delayed reply from an older request generation must not confirm a
    // newer write to the same parameter.
    env.mock->set_param_drop(2);
    char* first = mg_params_set(env.core, "WPNAV_SPEED", 11.0f, 0);
    if (first) mg_free_string(first);
    env.tick(50);
    char* second = mg_params_set(env.core, "WPNAV_SPEED", 12.0f, 0);
    CHECK("replacement write request is sent",
          second != nullptr && strstr(second, "true") != nullptr);
    if (second) mg_free_string(second);
    env.tick(50);

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 11.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("stale-generation value does not confirm replacement write",
          diagnostics_has_state(env, "\"state\":\"pending\""));

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 12.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("current generation value confirms replacement write",
          diagnostics_has_state(env, "\"state\":\"confirmed\""));
}

static void test_write_isolated_by_new_download() {
    fprintf(stderr, "\n[Group] Parameter write isolation across a new download\n");
    TestEnv env;
    env.tick(1100);
    if (!load_all_params(env)) return;

    // Hold the echo so the write is still pending when the download restarts.
    env.mock->set_param_drop(1);
    char* set_res = mg_params_set(env.core, "WPNAV_SPEED", 10.0f, 0);
    CHECK("pending write request is sent",
          set_res != nullptr && strstr(set_res, "true") != nullptr);
    if (set_res) mg_free_string(set_res);
    env.tick(50);
    CHECK("write is pending before the download restarts",
          diagnostics_has_state(env, "\"state\":\"pending\""));

    // A reconnect or forced refresh begins a new download. The in-flight write
    // must be failed against the old session instead of crossing into the new
    // one, where a same-name PARAM_VALUE could otherwise confirm it.
    char* restart = mg_params_request_all(env.core);
    CHECK("new download request is sent after the link restarts",
          restart != nullptr && strstr(restart, "\"success\":true") != nullptr);
    if (restart) mg_free_string(restart);
    env.tick(50);
    CHECK("in-flight write is failed by the new download",
          diagnostics_has_state(
              env, "parameter write cancelled by a new parameter download"));
    CHECK("cancelled write is no longer pending",
          !diagnostics_has_state(env, "\"state\":\"pending\""));

    // A late echo from the old request must not resurrect a confirmed state.
    inject_param_value(env, 1, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK("late echo after restart does not confirm the cancelled write",
          !diagnostics_has_state(env, "\"state\":\"confirmed\""));
}

static void test_download_index_completion_and_source_filter() {
    fprintf(stderr, "\n[Group] Parameter download index and source filtering\n");
    TestEnv env;
    env.tick(1100);
    env.mock->set_param_list_no_response(true);

    char* request = mg_params_request_all(env.core);
    CHECK("indexed download request is sent",
          request != nullptr && strstr(request, "\"success\":true") != nullptr);
    if (request) mg_free_string(request);
    env.tick(50);

    inject_param_value(env, 2, 1, "WRONG_SYSTEM", 1.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 0);
    char* wrong = mg_params_get_by_name(env.core, "WRONG_SYSTEM");
    CHECK("other system parameter is not cached by active download",
          wrong != nullptr && strstr(wrong, "\"found\":false") != nullptr);
    if (wrong) mg_free_string(wrong);

    inject_param_value(env, 1, 2, "WRONG_COMPONENT", 1.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 0);
    char* progress = mg_params_get_progress(env.core);
    CHECK("other component parameter does not advance download",
          parse_progress_received(progress) == 0 && parse_progress_total(progress) == 0);
    if (progress) mg_free_string(progress);

    inject_param_value(env, 1, 1, "IDX0", 1.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 0);
    inject_param_value(env, 1, 1, "IDX2", 3.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 2);
    inject_param_value(env, 1, 1, "IDX2_DUPLICATE", 3.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 2);

    progress = mg_params_get_progress(env.core);
    CHECK("duplicate index does not complete a missing-index download",
          parse_progress_received(progress) == 2 && parse_progress_total(progress) == 3);
    if (progress) mg_free_string(progress);
    CHECK("download remains active until index 1 arrives",
          env.mav->param_download_active());

    inject_param_value(env, 1, 1, "IDX1", 2.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 1);
    CHECK("download completes after all 0..count-1 indices arrive",
          !env.mav->param_download_active() && !env.mav->param_download_failed());
}

static void test_refresh_source_filter() {
    fprintf(stderr, "\n[Group] Parameter refresh source filtering\n");
    TestEnv env;
    env.tick(1100);
    if (!load_all_params(env)) return;

    env.mock->set_param_drop(1);
    char* request = mg_params_refresh(env.core, "[\"WPNAV_SPEED\"]");
    CHECK("refresh request is sent",
          request != nullptr && strstr(request, "\"success\":true") != nullptr);
    if (request) mg_free_string(request);
    env.tick(50);

    char* status = mg_params_get_refresh_status(env.core);
    CHECK("refresh waits for its target reply",
          status != nullptr && strstr(status, "\"active\":true") != nullptr &&
              strstr(status, "\"pending\":1") != nullptr);
    if (status) mg_free_string(status);

    inject_param_value(env, 2, 1, "WPNAV_SPEED", 5.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    status = mg_params_get_refresh_status(env.core);
    CHECK("reply from another system does not complete refresh",
          status != nullptr && strstr(status, "\"active\":true") != nullptr);
    if (status) mg_free_string(status);

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 5.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    status = mg_params_get_refresh_status(env.core);
    CHECK("matching target reply completes refresh",
          status != nullptr && strstr(status, "\"active\":false") != nullptr &&
              strstr(status, "\"completed\":1") != nullptr);
    if (status) mg_free_string(status);
}

static void test_full_download() {
    fprintf(stderr, "\n[Group] Full parameter download\n");
    TestEnv env;

    env.tick(1100);

    char* res = mg_params_request_all(env.core);
    CHECK("request_all returns success", res != nullptr && strstr(res, "true") != nullptr);
    mg_free_string(res);

    int received = 0;
    int total = 0;
    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        received = parse_progress_received(prog);
        total = parse_progress_total(prog);
        mg_free_string(prog);
        if (total > 0 && received >= total) break;
    }

    CHECK("total > 0", total > 0);
    CHECK("total >= 25", total >= 25);
    CHECK("received == total (download complete)", total > 0 && received >= total);

    fprintf(stderr, "  (params: received=%d total=%d)\n", received, total);
}

static void test_cancel_download() {
    fprintf(stderr, "\n[Group] Parameter download cancellation\n");
    TestEnv env;
    env.tick(1100);

    char* res = mg_params_request_all(env.core);
    CHECK("cancel test request starts",
          res != nullptr && strstr(res, "\"success\":true") != nullptr);
    if (res) mg_free_string(res);

    char* cancel = mg_operations_cancel(env.core, "params");
    CHECK("active parameter download can be cancelled through operations",
          cancel != nullptr && strstr(cancel, "\"canceled\":true") != nullptr);
    if (cancel) mg_free_string(cancel);

    char* snapshot = mg_operations_get_snapshot(env.core);
    CHECK("parameter operation reports cancelled terminal state",
          snapshot != nullptr && strstr(snapshot, "\"state\":\"cancelled\"") != nullptr);
    CHECK("parameter cancellation keeps its reason",
          snapshot != nullptr && strstr(snapshot, "parameter download cancelled") != nullptr);
    if (snapshot) mg_free_string(snapshot);

    res = mg_params_request_all(env.core);
    CHECK("parameter download can restart after cancellation",
          res != nullptr && strstr(res, "\"success\":true") != nullptr);
    if (res) mg_free_string(res);

    int received = 0;
    int total = 0;
    for (int i = 0; i < 50; ++i) {
        env.tick(50);
        char* progress = mg_params_get_progress(env.core);
        received = parse_progress_received(progress);
        total = parse_progress_total(progress);
        if (progress) mg_free_string(progress);
        if (total > 0 && received >= total) break;
    }
    CHECK("parameter download completes after restart", total > 0 && received >= total);
}

static void test_param_get() {
    fprintf(stderr, "\n[Group] Parameter get by name\n");
    TestEnv env;

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    char* json = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    CHECK("get WPNAV_SPEED not null", json != nullptr);
    CHECK("get WPNAV_SPEED found", strstr(json, "\"found\":true") != nullptr);

    const char* vp = strstr(json, "\"value\":");
    CHECK("value field present", vp != nullptr);
    if (vp) {
        const float val = static_cast<float>(atof(vp + 8));
        CHECK("WPNAV_SPEED == 5.0", val > 4.9f && val < 5.1f);
        fprintf(stderr, "  (WPNAV_SPEED = %g)\n", val);
    }
    mg_free_string(json);

    char* notfound = mg_params_get_by_name(env.core, "NONEXISTENT_PARAM");
    CHECK("not found param returns found:false", strstr(notfound, "\"found\":false") != nullptr);
    mg_free_string(notfound);
}

static void test_param_set() {
    fprintf(stderr, "\n[Group] Parameter set\n");
    TestEnv env;

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    env.mock->received_params_set.clear();

    char* set_res = mg_params_set(env.core, "WPNAV_SPEED", 10.0f, 0);
    CHECK("set returns success", set_res != nullptr && strstr(set_res, "true") != nullptr);
    mg_free_string(set_res);

    env.tick(100);

    bool found = false;
    for (const auto& name : env.mock->received_params_set) {
        if (name == "WPNAV_SPEED") {
            found = true;
            break;
        }
    }
    CHECK("mock received WPNAV_SPEED set", found);

    char* json = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    const char* vp = strstr(json, "\"value\":");
    if (vp) {
        const float val = static_cast<float>(atof(vp + 8));
        CHECK("WPNAV_SPEED updated to 10.0", val > 9.9f && val < 10.1f);
        fprintf(stderr, "  (WPNAV_SPEED = %g after set)\n", val);
    }
    mg_free_string(json);

    char* diagnostics = mg_system_get_diagnostics(env.core);
    CHECK("system diagnostics exposes parameter_history",
          diagnostics != nullptr && strstr(diagnostics, "\"parameter_history\"") != nullptr);
    CHECK("parameter history records the requested parameter",
          diagnostics != nullptr && strstr(diagnostics, "\"name\":\"WPNAV_SPEED\"") != nullptr);
    CHECK("parameter history marks a readback as confirmed",
          diagnostics != nullptr && strstr(diagnostics, "\"state\":\"confirmed\"") != nullptr);
    if (diagnostics) mg_free_string(diagnostics);
}

static void test_config_write_rejects_read_only() {
    fprintf(stderr, "\n[Group] Config parameter metadata validation\n");
    TestEnv env;

    env.tick(1100);
    env.mock->received_params_set.clear();

    char* single = mg_config_write_param(env.core, "BARO1_DEVID", 1.0f);
    CHECK("single read-only write is rejected",
          single != nullptr && strstr(single, "read-only") != nullptr);
    CHECK("single read-only write sends no PARAM_SET",
          env.mock->received_params_set.empty());
    if (single) mg_free_string(single);

    const char* batch =
        "[{\"name\":\"WPNAV_SPEED\",\"value\":8.0},"
        "{\"name\":\"BARO1_DEVID\",\"value\":1.0}]";
    char* batch_result = mg_config_write_params(env.core, batch);
    CHECK("batch write reports one valid and one failed item",
          batch_result != nullptr && strstr(batch_result, "\"written\":1") != nullptr &&
              strstr(batch_result, "\"failed\":1") != nullptr);
    CHECK("batch write exposes metadata failure",
          batch_result != nullptr && strstr(batch_result, "read-only") != nullptr);
    env.tick(1);
    CHECK("batch write only sends the valid PARAM_SET",
          env.mock->received_params_set.size() == 1 &&
              env.mock->received_params_set.front() == "WPNAV_SPEED");
    if (batch_result) mg_free_string(batch_result);
}

static void test_bulk_param_refresh() {
    fprintf(stderr, "\n[Group] Bulk parameter refresh\n");
    TestEnv env;

    env.tick(1100);
    char* load = mg_params_request_all(env.core);
    if (load) mg_free_string(load);
    if (!load_all_params(env)) return;

    env.mock->received_param_reads.clear();
    char* result = mg_params_refresh(
        env.core,
        "[\"WPNAV_SPEED\",\"WPNAV_*\",\"WPNAV_SPEED\",\"DOES_NOT_EXIST\"]");
    env.tick(1);
    CHECK("bulk refresh resolves exact names and prefixes",
          result != nullptr && strstr(result, "\"resolved\":3") != nullptr);
    CHECK("bulk refresh reports unknown names",
          result != nullptr && strstr(result, "\"DOES_NOT_EXIST\"") != nullptr);
    CHECK("bulk refresh reports duplicate requests",
          result != nullptr && strstr(result, "\"duplicates\":2") != nullptr);
    CHECK("bulk refresh sends each resolved name once",
          env.mock->received_param_reads.size() == 3);
    if (result) mg_free_string(result);
    char* status = mg_params_get_refresh_status(env.core);
    CHECK("bulk refresh status completes after PARAM_VALUE replies",
          status != nullptr && strstr(status, "\"active\":false") != nullptr &&
              strstr(status, "\"completed\":3") != nullptr);
    if (status) mg_free_string(status);
}

static void test_unclassified_parameter_group() {
    fprintf(stderr, "\n[Group] Unclassified parameter metadata fallback\n");
    TestEnv env;
    env.mock->add_param("MGC_UNKNOWN", 42.0f, MAV_PARAM_TYPE_REAL32);

    env.tick(1100);
    char* load = mg_params_request_all(env.core);
    if (load) mg_free_string(load);
    if (!load_all_params(env)) return;

    char* groups = mg_config_get_groups(env.core);
    CHECK("unclassified parameter group is exposed",
          groups != nullptr && strstr(groups, "\"name\":\"other\"") != nullptr);
    if (groups) mg_free_string(groups);

    char* descriptor = mg_config_get_group_descriptor(env.core, "other");
    CHECK("unclassified descriptor keeps raw parameter name",
          descriptor != nullptr && strstr(descriptor, "\"param_name\":\"MGC_UNKNOWN\"") != nullptr);
    CHECK("unclassified descriptor marks missing metadata",
          descriptor != nullptr && strstr(descriptor, "\"found\":false") != nullptr);
    CHECK("unclassified descriptor infers MAVLink type",
          descriptor != nullptr && strstr(descriptor, "\"type\":\"float\"") != nullptr);
    if (descriptor) mg_free_string(descriptor);

    char* values = mg_config_read_group(env.core, "other");
    CHECK("unclassified group carries live value",
          values != nullptr && strstr(values, "\"value\":42") != nullptr);
    if (values) mg_free_string(values);

    char* coverage = mg_config_get_metadata_coverage(env.core);
    CHECK("metadata coverage reports loaded parameters",
          coverage != nullptr && strstr(coverage, "\"total\":36") != nullptr);
    CHECK("metadata coverage reports fallback and missing name",
          coverage != nullptr && strstr(coverage, "\"fallback\":1") != nullptr &&
              strstr(coverage, "\"MGC_UNKNOWN\"") != nullptr);
    if (coverage) mg_free_string(coverage);
}

static void test_metadata_profile_for_rover() {
    fprintf(stderr, "\n[Group] Vehicle parameter profile mapping\n");
    MockFlightController::Config config;
    config.vehicle = MockFlightController::Rover;
    TestEnv env(config);

    env.tick(1100);
    char* load = mg_params_request_all(env.core);
    if (load) mg_free_string(load);
    if (!load_all_params(env)) return;

    char* coverage = mg_config_get_metadata_coverage(env.core);
    CHECK("Rover uses an explicit parameter profile",
          coverage != nullptr && strstr(coverage, "\"profile\":\"rover\"") != nullptr);
    if (coverage) mg_free_string(coverage);
}

static void test_config_uses_ardupilot_profile_metadata() {
    fprintf(stderr, "\n[Group] Config metadata profile\n");
    TestEnv env;

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    char* groups = mg_config_get_groups(env.core);
    CHECK("copter metadata exposes BATT group", strstr(groups, "\"name\":\"BATT\"") != nullptr);
    mg_free_string(groups);

    char* descriptor = mg_config_get_group_descriptor(env.core, "BATT");
    CHECK("BATT_MONITOR official display name",
          strstr(descriptor, "\"display_name\":\"电池监测器\"") != nullptr);
    CHECK("BATT_MONITOR bilingual enum option",
          strstr(descriptor, "模拟电压和电流") != nullptr &&
          strstr(descriptor, "Analog Voltage and Current") != nullptr);
    CHECK("BATT_MONITOR enum option keeps explicit English field",
          strstr(descriptor,
                 "\"english_label\":\"Analog Voltage and Current\"") != nullptr);
    mg_free_string(descriptor);

    char* values = mg_config_read_group(env.core, "BATT");
    CHECK("BATT_MONITOR bilingual live display value",
          strstr(values, "模拟电压和电流") != nullptr &&
          strstr(values, "Analog Voltage and Current") != nullptr);
    CHECK("BATT_MONITOR live value exposes English field",
          strstr(values,
                 "\"display_value_en\":\"Analog Voltage and Current\"") != nullptr);
    mg_free_string(values);
}

static void test_geofence_get_uses_loaded_ardupilot_params() {
    fprintf(stderr, "\n[Group] Geofence config from parameters\n");
    TestEnv env;

    env.mock->add_param("FENCE_ENABLE", 1, 2);
    env.mock->add_param("FENCE_TYPE", 7, 2);
    env.mock->add_param("FENCE_ACTION", 1, 2);
    env.mock->add_param("FENCE_ALT_MAX", 100, 9);
    env.mock->add_param("FENCE_ALT_MIN", -10, 9);
    env.mock->add_param("FENCE_MARGIN", 2, 9);

    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int r = parse_progress_received(prog);
        const int t = parse_progress_total(prog);
        mg_free_string(prog);
        if (t > 0 && r >= t) break;
    }

    char* fence = mg_geofence_get_config(env.core);
    CHECK("FENCE_ENABLE maps to enabled true", strstr(fence, "\"enabled\":true") != nullptr);
    CHECK("FENCE_ALT_MAX maps to max_altitude 100", strstr(fence, "\"max_altitude\":100") != nullptr);
    CHECK("FENCE_ALT_MIN maps to min_altitude -10", strstr(fence, "\"min_altitude\":-10") != nullptr);
    CHECK("FENCE_ACTION maps to action 1", strstr(fence, "\"action\":1") != nullptr);
    CHECK("FENCE_MARGIN maps to breach_distance 2", strstr(fence, "\"breach_distance\":2") != nullptr);
    mg_free_string(fence);
}

static bool load_all_params(TestEnv& env) {
    env.tick(1100);
    char* res = mg_params_request_all(env.core);
    if (res) mg_free_string(res);

    for (int i = 0; i < 50; i++) {
        env.tick(50);
        char* prog = mg_params_get_progress(env.core);
        const int received = parse_progress_received(prog);
        const int total = parse_progress_total(prog);
        if (prog) mg_free_string(prog);
        if (total > 0 && received >= total) return true;
    }
    return false;
}

static void test_change_set_requires_loaded_params() {
    fprintf(stderr, "\n[Group] Change set requires loaded params\n");
    TestEnv env;
    env.tick(1100);

    const char* request = "[{\"name\":\"WPNAV_SPEED\",\"value\":6}]";

    char* prepared = mg_params_prepare_change_set(env.core, request);
    CHECK("prepare rejects before params load",
          prepared != nullptr && strstr(prepared, "\"success\":false") != nullptr);
    if (prepared) mg_free_string(prepared);

    char* committed = mg_params_commit_change_set(env.core, request);
    CHECK("commit rejects before params load",
          committed != nullptr && strstr(committed, "\"success\":false") != nullptr);
    if (committed) mg_free_string(committed);
}

static void test_change_set_rejects_malformed_json() {
    fprintf(stderr, "\n[Group] Change set malformed JSON\n");
    TestEnv env;
    CHECK("params loaded for malformed json", load_all_params(env));

    char* not_array = mg_params_prepare_change_set(env.core, "{\"name\":\"A\"}");
    CHECK("prepare rejects non-array json",
          not_array != nullptr && strstr(not_array, "\"success\":false") != nullptr);
    if (not_array) mg_free_string(not_array);

    char* missing_value = mg_params_prepare_change_set(
        env.core, "[{\"name\":\"WPNAV_SPEED\"}]");
    CHECK("prepare rejects entry without numeric value",
          missing_value != nullptr &&
              strstr(missing_value, "\"success\":false") != nullptr);
    if (missing_value) mg_free_string(missing_value);
}

static void test_change_set_planning() {
    fprintf(stderr, "\n[Group] Change set planning\n");
    TestEnv env;
    CHECK("params loaded for planning", load_all_params(env));

    // 重名取最后一次，未知参数与未变化参数分别计数。
    const char* request =
        "[{\"name\":\"WPNAV_SPEED\",\"value\":6},"
        "{\"name\":\"WPNAV_SPEED\",\"value\":7},"
        "{\"name\":\"NOT_A_REAL_PARAM\",\"value\":1},"
        "{\"name\":\"WPNAV_RADIUS\",\"value\":2}]";
    char* json = mg_params_prepare_change_set(env.core, request);
    CHECK("duplicates collapse to one ready change",
          json != nullptr && strstr(json, "\"ready\":1") != nullptr);
    CHECK("unchanged change counted",
          json != nullptr && strstr(json, "\"unchanged\":1") != nullptr);
    CHECK("unknown change counted",
          json != nullptr && strstr(json, "\"unknown\":1") != nullptr);
    CHECK("last duplicate value wins",
          json != nullptr && strstr(json, "\"value\":7") != nullptr);
    if (json) mg_free_string(json);
}

static void test_change_set_commit_orders_enable_last() {
    fprintf(stderr, "\n[Group] Change set commit ordering\n");
    TestEnv env;
    CHECK("params loaded for commit", load_all_params(env));
    env.mock->received_params_set.clear();

    const char* request =
        "[{\"name\":\"COMPASS_ENABLE\",\"value\":0},"
        "{\"name\":\"WPNAV_SPEED\",\"value\":8}]";
    char* json = mg_params_commit_change_set(env.core, request);
    CHECK("both ready changes written",
          json != nullptr && strstr(json, "\"written\":2") != nullptr);
    CHECK("commit success when everything resolved",
          json != nullptr && strstr(json, "\"success\":true") != nullptr);
    if (json) mg_free_string(json);

    env.tick(100);

    int speed_index = -1;
    int enable_index = -1;
    for (size_t i = 0; i < env.mock->received_params_set.size(); i++) {
        if (env.mock->received_params_set[i] == "WPNAV_SPEED") {
            speed_index = static_cast<int>(i);
        }
        if (env.mock->received_params_set[i] == "COMPASS_ENABLE") {
            enable_index = static_cast<int>(i);
        }
    }
    CHECK("both PARAM_SET reached the flight controller",
          speed_index >= 0 && enable_index >= 0);
    CHECK("_ENABLE parameter is written after its dependents",
          speed_index >= 0 && enable_index > speed_index);

    char* unknown_only = mg_params_commit_change_set(
        env.core, "[{\"name\":\"NOT_A_REAL_PARAM\",\"value\":1}]");
    CHECK("commit reports failure when a change is unresolved",
          unknown_only != nullptr &&
              strstr(unknown_only, "\"success\":false") != nullptr);
    if (unknown_only) mg_free_string(unknown_only);
}

int main() {
    fprintf(stderr, "=== Parameter Simulation Tests ===\n");
    test_full_download();
    test_download_index_completion_and_source_filter();
    test_param_value_write_validation();
    test_write_isolated_by_new_download();
    test_refresh_source_filter();
    test_cancel_download();
    test_param_get();
    test_param_set();
    test_config_write_rejects_read_only();
    test_bulk_param_refresh();
    test_unclassified_parameter_group();
    test_metadata_profile_for_rover();
    test_change_set_requires_loaded_params();
    test_change_set_rejects_malformed_json();
    test_change_set_planning();
    test_change_set_commit_orders_enable_last();
    test_config_uses_ardupilot_profile_metadata();
    test_geofence_get_uses_loaded_ardupilot_params();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
