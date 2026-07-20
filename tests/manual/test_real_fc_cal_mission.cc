/**
 * test_real_fc_cal_mission.cc - Real FC calibration-status and mission roundtrip check.
 *
 * This tool is intended for operator-run hardware verification.
 * It does not start sensor calibration. It only reads calibration state.
 *
 * Mission write/read is intentionally gated by an armed-state check. The tool
 * backs up the current mission, uploads a tiny waypoint mission, downloads it
 * for verification, then attempts to restore the original mission.
 */
#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 20
#include "cJSON.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

struct MissionItemSnapshot {
    int seq = 0;
    int frame = 3;
    int command = 16;
    int current = 0;
    int autocontinue = 1;
    double param1 = 0;
    double param2 = 0;
    double param3 = 0;
    double param4 = 0;
    int x = 0;
    int y = 0;
    double z = 0;
};

static void free_json(char* p) {
    if (p) mg_free_string(p);
}

static std::string call_json(const char* label, char* (*fn)(mg_core_t*), mg_core_t* core) {
    char* p = fn(core);
    std::string out = p ? p : "";
    fprintf(stderr, "[%s] %s\n", label, out.c_str());
    free_json(p);
    return out;
}

static std::string call_json_int(const char* label, char* (*fn)(mg_core_t*, int), mg_core_t* core, int arg) {
    char* p = fn(core, arg);
    std::string out = p ? p : "";
    fprintf(stderr, "[%s] %s\n", label, out.c_str());
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

static double json_num(cJSON* obj, const char* key, double def = 0) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valuedouble : def;
}

static std::string json_string(cJSON* obj, const char* key, const char* def = "") {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : def;
}

static bool parse_summary(const std::string& json, bool& armed, std::string& mode) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return false;
    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsObject(data)) {
        cJSON_Delete(root);
        return false;
    }
    armed = json_bool(data, "armed", false);
    mode = json_string(data, "flight_mode", "");
    cJSON_Delete(root);
    return true;
}

static std::vector<MissionItemSnapshot> parse_mission_items(const std::string& json) {
    std::vector<MissionItemSnapshot> result;
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) return result;
    cJSON* items = cJSON_GetObjectItemCaseSensitive(root, "items");
    if (!cJSON_IsArray(items)) {
        cJSON_Delete(root);
        return result;
    }
    int count = cJSON_GetArraySize(items);
    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(items, i);
        if (!cJSON_IsObject(item)) continue;
        MissionItemSnapshot s;
        s.seq = static_cast<int>(json_num(item, "seq", i));
        s.frame = static_cast<int>(json_num(item, "frame", 3));
        s.command = static_cast<int>(json_num(item, "command", 16));
        s.current = static_cast<int>(json_num(item, "current", i == 0 ? 1 : 0));
        s.autocontinue = static_cast<int>(json_num(item, "autocontinue", 1));
        s.param1 = json_num(item, "param1", 0);
        s.param2 = json_num(item, "param2", 0);
        s.param3 = json_num(item, "param3", 0);
        s.param4 = json_num(item, "param4", 0);
        s.x = static_cast<int>(json_num(item, "x", 0));
        s.y = static_cast<int>(json_num(item, "y", 0));
        s.z = json_num(item, "z", 0);
        result.push_back(s);
    }
    cJSON_Delete(root);
    return result;
}

static std::string mission_to_upload_json(const std::vector<MissionItemSnapshot>& items) {
    std::string out = "[";
    for (size_t i = 0; i < items.size(); i++) {
        const auto& item = items[i];
        if (i > 0) out += ",";
        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"seq\":%d,\"frame\":%d,\"command\":%d,\"current\":%d,"
            "\"autocontinue\":%d,\"param1\":%.8g,\"param2\":%.8g,"
            "\"param3\":%.8g,\"param4\":%.8g,\"x\":%d,\"y\":%d,\"z\":%.8g}",
            item.seq, item.frame, item.command, item.current,
            item.autocontinue, item.param1, item.param2,
            item.param3, item.param4, item.x, item.y, item.z);
        out += buf;
    }
    out += "]";
    return out;
}

