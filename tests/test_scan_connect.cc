/**
 * test_scan_connect.cc - Scan ports, connect all FCs, dump all data
 */
#include "ffi_api.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    fprintf(stderr, "=== Quick FC Scanner ===\n\n");

    mg_core_t* core = mg_create();
    if (!core) { fprintf(stderr, "Failed to create core\n"); return 1; }

    // Scan
    fprintf(stderr, "--- Ports ---\n");
    char* ports = mg_connection_list_ports(core);
    if (ports) { fprintf(stderr, "%s\n", ports); mg_free_string(ports); }

    // If port given as arg or env, connect to it
    const char* port = nullptr;
    if (argc > 1) port = argv[1];
    if (!port) port = getenv("MG_TEST_SERIAL");
    if (!port || !port[0]) {
        fprintf(stderr, "[SKIP] No serial port. Set MG_TEST_SERIAL or pass as argv[1]\n");
        mg_destroy(core);
        return 0;
    }

    fprintf(stderr, "\nConnecting to %s...\n", port);
    int vid = mg_vehicle_open(core, port);
    if (vid < 0) {
        // Try as serial:// URI
        std::string uri = std::string("serial://") + port + "?baud=115200";
        vid = mg_vehicle_open(core, uri.c_str());
    }
    fprintf(stderr, "Vehicle ID: %d\n", vid);

    // Wait for heartbeat + data
    fprintf(stderr, "Waiting 8s for telemetry...\n");
    std::this_thread::sleep_for(std::chrono::seconds(8));

    // Select and dump everything
    if (vid >= 0) mg_vehicle_select(core, vid);

    fprintf(stderr, "\n========== ALL FC DATA ==========\n");

    auto dump = [](const char* label, char* json) {
        if (json && strlen(json) > 2) fprintf(stderr, "[%s] %s\n", label, json);
        if (json) mg_free_string(json);
    };

    dump("summary",    mg_vehicle_get_summary(core));
    dump("firmware",   mg_vehicle_get_firmware_version(core));
    dump("gps",        mg_vehicle_get_gps(core));
    dump("global_pos", mg_vehicle_get_global_position(core));
    dump("attitude",   mg_vehicle_get_attitude(core));
    dump("battery",    mg_vehicle_get_battery(core));
    dump("vfr_hud",    mg_vehicle_get_vfr_hud(core));
    dump("imu",        mg_vehicle_get_imu(core));
    dump("pressure",   mg_vehicle_get_pressure(core));
    dump("rc",         mg_vehicle_get_rc_channels(core));
    dump("servo",      mg_vehicle_get_servo_output(core));
    dump("home",       mg_vehicle_get_home_position(core));
    dump("ext_state",  mg_vehicle_get_extended_sys_state(core));
    dump("vibration",  mg_vehicle_get_vibration(core));
    dump("esc",        mg_vehicle_get_esc_status(core));
    dump("wind",       mg_vehicle_get_wind_estimate(core));
    dump("prearm",     mg_vehicle_get_prearm_check(core));
    dump("health",     mg_health_get_all(core));
    dump("modes",      mg_flight_get_available_modes(core));

    // Request params
    fprintf(stderr, "\n--- Requesting parameters ---\n");
    mg_params_request_all(core);
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
                    if (i % 10 == 0)
                        fprintf(stderr, "  Params: %d/%d\n", recv, total);
                }
            }
            mg_free_string(prog);
        }
    }

    // Some key params
    auto show_param = [&](const char* name) {
        char* p = mg_param_get(core, name);
        if (p) { fprintf(stderr, "  %s = %s\n", name, p); mg_free_string(p); }
    };

    fprintf(stderr, "\n--- Key Parameters ---\n");
    show_param("BATT_MONITOR");
    show_param("BATT_CAPACITY");
    show_param("BATT_LOW_VOLT");
    show_param("BATT_CRT_VOLT");
    show_param("GPS1_TYPE");
    show_param("GPS2_TYPE");
    show_param("FRAME_CLASS");
    show_param("FRAME_TYPE");
    show_param("ARMING_CHECK");
    show_param("MAV_SYSID");
    show_param("MAV_GCS_SYSID");
    show_param("COMPASS_ENABLE");
    show_param("WPNAV_SPEED");
    show_param("WPNAV_RADIUS");
    show_param("FLTMODE1");
    show_param("FLTMODE2");
    show_param("FLTMODE3");
    show_param("FENCE_ENABLE");
    show_param("FENCE_TYPE");
    show_param("RALLY_TOTAL");
    show_param("MIS_TOTAL");
    show_param("SERIAL0_BAUD");

    fprintf(stderr, "\n--- Geofence Config (after params) ---\n");
    dump("fence", mg_geofence_get(core));

    // Final battery check with updated data
    fprintf(stderr, "\n--- Final Battery Status ---\n");
    dump("battery", mg_vehicle_get_battery(core));

    // Cleanup
    mg_connection_disconnect(core);
    mg_destroy(core);
    fprintf(stderr, "\nDone.\n");
    return 0;
}
