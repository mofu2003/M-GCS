#define MGCORE_EXPORTS
/**
 * ffi_connection.cc - 生命周期、连接与数据流相关 FFI
 *
 * 核心实例生命周期、连接建立/断开、数据流间隔设置。
 *
 * mg_create, mg_destroy, mg_connection_list_ports, mg_stream_set_interval
 */
#include "ffi_internal.h"
#include "core/platform/platform_filesystem.h"

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

    // Take lifetime leases: raw registry pointers can be removed by watchdogs.
    const auto snapshot = core->vehicles.get_all_vehicles_shared();
    for (const auto& vehicle : snapshot) {
        vehicle->set_event_callback(nullptr);
        core->vehicles.close_vehicle(vehicle->vehicle_id());
    }

    // fallback is destroyed after the vehicle registry; its disconnect
    // callbacks must not retain a reference to the destroyed core.
    if (core->fallback_vehicle) {
        core->fallback_vehicle->set_event_callback(nullptr);
        core->fallback_vehicle->disconnect();
    }

    // 3. 最后释放核心对象。
    delete core;
}


MGCORE_API char* mg_connection_list_ports(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::transport::SerialPortDiscovery discovery;
    auto ports = discovery.enumerate_ports();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_array();
    for (auto& p : ports) {
        mg::core::telemetry::JsonBuilder port;
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





MGCORE_API char* mg_stream_set_interval(mg_core_t* core, int msg_id, int interval_us) {
    CHECK_CORE(core); CHECK_MAVLINK(core); CHECK_CONNECTED(core);
    if (msg_id < 0) return fail(MG_ERROR_INVALID_PARAM, "invalid message id");
    auto mav = shared_active_mavlink(core);
    if (mav && mav->request_message_interval(
            active_target_sys(core), active_target_comp(core),
            static_cast<uint32_t>(msg_id), static_cast<int32_t>(interval_us))) {
        return ok();
    }
    return fail(MG_ERROR_SEND_FAILED, "send failed");
}
