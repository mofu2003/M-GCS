/**
 * test_real_fc_dump.cc - Connect to real FC and dump ALL telemetry data
 *
 * Usage: test_real_fc_dump [port] [--quick]
 *   Default port: auto-detect first serial port
 */
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <string>

static void print_json(const char* label, char* json) {
    if (json) {
        fprintf(stderr, "[%s] %s\n", label, json);
        mg_free_string(json);
    } else {
        fprintf(stderr, "[%s] (null)\n", label);
    }
}

static void dump_all(mg_core_t* core) {
    fprintf(stderr, "\n========== ALL FC DATA ==========\n\n");

    // Connection state
    print_json("connection_state", mg_connection_get_state(core));

    // Heartbeat / Vehicle summary
    print_json("vehicle_summary", mg_vehicle_get_summary(core));

    // GPS
    print_json("gps", mg_vehicle_get_gps(core));
    print_json("global_position", mg_vehicle_get_global_position(core));

    // Attitude
    print_json("attitude", mg_vehicle_get_attitude(core));

    // Battery
    print_json("battery", mg_vehicle_get_battery(core));

    // VFR HUD
    print_json("vfr_hud", mg_vehicle_get_vfr_hud(core));

    // IMU
    print_json("imu", mg_vehicle_get_imu(core));
    print_json("highres_imu", mg_vehicle_get_highres_imu(core));

    // Pressure
    print_json("pressure", mg_vehicle_get_pressure(core));

    // RC Channels
    print_json("rc_channels", mg_vehicle_get_rc_channels(core));

    // Servo Output
    print_json("servo_output", mg_vehicle_get_servo_output(core));

    // Home Position
    print_json("home_position", mg_vehicle_get_home_position(core));

    // Extended Sys State
    print_json("extended_sys_state", mg_vehicle_get_extended_sys_state(core));

    // Vibration
    print_json("vibration", mg_vehicle_get_vibration(core));

    // ESC Status
    print_json("esc_status", mg_vehicle_get_esc_status(core));

    // Wind Estimate
    print_json("wind_estimate", mg_vehicle_get_wind_estimate(core));

    // Prearm Check
    print_json("prearm_check", mg_vehicle_get_prearm_check(core));

    // Firmware
    print_json("firmware", mg_vehicle_get_firmware_version(core));

    // Health
    print_json("health", mg_health_get_all(core));

    // Flight time
    print_json("flight_time", mg_vehicle_get_flight_time(core));

    // Distance sensors
    print_json("distance_sensors", mg_vehicle_get_distance_sensors(core));

    // ADSB traffic
    print_json("adsb_traffic", mg_vehicle_get_adsb_traffic(core));

    // Parameters (first 10)
    print_json("params_all", mg_params_get_all(core));
    print_json("params_progress", mg_params_get_progress(core));

    // Mission state
    print_json("mission_plan", mg_mission_get_plan(core));
    print_json("mission_progress", mg_mission_get_progress(core));
    print_json("mission_current", mg_mission_get_current(core));

    // Geofence
    print_json("geofence", mg_geofence_get(core));

    // Flight mode
    print_json("flight_mode_status", mg_flight_get_mode_status(core));
    print_json("available_modes", mg_flight_get_available_modes(core));

    // Sensors
    print_json("sensor_health", mg_sensors_get_health(core));
    print_json("sensor_statustext", mg_sensors_get_statustext(core));

    fprintf(stderr, "\n========== END ==========\n");
}

static void dump_quick(mg_core_t* core) {
    fprintf(stderr, "\n========== QUICK FC CHECK ==========\n\n");
    print_json("connection_state", mg_connection_get_state(core));
    print_json("vehicle_summary", mg_vehicle_get_summary(core));
    print_json("attitude", mg_vehicle_get_attitude(core));
    print_json("health", mg_health_get_all(core));
    print_json("firmware", mg_vehicle_get_firmware_version(core));
    print_json("available_modes", mg_flight_get_available_modes(core));
    fprintf(stderr, "\n========== END ==========\n");
}

