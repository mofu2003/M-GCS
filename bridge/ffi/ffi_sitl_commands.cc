#define MGCORE_EXPORTS
/**
 * SITL lifecycle FFI entrypoints.
 */
#include "ffi_internal.h"
#include "core/sitl/sitl_launcher.h"

#define CJSON_NESTING_LIMIT 10
#include "cJSON.h"

MGCORE_API char* mg_sitl_start(mg_core_t* core, const char* json_config) {
    CHECK_CORE(core);
    mg::core::sitl::SitlConfig cfg;

    if (json_config && json_config[0] != '\0') {
        cJSON* root = cJSON_Parse(json_config);
        if (root) {
            cJSON* vehicle = cJSON_GetObjectItemCaseSensitive(root, "vehicle");
            if (vehicle && cJSON_IsString(vehicle)) {
                std::string v = vehicle->valuestring;
                if (v == "copter") cfg.vehicle = mg::core::sitl::SitlVehicle::Copter;
                else if (v == "plane") cfg.vehicle = mg::core::sitl::SitlVehicle::Plane;
                else if (v == "rover") cfg.vehicle = mg::core::sitl::SitlVehicle::Rover;
            }
            cJSON* port = cJSON_GetObjectItemCaseSensitive(root, "port");
            if (port && cJSON_IsNumber(port)) cfg.port = port->valueint;
            cJSON* home = cJSON_GetObjectItemCaseSensitive(root, "home");
            if (home && cJSON_IsString(home)) cfg.home = home->valuestring;
            cJSON* speedup = cJSON_GetObjectItemCaseSensitive(root, "speedup");
            if (speedup && cJSON_IsNumber(speedup)) cfg.speedup = speedup->valuedouble;
            cJSON_Delete(root);
        }
    }

    core->sitl.set_status_callback([core](mg::core::sitl::SitlStatus status, const std::string& msg) {
        mg::bridge::JsonBuilder jb;
        jb.begin_object();
        jb.field("status", static_cast<int>(status));
        jb.field("message", msg);
        jb.end_object();
        post_event(core, MG_EVENT_CONNECTION, jb.str());
    });

    if (core->sitl.start(cfg)) {
        mg::bridge::JsonBuilder jb;
        jb.begin_object();
        jb.field("success", true);
        jb.field("pid", core->sitl.get_pid());
        jb.field("endpoint", core->sitl.get_endpoint());
        jb.end_object();
        return to_cstr(jb.str());
    }
    return fail(MG_ERROR_SEND_FAILED, "failed to start SITL");
}

MGCORE_API char* mg_sitl_stop(mg_core_t* core) {
    CHECK_CORE(core);
    core->sitl.stop();
    return ok();
}

MGCORE_API char* mg_sitl_status(mg_core_t* core) {
    CHECK_CORE(core);
    auto status = core->sitl.status();
    mg::bridge::JsonBuilder jb;
    jb.begin_object();
    const char* status_str = "stopped";
    switch (status) {
        case mg::core::sitl::SitlStatus::Stopped: status_str = "stopped"; break;
        case mg::core::sitl::SitlStatus::Starting: status_str = "starting"; break;
        case mg::core::sitl::SitlStatus::Running: status_str = "running"; break;
        case mg::core::sitl::SitlStatus::Error: status_str = "error"; break;
    }
    jb.field("status", status_str);
    jb.field("pid", core->sitl.get_pid());
    jb.field("endpoint", core->sitl.get_endpoint());
    jb.end_object();
    return to_cstr(jb.str());
}
