#ifndef MGCORE_FFI_CORE_CONTEXT_H
#define MGCORE_FFI_CORE_CONTEXT_H

#include "ffi_api.h"
#include "core/platform/platform_mutex.h"
#include "core/settings/flight_log_writer.h"
#include "core/sitl/sitl_launcher.h"
#include "core/vehicle/multi_vehicle_manager.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

inline char* to_cstr(const std::string& s) {
    char* c = static_cast<char*>(malloc(s.size() + 1));
    if (c) memcpy(c, s.c_str(), s.size() + 1);
    return c;
}

struct EventDispatcher {
    mutable mg::core::platform::Mutex mtx;
    mg_event_callback_t callback = nullptr;
    void*               user_data = nullptr;

    void set(mg_event_callback_t cb, void* ud) {
        mg::core::platform::LockGuard lk(mtx);
        callback = cb;
        user_data = ud;
    }

    void fire(mg_event_type_t type, const std::string& json) const {
        mg_event_callback_t cb_copy;
        void* ud_copy;
        {
            mg::core::platform::LockGuard lk(mtx);
            cb_copy = callback;
            ud_copy = user_data;
        }
        if (cb_copy) cb_copy(type, to_cstr(json), ud_copy);
    }
};

struct mg_core_t {
    mg::core::vehicle::MultiVehicleManager vehicles;
    std::shared_ptr<mg::core::vehicle::Vehicle> fallback_vehicle;

    EventDispatcher                        events;
    mg::core::platform::Mutex             core_mtx;
    mg::core::platform::Mutex             mission_mtx;

    mg::core::settings::FlightLogWriter      log_writer;
    mg::core::sitl::SitlLauncher             sitl;
    std::vector<std::string>                 log_events;
    static constexpr size_t kMaxLogEvents = 5000;
};

#endif /* MGCORE_FFI_CORE_CONTEXT_H */