static void dump_identity(mg_core_t* core) {
    fprintf(stderr, "\n========== FC IDENTITY ==========\n\n");
    print_json("vehicle_summary", mg_vehicle_get_summary(core));
    print_json("firmware", mg_vehicle_get_firmware_version(core));
    print_json("sensor_statustext", mg_sensors_get_statustext(core));
    fprintf(stderr, "\n========== END ==========\n");
}

static bool is_connected_state(const char* state) {
    return state && strstr(state, "\"connected\":true");
}

static bool has_firmware_version(mg_core_t* core) {
    char* firmware = mg_vehicle_get_firmware_version(core);
    bool received = firmware && strstr(firmware, "\"received\":true");
    mg_free_string(firmware);
    return received;
}

static bool has_vehicle_summary(mg_core_t* core) {
    char* summary = mg_vehicle_get_summary(core);
    bool ready = summary && strstr(summary, "\"system_id\":1");
    mg_free_string(summary);
    return ready;
}

static bool has_product_name(mg_core_t* core) {
    char* summary = mg_vehicle_get_summary(core);
    bool ready = summary && strstr(summary, "\"product_name\":");
    mg_free_string(summary);
    return ready;
}

static bool has_health_snapshot(mg_core_t* core) {
    char* health = mg_health_get_all(core);
    bool ready = health && strstr(health, "\"system_id\":1");
    mg_free_string(health);
    return ready;
}

