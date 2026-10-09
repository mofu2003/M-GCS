#define MGCORE_EXPORTS
/**
 * ffi_connection.cc - 生命周期、事件、连接与数据流相关 FFI
 *
 * 生命周期管理、事件回调、连接建立/断开、数据流请求。
 *
 * mg_create, mg_destroy, mg_set_event_callback,
 * mg_connection_list_ports, mg_connection_connect, mg_connection_disconnect,
 * mg_connection_get_state, mg_stream_request, mg_stream_set_interval,
 * mg_send_heartbeat
 */
#include "ffi_internal.h"
#include "core/platform/platform_filesystem.h"

// ================================================================
// 生命周期、事件、连接与数据流
// ================================================================

MGCORE_API mg_core_t* mg_create(void) {
    auto* core = new mg_core_t();
    core->fallback_vehicle = std::make_shared<mg::core::vehicle::Vehicle>(0);

    const std::string db_path =
        mg::core::platform::find_bundled_data_file("SensorDeviceInfo.json");
    if (!db_path.empty()) {
        core->fallback_vehicle->load_sensor_decoder(db_path);
    }

    return core;
}

MGCORE_API void mg_destroy(mg_core_t* core) {
    if (!core) return;

    // Stop submissions and drain bounded workers before releasing core state.
    // Never wait while holding command_jobs_mtx.
    std::map<uint64_t, mg_core_t::CommandJob> jobs;
    {
        mg::core::platform::LockGuard lk(core->command_jobs_mtx);
        core->command_jobs_stopping = true;
        jobs.swap(core->command_jobs);
    }
    for (auto& [ticket, job] : jobs) {
        (void)ticket;
        if (job.result.valid()) job.result.wait();
    }

    // Stop registry/heartbeat work while all core members are still alive.
    // QGC detaches vehicle consumers before destroying vehicle/link objects.
    core->vehicles.set_gcs_heartbeat_enabled(false);
    core->events.set(nullptr, nullptr);

    // Take lifetime leases: raw registry pointers can be removed by watchdogs.
    const auto snapshot = core->vehicles.get_all_vehicles_shared();
    for (const auto& vehicle : snapshot) {
        vehicle->set_event_callback(nullptr);
        core->vehicles.close_vehicle(vehicle->vehicle_id());
    }

    // fallback is destroyed after EventDispatcher due to member ordering;
    // its disconnect callbacks must not retain a reference to destroyed core.
    if (core->fallback_vehicle) {
        core->fallback_vehicle->set_event_callback(nullptr);
        core->fallback_vehicle->disconnect();
    }

    // 3. 最后释放核心对象。
    delete core;
}

MGCORE_API void mg_set_event_callback(mg_core_t* core, mg_event_callback_t cb, void* user_data) {
    if (!core) return;
    core->events.set(cb, user_data);
}

MGCORE_API char* mg_connection_list_ports(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::transport::SerialPortDiscovery discovery;
    auto ports = discovery.enumerate_ports();
    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (auto& p : ports) {
        mg::bridge::JsonBuilder port;
        port.begin_object();
        port.field("port_name",    p.port_name);
        port.field("description",  p.description);
        port.field("manufacturer", p.manufacturer);
        port.field("vendor_id",    (int)p.vendor_id);
        port.field("product_id",   (int)p.product_id);
        port.field("is_likely_fc", p.is_likely_fc);
        port.field("board_class",  p.board_class);
        port.field("board_name",   p.board_name);
        port.end_object();
        jb.append_json(port.str());
    }
    jb.end_array();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_connection_connect(mg_core_t* core, const char* port, int baud) {
    CHECK_CORE(core);
    if (!port) return fail(MG_ERROR_INVALID_PARAM, "null port");

    std::string endpoint = "serial://";
    endpoint += port;
    endpoint += "?baud=";
    endpoint += std::to_string(baud);
    const int vehicle_id = mg_vehicle_open(core, endpoint.c_str());
    if (vehicle_id < 0) {
        mg::bridge::JsonBuilder jb;
        jb.begin_object();
        jb.field("success", false);
        jb.field("error", "open failed");
        jb.field("message", std::string("无法打开串口: ") + port);
        jb.field("code", static_cast<int>(MG_ERROR_SEND_FAILED));
        jb.end_object();
        return to_cstr(jb.str());
    }
    // Legacy single-connection API keeps its historical active-vehicle
    // behavior. The multi-vehicle mg_vehicle_open() path deliberately does
    // not change focus; callers select a vehicle explicitly when needed.
    core->vehicles.set_active_vehicle(vehicle_id);
    return ok();
}

MGCORE_API char* mg_connection_disconnect(mg_core_t* core) {
    CHECK_CORE(core);
    if (auto* active = core->vehicles.active_vehicle()) {
        core->vehicles.close_vehicle(active->vehicle_id());
    } else if (core->fallback_vehicle) {
        core->fallback_vehicle->disconnect();
    }
    return ok();
}


MGCORE_API char* mg_connection_connect_tcp(mg_core_t* core, const char* endpoint) {
    CHECK_CORE(core);
    if (!endpoint || endpoint[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null endpoint");

    std::string uri = "tcp://";
    uri += endpoint;
    const int vehicle_id = mg_vehicle_open(core, uri.c_str());
    if (vehicle_id < 0) return fail(MG_ERROR_SEND_FAILED, "tcp connect failed");
    core->vehicles.set_active_vehicle(vehicle_id);
    return ok();
}

MGCORE_API char* mg_connection_connect_udp(mg_core_t* core, const char* endpoint) {
    CHECK_CORE(core);
    if (!endpoint || endpoint[0] == '\0') return fail(MG_ERROR_INVALID_PARAM, "null endpoint");

    std::string uri = "udp://";
    uri += endpoint;
    const int vehicle_id = mg_vehicle_open(core, uri.c_str());
    if (vehicle_id < 0) return fail(MG_ERROR_SEND_FAILED, "udp connect failed");
    core->vehicles.set_active_vehicle(vehicle_id);
    return ok();
}

MGCORE_API char* mg_stream_request(mg_core_t* core, int stream_id, int rate_hz) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (mav && mav->request_data_stream(active_target_sys(core), active_target_comp(core),
            static_cast<uint8_t>(stream_id), static_cast<uint16_t>(rate_hz))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_stream_set_interval(mg_core_t* core, int msg_id, int interval_us) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (msg_id < 0) return fail(MG_ERROR_INVALID_PARAM, "invalid message id");
    auto* mav = get_active_mavlink(core);
    if (mav && mav->request_message_interval(
            active_target_sys(core), active_target_comp(core),
            static_cast<uint32_t>(msg_id), static_cast<int32_t>(interval_us))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}

MGCORE_API char* mg_send_heartbeat(mg_core_t* core) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    if (mav && mav->send_heartbeat()) return ok();
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}
