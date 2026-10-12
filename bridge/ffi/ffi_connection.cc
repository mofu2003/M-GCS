#define MGCORE_EXPORTS
/**
 * ffi_connection.cc - 生命周期、事件、连接与数据流相关 FFI
 *
 * 生命周期管理、事件回调、连接建立/断开、数据流间隔设置。
 *
 * mg_create, mg_destroy, mg_set_event_callback,
 * mg_connection_list_ports, mg_connection_connect, mg_connection_disconnect,
 * mg_connection_get_state, mg_stream_set_interval
 */
#include "ffi_internal.h"
#include "core/platform/platform_filesystem.h"
#include "core/event/event_codes.h"

// A1：core 不能 include bridge 头（core/include/core/event/event_codes.h 里有一份
// 自己的事件码），所以用 static_assert 把两份枚举钉在一起：任何一侧改值或改顺序
// 都会在这里编译失败。
#define MG_ASSERT_EVENT(core_name, ffi_name) \
    static_assert(static_cast<int>(mg::core::event::core_name) == ffi_name, \
                  #core_name " must match " #ffi_name)
MG_ASSERT_EVENT(kHeartbeat, MG_EVENT_HEARTBEAT);
MG_ASSERT_EVENT(kSysStatus, MG_EVENT_SYS_STATUS);
MG_ASSERT_EVENT(kGps, MG_EVENT_GPS);
MG_ASSERT_EVENT(kAttitude, MG_EVENT_ATTITUDE);
MG_ASSERT_EVENT(kConnection, MG_EVENT_CONNECTION);
MG_ASSERT_EVENT(kParameters, MG_EVENT_PARAMETERS);
MG_ASSERT_EVENT(kMission, MG_EVENT_MISSION);
MG_ASSERT_EVENT(kLog, MG_EVENT_LOG);
MG_ASSERT_EVENT(kTelemetry, MG_EVENT_TELEMETRY);
MG_ASSERT_EVENT(kFirmware, MG_EVENT_FIRMWARE);
MG_ASSERT_EVENT(kSensorHealth, MG_EVENT_SENSOR_HEALTH);
MG_ASSERT_EVENT(kImu, MG_EVENT_IMU);
MG_ASSERT_EVENT(kPressure, MG_EVENT_PRESSURE);
MG_ASSERT_EVENT(kGlobalPosition, MG_EVENT_GLOBAL_POSITION);
MG_ASSERT_EVENT(kHomePosition, MG_EVENT_HOME_POSITION);
MG_ASSERT_EVENT(kRcChannels, MG_EVENT_RC_CHANNELS);
MG_ASSERT_EVENT(kServoOutput, MG_EVENT_SERVO_OUTPUT);
MG_ASSERT_EVENT(kStatustext, MG_EVENT_STATUSTEXT);
MG_ASSERT_EVENT(kBattery, MG_EVENT_BATTERY);
MG_ASSERT_EVENT(kHighresImu, MG_EVENT_HIGHRES_IMU);
MG_ASSERT_EVENT(kExtendedSysState, MG_EVENT_EXTENDED_SYS_STATE);
MG_ASSERT_EVENT(kInitialConnect, MG_EVENT_INITIAL_CONNECT);
MG_ASSERT_EVENT(kVibration, MG_EVENT_VIBRATION);
MG_ASSERT_EVENT(kEscStatus, MG_EVENT_ESC_STATUS);
MG_ASSERT_EVENT(kWindEstimate, MG_EVENT_WIND_ESTIMATE);
MG_ASSERT_EVENT(kPrearmCheck, MG_EVENT_PREARM_CHECK);
MG_ASSERT_EVENT(kWaypointReached, MG_EVENT_WAYPOINT_REACHED);
MG_ASSERT_EVENT(kDistanceSensor, MG_EVENT_DISTANCE_SENSOR);
MG_ASSERT_EVENT(kBatteryFailsafe, MG_EVENT_BATTERY_FAILSAFE);
MG_ASSERT_EVENT(kAdsb, MG_EVENT_ADSB);
#undef MG_ASSERT_EVENT

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

MGCORE_API char* mg_connection_connect(mg_core_t* core, const char* port, int baud) {
    CHECK_CORE(core);
    if (!port) return fail(MG_ERROR_INVALID_PARAM, "null port");

    std::string endpoint = "serial://";
    endpoint += port;
    endpoint += "?baud=";
    endpoint += std::to_string(baud);
    const int vehicle_id = mg_vehicle_open(core, endpoint.c_str());
    if (vehicle_id < 0) {
        mg::core::telemetry::JsonBuilder jb;
        jb.begin_object();
        jb.field("success", false);
        write_error(jb, MG_ERROR_SEND_FAILED,
                    std::string("无法打开串口: ") + port);
        jb.end_object();
        return to_cstr(jb.str());
    }
    // Convenience connection calls select their newly opened vehicle so the
    // active-target command API has an explicit current target.
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