static void wait_for_identity(mg_core_t* core, bool require_product_name) {
    for (int j = 0; j < 30; j++) {
        const bool summary_ready = has_vehicle_summary(core);
        const bool firmware_ready = has_firmware_version(core);
        const bool product_ready = has_product_name(core);
        const bool health_ready = has_health_snapshot(core);
        if (summary_ready && firmware_ready && health_ready &&
            (!require_product_name || product_ready)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!require_product_name || has_product_name(core)) {
        return;
    }

    char* param_request = mg_params_request_all(core);
    if (param_request) mg_free_string(param_request);

    for (int j = 0; j < 20; j++) {
        if (has_product_name(core)) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

struct CallbackOptions {
    bool quick = false;
};

static void on_event(mg_event_type_t type, char* json, void* user_data) {
    auto* options = static_cast<CallbackOptions*>(user_data);
    if (options && options->quick &&
        type != MG_EVENT_CONNECTION &&
        type != MG_EVENT_STATUSTEXT) {
        mg_free_string(json);
        return;
    }

    const char* type_name = "UNKNOWN";
    switch (type) {
        case MG_EVENT_HEARTBEAT:       type_name = "HEARTBEAT"; break;
        case MG_EVENT_GPS:             type_name = "GPS"; break;
        case MG_EVENT_ATTITUDE:        type_name = "ATTITUDE"; break;
        case MG_EVENT_BATTERY:         type_name = "BATTERY"; break;
        case MG_EVENT_CONNECTION:      type_name = "CONNECTION"; break;
        case MG_EVENT_STATUSTEXT:      type_name = "STATUSTEXT"; break;
        case MG_EVENT_GLOBAL_POSITION: type_name = "GLOBAL_POS"; break;
        case MG_EVENT_RC_CHANNELS:     type_name = "RC"; break;
        case MG_EVENT_SERVO_OUTPUT:    type_name = "SERVO"; break;
        case MG_EVENT_VIBRATION:       type_name = "VIBRATION"; break;
        case MG_EVENT_ESC_STATUS:      type_name = "ESC"; break;
        case MG_EVENT_WIND_ESTIMATE:   type_name = "WIND"; break;
        case MG_EVENT_PREARM_CHECK:    type_name = "PREARM"; break;
        case MG_EVENT_SENSOR_HEALTH:   type_name = "SENSOR"; break;
        case MG_EVENT_PARAMETERS:      type_name = "PARAM"; break;
        case MG_EVENT_MISSION:         type_name = "MISSION"; break;
        case MG_EVENT_HOME_POSITION:   type_name = "HOME"; break;
        case MG_EVENT_INITIAL_CONNECT: type_name = "INIT_CONN"; break;
        case MG_EVENT_EXTENDED_SYS_STATE: type_name = "EXT_STATE"; break;
        case MG_EVENT_WAYPOINT_REACHED: type_name = "WP_REACHED"; break;
        case MG_EVENT_DISTANCE_SENSOR: type_name = "DISTANCE"; break;
        case MG_EVENT_BATTERY_FAILSAFE: type_name = "BATT_FAILSAFE"; break;
        case MG_EVENT_ADSB:            type_name = "ADSB"; break;
        default: break;
    }
    fprintf(stderr, "[EVENT:%s] %s\n", type_name, json ? json : "");
    mg_free_string(json);
}

int main(int argc, char* argv[]) {
    fprintf(stderr, "=== Real FC Telemetry Dump ===\n");
    CallbackOptions callback_options;
    const char* port_arg = nullptr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quick") == 0) {
            callback_options.quick = true;
        } else if (!port_arg) {
            port_arg = argv[i];
        }
    }

    mg_core_t* core = mg_create();
    if (!core) { fprintf(stderr, "Failed to create core\n"); return 1; }

    // Set event callback
    mg_set_event_callback(core, on_event, &callback_options);

    // List serial ports
    fprintf(stderr, "\n--- Available Serial Ports ---\n");
    char* ports = mg_connection_list_ports(core);
    if (ports) {
        fprintf(stderr, "%s\n", ports);
        mg_free_string(ports);
    }

    // Determine port: argv[1] > MG_TEST_SERIAL env > skip
    const char* port = nullptr;
    if (port_arg) port = port_arg;
    if (!port) port = getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) {
        fprintf(stderr, "[SKIP] No serial port. Set MG_TEST_SERIAL or pass as argv[1]\n");
        mg_destroy(core);
        return 0;
    }

    fprintf(stderr, "\nConnecting to %s...\n", port);

    // Connect via serial
    char* result = mg_connection_connect(core, port, 115200);
    if (result) {
        fprintf(stderr, "Connect result: %s\n", result);
        mg_free_string(result);
    }

    // Wait for connection and initial data.
    fprintf(stderr, "Waiting for telemetry data...\n");
    bool dumped = false;
    for (int i = 0; i < 100; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        char* state = mg_connection_get_state(core);
        if (state) {
            if (is_connected_state(state)) {
                wait_for_identity(core, callback_options.quick);
                fprintf(stderr, "Connected! Dumping data...\n");
                if (callback_options.quick) {
                    dump_quick(core);
                } else {
                    dump_all(core);
                }
                mg_free_string(state);
                dumped = true;
                break;
            }
            mg_free_string(state);
        }
    }
    if (!dumped) fprintf(stderr, "[WARN] Timed out waiting for connection data\n");

    if (callback_options.quick) {
        fprintf(stderr, "\nDisconnecting...\n");
        mg_set_event_callback(core, nullptr, nullptr);
        mg_destroy(core);
        fprintf(stderr, "Done.\n");
        return 0;
    }

    // Request all params
    fprintf(stderr, "\n--- Requesting all parameters ---\n");
    char* pr = mg_params_request_all(core);
    if (pr) { fprintf(stderr, "Param request: %s\n", pr); mg_free_string(pr); }

    // Wait for params
    for (int i = 0; i < 50; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        char* prog = mg_params_get_progress(core);
        if (prog) {
            if (strstr(prog, "\"total\":") && strstr(prog, "\"received\":")) {
                const char* r = strstr(prog, "\"received\":");
                const char* t = strstr(prog, "\"total\":");
                if (r && t) {
                    int recv = atoi(r + 11);
                    int total = atoi(t + 8);
                    if (recv > 0 && recv == total) {
                        fprintf(stderr, "All %d params received!\n", total);
                        mg_free_string(prog);
                        break;
                    }
                }
            }
            mg_free_string(prog);
        }
    }

    // Final dump with all data
    fprintf(stderr, "\n--- Final Data Dump (after params) ---\n");
    dump_all(core);
    dump_identity(core);

    // Get first 20 params
    fprintf(stderr, "\n--- First 20 Parameters ---\n");
    char* all_params = mg_params_get_all(core);
    if (all_params) {
        fprintf(stderr, "%s\n", all_params);
        mg_free_string(all_params);
    }

    // Disconnect
    fprintf(stderr, "\nDisconnecting...\n");
    mg_set_event_callback(core, nullptr, nullptr);
    mg_destroy(core);

    fprintf(stderr, "Done.\n");
    return 0;
}
