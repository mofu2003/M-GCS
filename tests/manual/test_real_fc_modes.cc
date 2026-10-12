/**
 * test_real_fc_modes.cc - Real FC mode switch check.
 *
 * This tool connects to a real flight controller, verifies the current mode,
 * switches to AltHold, waits for the heartbeat-reported mode change, then
 * switches back to Stabilize.
 */
#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 20
#include "cJSON.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

struct SummarySnapshot {
    bool armed = false;
    std::string mode;
};

static void free_json(char* p) {
    if (p) mg_free_string(p);
}

static std::string call_json(const char* label, char* (*fn)(mg_core_t*), mg_core_t* core) {
    char* p = fn(core);
    std::string out = p ? p : "";
    std::fprintf(stderr, "[%s] %s\n", label, out.c_str());
    free_json(p);
    return out;
}

static bool json_bool(cJSON* obj, const char* key, bool def = false) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item) return def;
    if (cJSON_IsTrue(item)) return true;
    if (cJSON_IsFalse(item)) return false;
    return def;
}

static std::string json_string(cJSON* obj, const char* key, const char* def = "") {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : def;
}

static bool parse_summary(const std::string& json, SummarySnapshot& summary) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return false;
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return false;
    }
    summary.armed = json_bool(data, "armed", false);
    summary.mode = json_string(data, "flight_mode", "");
    cJSON_Delete(root);
    return true;
}

static bool wait_for_mode(mg_core_t* core, const char* expected_mode, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        char* p = mg_vehicle_get_summary(core);
        std::string summary_json = p ? p : "";
        free_json(p);

        SummarySnapshot summary;
        if (parse_summary(summary_json, summary)) {
            std::fprintf(stderr, "[mode_poll] expected=%s actual=%s armed=%s\n",
                         expected_mode, summary.mode.c_str(), summary.armed ? "true" : "false");
            if (summary.mode == expected_mode) {
                return true;
            }
        } else {
            std::fprintf(stderr, "[mode_poll_parse_fail] %s\n", summary_json.c_str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return false;
}

/// 通过 submit/poll 同步切换模式（旧的同步 mg_flight_set_mode 已删除）。
static bool set_mode_sync(mg_core_t* core, const char* mode) {
    const int vid = mg_vehicle_get_active(core);
    const std::string body =
        std::string("{\"command\":\"set_mode\",\"args\":{\"mode\":\"") + mode + "\"}}";
    char* submitted = mg_command_submit_for_vehicle(core, vid, body.c_str());
    std::string submitted_json = submitted ? submitted : "";
    free_json(submitted);

    uint64_t ticket = 0;
    cJSON* root = cJSON_Parse(submitted_json.c_str());
    cJSON* ticket_json = root ? cJSON_GetObjectItemCaseSensitive(root, "ticket") : nullptr;
    if (cJSON_IsNumber(ticket_json)) ticket = static_cast<uint64_t>(ticket_json->valuedouble);
    if (root) cJSON_Delete(root);
    if (ticket == 0) return false;

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(12);
    while (std::chrono::steady_clock::now() < deadline) {
        char* polled = mg_command_poll(core, ticket);
        const std::string polled_json = polled ? polled : "";
        free_json(polled);
        if (polled_json.find("\"state\":\"complete\"") != std::string::npos) {
            std::fprintf(stderr, "[set_mode %s] %s\n", mode, polled_json.c_str());
            return polled_json.find("\"result\":{\"success\":true") != std::string::npos;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

int main(int argc, char** argv) {
    const char* port = argc > 1 ? argv[1] : std::getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) {
        std::fprintf(stderr, "[SKIP] Set MG_TEST_SERIAL or pass the serial port explicitly.\n");
        return 0;
    }

    std::fprintf(stderr, "=== Real FC Mode Switch Check ===\n");
    std::fprintf(stderr, "Target: %s @ 115200\n", port);

    mg_core_t* core = mg_create();
    if (!core) {
        std::fprintf(stderr, "[FAIL] mg_create failed\n");
        return 1;
    }

    std::string endpoint = std::string("serial://") + port + "?baud=115200";
    int vehicle_id = mg_vehicle_open(core, endpoint.c_str());
    std::fprintf(stderr, "[vehicle_open] id=%d endpoint=%s\n", vehicle_id, endpoint.c_str());
    if (vehicle_id < 0) {
        mg_destroy(core);
        return 1;
    }
    free_json(mg_vehicle_select(core, vehicle_id));
    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::string summary_json = call_json("summary_before", mg_vehicle_get_summary, core);
    call_json("available_modes", mg_flight_get_modes, core);
    call_json("mode_status_before", mg_flight_get_modes, core);

    SummarySnapshot before;
    if (!parse_summary(summary_json, before)) {
        std::fprintf(stderr, "[FAIL] unable to parse vehicle summary\n");
            mg_destroy(core);
        return 1;
    }
    std::fprintf(stderr, "[safety] armed=%s mode=%s\n", before.armed ? "true" : "false", before.mode.c_str());
    if (before.armed) {
        std::fprintf(stderr, "[ABORT] vehicle is armed; mode switch test skipped\n");
            mg_destroy(core);
        return 2;
    }

    std::fprintf(stderr, "\n--- Switch to AltHold ---\n");
    const bool switched_to_alt_hold =
        set_mode_sync(core, "AltHold") && wait_for_mode(core, "AltHold", 6000);
    call_json("mode_status_after_alt_hold", mg_flight_get_modes, core);

    std::fprintf(stderr, "\n--- Switch back to Stabilize ---\n");
    const bool switched_back =
        set_mode_sync(core, "Stabilize") && wait_for_mode(core, "Stabilize", 6000);
    call_json("summary_after", mg_vehicle_get_summary, core);
    call_json("mode_status_after", mg_flight_get_modes, core);

    mg_destroy(core);

    const bool ok = switched_to_alt_hold && switched_back;
    std::fprintf(stderr, "\n=== Result: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
