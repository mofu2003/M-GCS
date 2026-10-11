/**
 * test_real_fc_px4_ops.cc - Manual PX4 hardware operation probe.
 *
 * Defaults to read-only identification. Mutating commands require explicit
 * flags so operators can run one narrow action at a time.
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
#include <vector>

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

static bool wait_params_loaded(mg_core_t* core, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        char* p = mg_params_get_progress(core);
        std::string progress = p ? p : "";
        free_json(p);

        cJSON* root = cJSON_Parse(progress.c_str());
        if (root) {
            cJSON* received = cJSON_GetObjectItemCaseSensitive(root, "received");
            cJSON* total = cJSON_GetObjectItemCaseSensitive(root, "total");
            if (cJSON_IsNumber(received) && cJSON_IsNumber(total) &&
                total->valueint > 0 && received->valueint >= total->valueint) {
                std::fprintf(stderr, "[params_progress] %s\n", progress.c_str());
                cJSON_Delete(root);
                return true;
            }
            cJSON_Delete(root);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    call_json("params_progress_timeout", mg_params_get_progress, core);
    return false;
}

static void show_param(mg_core_t* core, const char* name) {
    char* p = mg_params_get_by_name(core, name);
    std::fprintf(stderr, "[param:%s] %s\n", name, p ? p : "");
    free_json(p);
}

static void show_identity_params(mg_core_t* core) {
    std::fprintf(stderr, "\n--- PX4 identity / airframe parameters ---\n");
    show_param(core, "SYS_AUTOSTART");
    show_param(core, "SYS_AUTOCONFIG");
    show_param(core, "CA_AIRFRAME");
    show_param(core, "PWM_MAIN_FUNC1");
    show_param(core, "PWM_MAIN_FUNC2");
    show_param(core, "PWM_MAIN_FUNC3");
    show_param(core, "PWM_MAIN_FUNC4");
    show_param(core, "COM_ARM_WO_GPS");
    show_param(core, "CBRK_IO_SAFETY");
    show_param(core, "CBRK_SUPPLY_CHK");
    show_param(core, "CBRK_USB_CHK");
}

static bool result_success(const std::string& json) {
    return json.find("\"success\":true") != std::string::npos;
}

static std::string command_arm(mg_core_t* core, bool force) {
    char* p = mg_command_arm(core, force ? 1 : 0);
    std::string out = p ? p : "";
    std::fprintf(stderr, "[%s_arm] %s\n", force ? "force" : "normal", out.c_str());
    free_json(p);
    return out;
}

static std::string command_disarm(mg_core_t* core, bool force) {
    char* p = mg_command_disarm(core, force ? 1 : 0);
    std::string out = p ? p : "";
    std::fprintf(stderr, "[%s_disarm] %s\n", force ? "force" : "normal", out.c_str());
    free_json(p);
    return out;
}

static void command_takeoff(mg_core_t* core, float altitude_m) {
    char* p = mg_command_takeoff(core, altitude_m);
    std::fprintf(stderr, "[takeoff %.1fm] %s\n", altitude_m, p ? p : "");
    free_json(p);
}

static void motor_test(mg_core_t* core, int motor, float throttle_pct, int duration_ms) {
    char* p = mg_actuator_motor_test(core, motor, throttle_pct, duration_ms);
    std::fprintf(stderr, "[motor_test motor=%d throttle=%.1f duration_ms=%d] %s\n",
                 motor, throttle_pct, duration_ms, p ? p : "");
    free_json(p);
}

static void usage() {
    std::fprintf(stderr,
        "Usage: test_real_fc_px4_ops <port> [--identify] [--force-arm] [--takeoff-alt N] "
        "[--motor-test] [--motor N] [--throttle PCT] [--duration-ms N]\n");
}

int main(int argc, char** argv) {
    const char* port = nullptr;
    bool identify = true;
    bool do_force_arm = false;
    bool do_takeoff = false;
    bool do_motor_test = false;
    float takeoff_alt = 2.0f;
    int motor = 1;
    float throttle_pct = 0.0f;
    int duration_ms = 1000;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--identify") == 0) {
            identify = true;
        } else if (std::strcmp(argv[i], "--force-arm") == 0) {
            do_force_arm = true;
        } else if (std::strcmp(argv[i], "--takeoff-alt") == 0 && i + 1 < argc) {
            do_takeoff = true;
            takeoff_alt = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--motor-test") == 0) {
            do_motor_test = true;
        } else if (std::strcmp(argv[i], "--motor") == 0 && i + 1 < argc) {
            motor = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--throttle") == 0 && i + 1 < argc) {
            throttle_pct = static_cast<float>(std::atof(argv[++i]));
        } else if (std::strcmp(argv[i], "--duration-ms") == 0 && i + 1 < argc) {
            duration_ms = std::atoi(argv[++i]);
        } else if (!port) {
            port = argv[i];
        } else {
            usage();
            return 2;
        }
    }

    if (!port) port = std::getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) {
        usage();
        return 2;
    }

    std::fprintf(stderr, "=== PX4 Manual Ops Probe ===\n");
    std::fprintf(stderr, "Target: %s @ 115200\n", port);
    if (do_force_arm || do_takeoff || do_motor_test) {
        std::fprintf(stderr, "Mutating actions: force_arm=%s takeoff=%s motor_test=%s\n",
                     do_force_arm ? "true" : "false",
                     do_takeoff ? "true" : "false",
                     do_motor_test ? "true" : "false");
    }

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

    std::string summary = call_json("summary", mg_vehicle_get_summary, core);
    bool armed = false;
    std::string mode;
    if (!parse_summary(summary, armed, mode)) {
        std::fprintf(stderr, "[FAIL] unable to parse vehicle summary\n");
        mg_destroy(core);
        return 1;
    }

    call_json("prearm", mg_vehicle_get_prearm_check, core);
    call_json("sensor_health", mg_sensors_get_health, core);
    call_json("available_modes", mg_flight_get_available_modes, core);
    call_json("px4_actuator_config", mg_px4_actuator_get_config, core);
    call_json("px4_safety", mg_px4_safety_get, core);

    if (identify) {
        char* request = mg_params_request_all(core);
        std::fprintf(stderr, "[params_request] %s\n", request ? request : "");
        free_json(request);
        wait_params_loaded(core, 15000);
        show_identity_params(core);
        call_json("px4_actuator_config_after_params", mg_px4_actuator_get_config, core);
        call_json("px4_safety_after_params", mg_px4_safety_get, core);
    }

    bool armed_by_test = false;
    int rc = 0;
    if (do_force_arm) {
        std::string arm = command_arm(core, true);
        armed_by_test = result_success(arm);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        call_json("summary_after_arm", mg_vehicle_get_summary, core);
        if (!armed_by_test) rc = 1;
    }

    if (do_takeoff) {
        command_takeoff(core, takeoff_alt);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        call_json("summary_after_takeoff", mg_vehicle_get_summary, core);
    }

    if (do_motor_test) {
        motor_test(core, motor, throttle_pct, duration_ms);
        std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms + 500));
    }

    if (armed_by_test) {
        command_disarm(core, true);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        call_json("summary_after_disarm", mg_vehicle_get_summary, core);
    }

    mg_connection_disconnect(core);
    mg_destroy(core);
    std::fprintf(stderr, "\n=== Result: %s ===\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