static bool wait_mission_idle(mg_core_t* core, const char* label, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        char* p = mg_mission_get_progress(core);
        std::string progress = p ? p : "";
        free_json(p);
        if (progress.find("\"state\":\"idle\"") != std::string::npos) {
            fprintf(stderr, "[%s-progress] %s\n", label, progress.c_str());
            return true;
        }
        if (progress.find("\"error\":\"\"") == std::string::npos &&
            progress.find("\"error\":") != std::string::npos) {
            fprintf(stderr, "[%s-progress] %s\n", label, progress.c_str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    char* p = mg_mission_get_progress(core);
    fprintf(stderr, "[%s-progress-timeout] %s\n", label, p ? p : "");
    free_json(p);
    return false;
}

static std::string download_plan(mg_core_t* core, const char* label) {
    char* r = mg_mission_download(core);
    fprintf(stderr, "[%s-download] %s\n", label, r ? r : "");
    free_json(r);
    wait_mission_idle(core, label, 10000);
    return call_json("mission_plan", mg_mission_get_plan, core);
}

static bool upload_plan(mg_core_t* core, const std::string& json, const char* label) {
    char* r = mg_mission_upload(core, json.c_str());
    std::string result = r ? r : "";
    fprintf(stderr, "[%s-upload] %s\n", label, result.c_str());
    free_json(r);
    if (result.find("\"success\":true") == std::string::npos) return false;
    return wait_mission_idle(core, label, 10000);
}

static bool near_int(int actual, int expected, int tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

int main(int argc, char** argv) {
    const char* port = argc > 1 ? argv[1] : getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) port = "COM12";

    fprintf(stderr, "=== Real FC Calibration + Mission Check ===\n");
    fprintf(stderr, "Target: %s @ 115200\n", port);

    mg_core_t* core = mg_create();
    if (!core) {
        fprintf(stderr, "[FAIL] mg_create failed\n");
        return 1;
    }

    std::string endpoint = std::string("serial://") + port + "?baud=115200";
    int vehicle_id = mg_vehicle_open(core, endpoint.c_str());
    fprintf(stderr, "[vehicle_open] id=%d endpoint=%s\n", vehicle_id, endpoint.c_str());
    if (vehicle_id < 0) {
        mg_destroy(core);
        return 1;
    }
    free_json(mg_vehicle_select(core, vehicle_id));
    std::this_thread::sleep_for(std::chrono::seconds(4));

    std::string summary = call_json("summary", mg_vehicle_get_summary, core);
    bool armed = false;
    std::string mode;
    if (!parse_summary(summary, armed, mode)) {
        fprintf(stderr, "[FAIL] unable to parse vehicle summary\n");
        mg_destroy(core);
        return 1;
    }
    fprintf(stderr, "[safety] armed=%s mode=%s\n", armed ? "true" : "false", mode.c_str());
    if (armed) {
        fprintf(stderr, "[ABORT] vehicle is armed; mission write test skipped\n");
        mg_destroy(core);
        return 2;
    }

    fprintf(stderr, "\n--- Calibration read-only checks ---\n");
    call_json("cal_status", mg_cal_status, core);
    call_json("cal_retry", mg_cal_retry_info, core);
    for (int type = 0; type <= 3; type++) {
        char label[32];
        snprintf(label, sizeof(label), "cal_history_%d", type);
        call_json_int(label, mg_cal_history, core, type);
    }
    call_json("prearm", mg_vehicle_get_prearm_check, core);
    call_json("sensor_health", mg_sensors_get_health, core);

    fprintf(stderr, "\n--- Mission backup/download ---\n");
    std::string original_plan = download_plan(core, "backup");
    auto original_items = parse_mission_items(original_plan);
    fprintf(stderr, "[backup] item_count=%zu\n", original_items.size());

    std::vector<MissionItemSnapshot> test_items;
    if (!original_items.empty()) {
        test_items.push_back(original_items.front());
        test_items.back().seq = 0;
        test_items.back().current = 0;
    }

    MissionItemSnapshot wp;
    wp.seq = static_cast<int>(test_items.size());
    wp.frame = 3;
    wp.command = 16;
    wp.current = test_items.empty() ? 1 : 0;
    wp.autocontinue = 1;
    wp.x = 323654520;
    wp.y = 1148079500;
    wp.z = 20.0;
    test_items.push_back(wp);
    std::string test_json = mission_to_upload_json(test_items);

    fprintf(stderr, "\n--- Mission upload/readback test ---\n");
    bool upload_ok = upload_plan(core, test_json, "test");
    std::string readback_plan = download_plan(core, "readback");
    auto readback_items = parse_mission_items(readback_plan);
    bool readback_ok = readback_items.size() == test_items.size();
    bool found_test_wp = false;
    for (const auto& item : readback_items) {
        if (item.command == 16 &&
            near_int(item.x, wp.x, 100) &&
            near_int(item.y, wp.y, 100) &&
            std::abs(item.z - wp.z) < 0.5) {
            found_test_wp = true;
            break;
        }
    }
    readback_ok = readback_ok && found_test_wp;
    fprintf(stderr, "[readback-check] upload_ok=%s expected_count=%zu readback_count=%zu found_test_wp=%s readback_ok=%s\n",
            upload_ok ? "true" : "false",
            test_items.size(),
            readback_items.size(),
            found_test_wp ? "true" : "false",
            readback_ok ? "true" : "false");

    fprintf(stderr, "\n--- Mission restore ---\n");
    bool restore_ok = true;
    if (!original_items.empty()) {
        std::string restore_json = mission_to_upload_json(original_items);
        restore_ok = upload_plan(core, restore_json, "restore");
        std::string restored_plan = download_plan(core, "restore-readback");
        auto restored_items = parse_mission_items(restored_plan);
        restore_ok = restore_ok && restored_items.size() == original_items.size();
        fprintf(stderr, "[restore-check] expected=%zu restored=%zu restore_ok=%s\n",
                original_items.size(), restored_items.size(), restore_ok ? "true" : "false");
    } else {
        char* clear = mg_mission_clear(core);
        fprintf(stderr, "[restore-clear-empty] %s\n", clear ? clear : "");
        free_json(clear);
        wait_mission_idle(core, "restore-clear", 5000);
    }

    mg_connection_disconnect(core);
    mg_destroy(core);

    bool ok = upload_ok && readback_ok && restore_ok;
    fprintf(stderr, "\n=== Result: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
