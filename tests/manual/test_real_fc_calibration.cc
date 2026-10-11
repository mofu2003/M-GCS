/**
 * test_real_fc_calibration.cc - 真实飞控传感器校准冒烟测试。
 *
 * 本硬件测试只启动陀螺仪校准。若飞控已解锁，测试会立即中止。
 * 加速度计和罗盘校准需要操作者移动或旋转机体，不能在这里自动触发。
 */
#include "ffi_api.h"

#define CJSON_NESTING_LIMIT 20
#include "cJSON.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

struct CalStatusSnapshot {
    bool active = false;
    std::string stage;
    int progress = 0;
    std::string message;
    std::string failure_reason;
};

static std::mutex g_print_mtx;

static void free_json(char* p) {
    if (p) mg_free_string(p);
}

static std::string call_json(const char* label, char* (*fn)(mg_core_t*), mg_core_t* core) {
    char* p = fn(core);
    std::string out = p ? p : "";
    std::lock_guard<std::mutex> lk(g_print_mtx);
    std::fprintf(stderr, "[%s] %s\n", label, out.c_str());
    free_json(p);
    return out;
}

static std::string call_json_int(const char* label, char* (*fn)(mg_core_t*, int), mg_core_t* core, int arg) {
    char* p = fn(core, arg);
    std::string out = p ? p : "";
    std::lock_guard<std::mutex> lk(g_print_mtx);
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

static bool parse_cal_status(const std::string& json, CalStatusSnapshot& status) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }
    status.active = json_bool(root, "active", false);
    status.stage = json_string(root, "stage", "");
    status.progress = static_cast<int>(json_num(root, "progress", 0));
    status.message = json_string(root, "message", "");
    status.failure_reason = json_string(root, "failure_reason", "");
    cJSON_Delete(root);
    return true;
}

static void event_callback(mg_event_type_t type, char* json, void*) {
    if (type == MG_EVENT_STATUSTEXT) {
        std::lock_guard<std::mutex> lk(g_print_mtx);
        std::fprintf(stderr, "[event:statustext] %s\n", json ? json : "");
    }
    free_json(json);
}

static CalStatusSnapshot poll_cal_status(mg_core_t* core, int timeout_ms) {
    CalStatusSnapshot last;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        char* p = mg_calibration_get_status(core);
        std::string json = p ? p : "";
        free_json(p);
        CalStatusSnapshot parsed;
        if (parse_cal_status(json, parsed)) {
            last = parsed;
            {
                std::lock_guard<std::mutex> lk(g_print_mtx);
                std::fprintf(stderr,
                             "[cal_poll] active=%s stage=%s progress=%d message=\"%s\" failure=\"%s\"\n",
                             parsed.active ? "true" : "false",
                             parsed.stage.c_str(),
                             parsed.progress,
                             parsed.message.c_str(),
                             parsed.failure_reason.c_str());
            }
            if (!parsed.active &&
                (parsed.stage == "success" ||
                 parsed.stage == "failed" ||
                 parsed.stage == "cancelled")) {
                return parsed;
            }
        } else {
            std::lock_guard<std::mutex> lk(g_print_mtx);
            std::fprintf(stderr, "[cal_poll_parse_fail] %s\n", json.c_str());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return last;
}

int main(int argc, char** argv) {
    const char* port = argc > 1 ? argv[1] : std::getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) {
        std::fprintf(stderr, "[SKIP] Set MG_TEST_SERIAL or pass the serial port explicitly.\n");
        return 0;
    }

    std::fprintf(stderr, "=== Real FC Gyro Calibration Test ===\n");
    std::fprintf(stderr, "Target: %s @ 115200\n", port);
    std::fprintf(stderr, "Safety: gyro only; keep the vehicle still and level.\n");

    mg_core_t* core = mg_create();
    if (!core) {
        std::fprintf(stderr, "[FAIL] mg_create failed\n");
        return 1;
    }
    mg_set_event_callback(core, event_callback, nullptr);

    std::string endpoint = std::string("serial://") + port + "?baud=115200";
    int vehicle_id = mg_vehicle_open(core, endpoint.c_str());
    std::fprintf(stderr, "[vehicle_open] id=%d endpoint=%s\n", vehicle_id, endpoint.c_str());
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
        std::fprintf(stderr, "[FAIL] unable to parse vehicle summary\n");
        mg_connection_disconnect(core);
        mg_destroy(core);
        return 1;
    }
    std::fprintf(stderr, "[safety] armed=%s mode=%s\n", armed ? "true" : "false", mode.c_str());
    if (armed) {
        std::fprintf(stderr, "[ABORT] vehicle is armed; calibration was not started\n");
        mg_connection_disconnect(core);
        mg_destroy(core);
        return 2;
    }

    call_json("prearm", mg_vehicle_get_prearm_check, core);
    call_json("sensor_health", mg_sensors_get_health, core);
    call_json("statustext_before", mg_sensors_get_statustext, core);
    call_json("cal_status_before", mg_calibration_get_status, core);

    std::fprintf(stderr, "\n--- Starting gyroscope calibration ---\n");
    char* start = mg_calibration_start(core, 0, 0);
    std::string start_json = start ? start : "";
    std::fprintf(stderr, "[cal_start_gyro] %s\n", start_json.c_str());
    free_json(start);
    if (start_json.find("\"success\":true") == std::string::npos) {
        std::fprintf(stderr, "[FAIL] gyro calibration command was not accepted by bridge\n");
        call_json("cal_status_after_start_fail", mg_calibration_get_status, core);
        mg_connection_disconnect(core);
        mg_destroy(core);
        return 1;
    }

    CalStatusSnapshot final_status = poll_cal_status(core, 30000);

    call_json("cal_status_final", mg_calibration_get_status, core);
    call_json_int("cal_history_gyro", mg_calibration_get_history, core, 0);
    call_json("cal_retry", mg_calibration_get_retry_info, core);
    call_json("statustext_after", mg_sensors_get_statustext, core);

    mg_connection_disconnect(core);
    mg_destroy(core);

    const bool ok = !final_status.active && final_status.stage == "success";
    std::fprintf(stderr, "\n=== Result: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
