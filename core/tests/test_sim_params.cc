/**
 * test_sim_params.cc - Parameter protocol simulation tests
 */
#include "mock_vehicle_test_env.h"
#include "ffi_api.h"

#include <cstdio>
#include <cstring>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();


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
    char* set_res = mg_config_write_param(env.core, "WPNAV_SPEED", 10.0f);
    CHECK(set_res != nullptr && strstr(set_res, "true") != nullptr, "controlled write request is sent");
    if (set_res) mg_free_string(set_res);
    env.tick(50);

    inject_param_value(env, 2, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"pending\""), "reply from another system does not confirm write");

    inject_param_value(env, 1, 2, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"pending\""), "reply from another component does not confirm write");

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_INT32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"pending\""), "reply with a different parameter type does not confirm write");

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 9.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"pending\""), "same-name reply with wrong value does not confirm write");

    char* cached = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    CHECK(cached != nullptr && parse_param_value(cached) > 4.9f &&
              parse_param_value(cached) < 5.1f, "mismatched replies do not update the active cache");
    if (cached) mg_free_string(cached);

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"confirmed\""), "matching source, component, type and value confirms write");

    // A delayed reply from an older request generation must not confirm a
    // newer write to the same parameter.
    env.mock->set_param_drop(2);
    char* first = mg_config_write_param(env.core, "WPNAV_SPEED", 11.0f);
    if (first) mg_free_string(first);
    env.tick(50);
    char* second = mg_config_write_param(env.core, "WPNAV_SPEED", 12.0f);
    CHECK(second != nullptr && strstr(second, "true") != nullptr, "replacement write request is sent");
    if (second) mg_free_string(second);
    env.tick(50);

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 11.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"pending\""), "stale-generation value does not confirm replacement write");

    inject_param_value(env, 1, 1, "WPNAV_SPEED", 12.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(diagnostics_has_state(env, "\"state\":\"confirmed\""), "current generation value confirms replacement write");
}

static void test_write_isolated_by_new_download() {
    fprintf(stderr, "\n[Group] Parameter write isolation across a new download\n");
    TestEnv env;
    env.tick(1100);
    if (!load_all_params(env)) return;

    // Hold the echo so the write is still pending when the download restarts.
    env.mock->set_param_drop(1);
    char* set_res = mg_config_write_param(env.core, "WPNAV_SPEED", 10.0f);
    CHECK(set_res != nullptr && strstr(set_res, "true") != nullptr, "pending write request is sent");
    if (set_res) mg_free_string(set_res);
    env.tick(50);
    CHECK(diagnostics_has_state(env, "\"state\":\"pending\""), "write is pending before the download restarts");

    // A reconnect or forced refresh begins a new download. The in-flight write
    // must be failed against the old session instead of crossing into the new
    // one, where a same-name PARAM_VALUE could otherwise confirm it.
    char* restart = mg_params_request_all(env.core);
    CHECK(restart != nullptr && strstr(restart, "\"success\":true") != nullptr, "new download request is sent after the link restarts");
    if (restart) mg_free_string(restart);
    env.tick(50);
    CHECK(diagnostics_has_state(
              env, "parameter write cancelled by a new parameter download"), "in-flight write is failed by the new download");
    CHECK(!diagnostics_has_state(env, "\"state\":\"pending\""), "cancelled write is no longer pending");

    // A late echo from the old request must not resurrect a confirmed state.
    inject_param_value(env, 1, 1, "WPNAV_SPEED", 10.0f,
                       MAV_PARAM_TYPE_REAL32, 36, 0);
    CHECK(!diagnostics_has_state(env, "\"state\":\"confirmed\""), "late echo after restart does not confirm the cancelled write");
}

static void test_download_index_completion_and_source_filter() {
    fprintf(stderr, "\n[Group] Parameter download index and source filtering\n");
    TestEnv env;
    env.tick(1100);
    env.mock->set_param_list_no_response(true);

    char* request = mg_params_request_all(env.core);
    CHECK(request != nullptr && strstr(request, "\"success\":true") != nullptr, "indexed download request is sent");
    if (request) mg_free_string(request);
    env.tick(50);

    inject_param_value(env, 2, 1, "WRONG_SYSTEM", 1.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 0);
    char* wrong = mg_params_get_by_name(env.core, "WRONG_SYSTEM");
    CHECK(wrong != nullptr && strstr(wrong, "\"found\":false") != nullptr, "other system parameter is not cached by active download");
    if (wrong) mg_free_string(wrong);

    inject_param_value(env, 1, 2, "WRONG_COMPONENT", 1.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 0);
    char* progress = mg_params_get_progress(env.core);
    CHECK(parse_progress_received(progress) == 0 && parse_progress_total(progress) == 0, "other component parameter does not advance download");
    if (progress) mg_free_string(progress);

    inject_param_value(env, 1, 1, "IDX0", 1.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 0);
    inject_param_value(env, 1, 1, "IDX2", 3.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 2);
    inject_param_value(env, 1, 1, "IDX2_DUPLICATE", 3.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 2);

    progress = mg_params_get_progress(env.core);
    CHECK(parse_progress_received(progress) == 2 && parse_progress_total(progress) == 3, "duplicate index does not complete a missing-index download");
    if (progress) mg_free_string(progress);
    CHECK(env.mav->param_download_active(), "download remains active until index 1 arrives");

    inject_param_value(env, 1, 1, "IDX1", 2.0f,
                       MAV_PARAM_TYPE_REAL32, 3, 1);
    CHECK(!env.mav->param_download_active() && !env.mav->param_download_failed(), "download completes after all 0..count-1 indices arrive");
}

