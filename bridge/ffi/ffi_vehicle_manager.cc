#define MGCORE_EXPORTS
// ffi_vehicle_manager.cc - 多机管理：打开/关闭连接、切换活跃飞行器、设备发现。
#include "ffi_internal.h"
#include "core/platform/platform_filesystem.h"
#include "core/transport/link_factory.h"
#include "core/transport/link_config.h"

// ================================================================
// mg_vehicle_open - Open a vehicle via unified endpoint URI
// ================================================================
MGCORE_API int mg_vehicle_open(mg_core_t* core, const char* endpoint) {
    if (!core || !endpoint || endpoint[0] == '\0') return -1;

    auto config = mg::core::transport::LinkConfig::from_uri(endpoint);
    auto* v = core->vehicles.open_vehicle(config, [core](int event_type, const std::string& json) {
        core->events.fire(static_cast<mg_event_type_t>(event_type), json);
    });
    if (!v) return -1;

    // Copy sensor decoder database to new vehicle if fallback runtime has it loaded.
    if (core->fallback_vehicle && core->fallback_vehicle->sensor_decoder().is_loaded()) {
        std::string db_path = mg::core::platform::find_data_file("SensorDeviceInfo.json");
        if (!db_path.empty()) {
            v->load_sensor_decoder(db_path);
        }
    }

    return v->vehicle_id();
}

// ================================================================
// mg_vehicle_close
// ================================================================
MGCORE_API char* mg_vehicle_close(mg_core_t* core, int vehicle_id) {
    CHECK_CORE(core);
    core->vehicles.close_vehicle(vehicle_id);
    return ok();
}

// ================================================================
// mg_vehicle_select
// ================================================================
MGCORE_API char* mg_vehicle_select(mg_core_t* core, int vehicle_id) {
    CHECK_CORE(core);
    if (!core->vehicles.get_vehicle(vehicle_id)) {
        return fail(MG_ERROR_NOT_FOUND, "vehicle not found");
    }
    core->vehicles.set_active_vehicle(vehicle_id);
    return ok();
}

// ================================================================
// mg_vehicle_get_active
// ================================================================
MGCORE_API int mg_vehicle_get_active(mg_core_t* core) {
    if (!core) return -1;
    auto* active = core->vehicles.active_vehicle();
    return active ? active->vehicle_id() : -1;
}

// ================================================================
// mg_discover_devices
// ================================================================
MGCORE_API char* mg_discover_devices(mg_core_t* core, const char* transport_type) {
    CHECK_CORE(core);

    mg::core::transport::LinkType lt = mg::core::transport::LinkType::Serial;
    if (transport_type) {
        std::string s = transport_type;
        if (s == "tcp") lt = mg::core::transport::LinkType::TCP;
        else if (s == "udp") lt = mg::core::transport::LinkType::UDP;
    }

    auto devices = core->vehicles.discover_devices(lt);
    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (auto& d : devices) {
        mg::bridge::JsonBuilder obj;
        obj.begin_object();
        obj.field("endpoint", d.endpoint);
        obj.field("description", d.description);
        obj.field("manufacturer", d.manufacturer);
        obj.field("is_likely_fc", d.is_likely_fc);
        obj.end_object();
        jb.append_json(obj.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}
