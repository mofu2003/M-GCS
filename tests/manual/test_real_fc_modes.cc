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

int main(int argc, char** argv) {
    const char* port = argc > 1 ? argv[1] : std::getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) port = "COM12";

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
    call_json("available_modes", mg_flight_get_available_modes, core);
    call_json("mode_status_before", mg_flight_get_mode_status, core);

    SummarySnapshot before;
    if (!parse_summary(summary_json, before)) {
        std::fprintf(stderr, "[FAIL] unable to parse vehicle summary\n");
        mg_connection_disconnect(core);
        mg_destroy(core);
        return 1;
    }
    std::fprintf(stderr, "[safety] armed=%s mode=%s\n", before.armed ? "true" : "false", before.mode.c_str());
    if (before.armed) {
        std::fprintf(stderr, "[ABORT] vehicle is armed; mode switch test skipped\n");
        mg_connection_disconnect(core);
        mg_destroy(core);
        return 2;
    }

    std::fprintf(stderr, "\n--- Switch to AltHold ---\n");
    char* alt_hold = mg_cmd_set_mode(core, "AltHold");
    std::string alt_hold_json = alt_hold ? alt_hold : "";
    std::fprintf(stderr, "[set_mode_alt_hold] %s\n", alt_hold_json.c_str());
    free_json(alt_hold);

    const bool switched_to_alt_hold =
        alt_hold_json.find("\"success\":true") != std::string::npos &&
        wait_for_mode(core, "AltHold", 6000);
    call_json("mode_status_after_alt_hold", mg_flight_get_mode_status, core);

    std::fprintf(stderr, "\n--- Switch back to Stabilize ---\n");
    char* stabilize = mg_cmd_set_mode(core, "Stabilize");
    std::string stabilize_json = stabilize ? stabilize : "";
    std::fprintf(stderr, "[set_mode_stabilize] %s\n", stabilize_json.c_str());
    free_json(stabilize);

    const bool switched_back =
        stabilize_json.find("\"success\":true") != std::string::npos &&
        wait_for_mode(core, "Stabilize", 6000);
    call_json("summary_after", mg_vehicle_get_summary, core);
    call_json("mode_status_after", mg_flight_get_mode_status, core);

    mg_connection_disconnect(core);
    mg_destroy(core);

    const bool ok = switched_to_alt_hold && switched_back;
    std::fprintf(stderr, "\n=== Result: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
