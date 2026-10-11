#ifndef MGCORE_FFI_CORE_CONTEXT_H
#define MGCORE_FFI_CORE_CONTEXT_H

#include "ffi_api.h"
#include "core/mission/cad/mission_document.h"
#include "core/log/flight_log_replayer.h"
#include "core/platform/platform_mutex.h"
#include "core/settings/flight_log_writer.h"
#include "core/terrain/terrain_store.h"
#include "core/video/video_decoder.h"
#include "core/sitl/sitl_launcher.h"
#include "core/vehicle/multi_vehicle_manager.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <future>
#include <cstdint>

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

    struct CommandJob {
        int vehicle_id;
        /// 该任务正在下发的命令名。并发判定按 (vehicle, command) 判重，
        /// 与 QGC MavCommandQueue 的 isPending(compId, command) 规则一致。
        std::string command_key;
        std::future<std::string> result;
    };
    mg::core::platform::Mutex command_jobs_mtx;
    std::map<uint64_t, CommandJob> command_jobs;
    uint64_t next_command_job = 1;
    bool command_jobs_stopping = false;

    EventDispatcher                        events;
    mg::core::platform::Mutex             core_mtx;
    mg::core::platform::Mutex             mission_mtx;

    mg::core::settings::FlightLogWriter      log_writer;
    // 高程数据由前端 provider 拉取后经 mg_terrain_cache_update 写入；缓存挂
    // 在 core 上以跨 FFI 调用共享（旧实现每次调用新建查询对象，缓存不跨调用）。
    mg::core::terrain::TerrainStore          terrain;
    mg::core::sitl::SitlLauncher             sitl;
    // 本地视频源解码器（S3/ADR-0008）：帧缓冲挂在 core 上，FFI 轮询拷贝取帧。
    mg::core::video::VideoDecoder            video_decoder;
    std::shared_ptr<mg::core::mission::cad::MissionDocumentService>
                                             mission_document;
    // QGC separates vehicle plans from the offline editing plan. Session IDs,
    // not sysids, isolate independent endpoints (including duplicate sysids).
    std::map<int, std::shared_ptr<mg::core::mission::cad::MissionDocumentService>>
                                             vehicle_mission_documents;
    std::vector<std::string>                 log_events;
    static constexpr size_t kMaxLogEvents = 5000;

    /// 本地 .tlog 回放会话。回放只读本地文件，与任何车辆/链路无关，
    /// 所以句柄挂在 core 上而不是 Vehicle 上；同一时刻只保留一个会话，
    /// 再次 open 会替换上一个。tlog_replay_mtx 既保护指针替换，也把
    /// 同一会话的并发读取串行化（FlightLogReplayer 内部不持锁）。
    mg::core::platform::Mutex                tlog_replay_mtx;
    std::shared_ptr<mg::core::log::FlightLogReplayer> tlog_replay;
};

#endif /* MGCORE_FFI_CORE_CONTEXT_H */
