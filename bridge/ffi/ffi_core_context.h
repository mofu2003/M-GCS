#ifndef MGCORE_FFI_CORE_CONTEXT_H
#define MGCORE_FFI_CORE_CONTEXT_H

#include "ffi_api.h"
#include "core/mission/cad/mission_document.h"
#include "core/log/flight_log_replayer.h"
#include "core/platform/platform_mutex.h"
#include "core/settings/flight_log_writer.h"
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

    mg::core::platform::Mutex             core_mtx;
    mg::core::platform::Mutex             mission_mtx;

    mg::core::settings::FlightLogWriter      log_writer;
    // 在 core 上以跨 FFI 调用共享（旧实现每次调用新建查询对象，缓存不跨调用）。
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
