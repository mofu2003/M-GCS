#define MGCORE_EXPORTS
/**
 * ffi_logging.cc - Section 8: Flight Logging
 *
 * 飞行日志：启停记录、事件采集、日志条目查询。
 *
 * mg_log_start, mg_log_stop, mg_log_get_status, mg_log_get_events
 *
 * FlightLoggingService captures STATUSTEXT, connection events, and command
 * results as timestamped JSON events for the flight log page.
 *
 * Event storage lives in mg_core_t::log_events (guarded by core_mtx).
 */
#include "ffi_internal.h"
#include "telemetry_bindings.h"

// ================================================================
// Public FFI Functions
// ================================================================

MGCORE_API char* mg_log_start(mg_core_t* core) {
    CHECK_CORE(core);

    // Set up the write callback to store events in mg_core_t
    get_active_flight_log(core).set_write_callback([core](const std::string& json_event_line) {
        {
        mg::core::platform::LockGuard lk(core->core_mtx);
        core->log_events.push_back(json_event_line);
        // Cap at kMaxLogEvents to prevent unbounded growth
        if (core->log_events.size() > mg_core_t::kMaxLogEvents) {
            core->log_events.erase(core->log_events.begin());
        }
        }
        // Fire FFI event so Dart side can react in real-time (lock released first)
        post_event(core, MG_EVENT_LOG, json_event_line);
    });

    if (get_active_flight_log(core).start()) {
        return ok();
    }
    return fail(MG_ERROR_CALIBRATION, "already active");
}

MGCORE_API char* mg_log_stop(mg_core_t* core) {
    CHECK_CORE(core);
    get_active_flight_log(core).stop();
    return ok();
}

MGCORE_API char* mg_log_get_status(mg_core_t* core) {
    CHECK_CORE(core);
    return to_cstr(get_active_flight_log(core).status_json());
}

MGCORE_API char* mg_log_get_events(mg_core_t* core) {
    CHECK_CORE(core);

    mg::core::platform::LockGuard lk(core->core_mtx);

    mg::bridge::JsonBuilder jb;
    jb.begin_array();
    for (const auto& e : core->log_events) {
        jb.append_json(e);
    }
    jb.end_array();
    return to_cstr(jb.str());
}

