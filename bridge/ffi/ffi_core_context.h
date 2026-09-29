#ifndef MGCORE_FFI_CORE_CONTEXT_H
#define MGCORE_FFI_CORE_CONTEXT_H

#include "ffi_api.h"
#include "core/mission/cad/mission_document.h"
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
    mutable mg::core::platform::ConditionVariable cv;
    mg_event_callback_t callback = nullptr;
    void*               user_data = nullptr;
    mutable size_t       in_flight = 0;

    void set(mg_event_callback_t cb, void* ud) {
        mg::core::platform::UniqueLock lk(mtx);
        callback = cb;
        user_data = ud;
        // Clearing the callback is the ownership boundary used by a client before
        // destroying mg_core and closing NativeCallable.  Wait until every
        // native thread that already observed the old callback has returned.
        if (!cb) {
            cv.wait(lk, [this] { return in_flight == 0; });
        }
    }

    void fire(mg_event_type_t type, const std::string& json) const {
        mg_event_callback_t cb_copy;
        void* ud_copy;
        {
            mg::core::platform::LockGuard lk(mtx);
            cb_copy = callback;
            ud_copy = user_data;
            if (cb_copy) {
                ++in_flight;
            }
        }
        if (!cb_copy) {
            return;
        }

        // Keep the in-flight count balanced even if the callback implementation
        // returns through an exception boundary in a future implementation.
        struct InFlightGuard {
            const EventDispatcher* owner;
            ~InFlightGuard() {
                mg::core::platform::UniqueLock lk(owner->mtx);
                if (--owner->in_flight == 0) {
                    owner->cv.notify_all();
                }
            }
        } guard{this};

        cb_copy(type, to_cstr(json), ud_copy);
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
    std::shared_ptr<mg::core::mission::cad::MissionDocumentService>
                                             mission_document;
    std::vector<std::string>                 log_events;
    static constexpr size_t kMaxLogEvents = 5000;
};

#endif /* MGCORE_FFI_CORE_CONTEXT_H */