static void test_full_download() {
    fprintf(stderr, "\n[Group] Full parameter download\n");
    TestEnv env;

    env.tick(1100);

    char* res = mg_params_request_all(env.core);
    CHECK(res != nullptr && strstr(res, "true") != nullptr, "request_all returns success");
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

    CHECK(total > 0, "total > 0");
    CHECK(total >= 25, "total >= 25");
    CHECK(total > 0 && received >= total, "received == total (download complete)");

    fprintf(stderr, "  (params: received=%d total=%d)\n", received, total);
}

static void test_cancel_download() {
    fprintf(stderr, "\n[Group] Parameter download cancellation\n");
    TestEnv env;
    env.tick(1100);

    char* res = mg_params_request_all(env.core);
    CHECK(res != nullptr && strstr(res, "\"success\":true") != nullptr, "cancel test request starts");
    if (res) mg_free_string(res);

    char* cancel = mg_operations_cancel(env.core, "params");
    CHECK(cancel != nullptr && strstr(cancel, "\"canceled\":true") != nullptr, "active parameter download can be cancelled through operations");
    if (cancel) mg_free_string(cancel);

    char* snapshot = mg_operations_get_snapshot(env.core);
    CHECK(snapshot != nullptr && strstr(snapshot, "\"state\":\"cancelled\"") != nullptr, "parameter operation reports cancelled terminal state");
    CHECK(snapshot != nullptr && strstr(snapshot, "parameter download cancelled") != nullptr, "parameter cancellation keeps its reason");
    if (snapshot) mg_free_string(snapshot);

    res = mg_params_request_all(env.core);
    CHECK(res != nullptr && strstr(res, "\"success\":true") != nullptr, "parameter download can restart after cancellation");
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
    CHECK(total > 0 && received >= total, "parameter download completes after restart");
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
    CHECK(json != nullptr, "get WPNAV_SPEED not null");
    CHECK(strstr(json, "\"found\":true") != nullptr, "get WPNAV_SPEED found");

    const char* vp = strstr(json, "\"value\":");
    CHECK(vp != nullptr, "value field present");
    if (vp) {
        const float val = static_cast<float>(atof(vp + 8));
        CHECK(val > 4.9f && val < 5.1f, "WPNAV_SPEED == 5.0");
        fprintf(stderr, "  (WPNAV_SPEED = %g)\n", val);
    }
    mg_free_string(json);

    char* notfound = mg_params_get_by_name(env.core, "NONEXISTENT_PARAM");
    CHECK(strstr(notfound, "\"found\":false") != nullptr, "not found param returns found:false");
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

    char* set_res = mg_config_write_param(env.core, "WPNAV_SPEED", 10.0f);
    CHECK(set_res != nullptr && strstr(set_res, "true") != nullptr, "set returns success");
    mg_free_string(set_res);

    env.tick(100);

    bool found = false;
    for (const auto& name : env.mock->received_params_set) {
        if (name == "WPNAV_SPEED") {
            found = true;
            break;
        }
    }
    CHECK(found, "mock received WPNAV_SPEED set");

    char* json = mg_params_get_by_name(env.core, "WPNAV_SPEED");
    const char* vp = strstr(json, "\"value\":");
    if (vp) {
        const float val = static_cast<float>(atof(vp + 8));
        CHECK(val > 9.9f && val < 10.1f, "WPNAV_SPEED updated to 10.0");
        fprintf(stderr, "  (WPNAV_SPEED = %g after set)\n", val);
    }
    mg_free_string(json);

    char* diagnostics = mg_system_get_diagnostics(env.core);
    CHECK(diagnostics != nullptr && strstr(diagnostics, "\"parameter_history\"") != nullptr, "system diagnostics exposes parameter_history");
    CHECK(diagnostics != nullptr && strstr(diagnostics, "\"name\":\"WPNAV_SPEED\"") != nullptr, "parameter history records the requested parameter");
    CHECK(diagnostics != nullptr && strstr(diagnostics, "\"state\":\"confirmed\"") != nullptr, "parameter history marks a readback as confirmed");
    if (diagnostics) mg_free_string(diagnostics);
}

static void test_config_write_rejects_read_only() {
    fprintf(stderr, "\n[Group] Config parameter metadata validation\n");
    TestEnv env;

    env.tick(1100);
    env.mock->received_params_set.clear();

    char* single = mg_config_write_param(env.core, "BARO1_DEVID", 1.0f);
    CHECK(single != nullptr && strstr(single, "read-only") != nullptr, "single read-only write is rejected");
    CHECK(env.mock->received_params_set.empty(), "single read-only write sends no PARAM_SET");
    if (single) mg_free_string(single);

    char* valid = mg_config_write_param(env.core, "WPNAV_SPEED", 8.0f);
    CHECK(valid != nullptr && strstr(valid, "\"success\":true") != nullptr,
          "writable parameter is accepted");
    env.tick(1);
    CHECK(env.mock->received_params_set.size() == 1 &&
              env.mock->received_params_set.front() == "WPNAV_SPEED",
          "only the writable parameter reaches PARAM_SET");
    if (valid) mg_free_string(valid);
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

int main() {
    fprintf(stderr, "=== Parameter Simulation Tests ===\n");
    test_full_download();
    test_download_index_completion_and_source_filter();
    test_param_value_write_validation();
    test_write_isolated_by_new_download();
    test_cancel_download();
    test_param_get();
    test_param_set();
    test_config_write_rejects_read_only();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
