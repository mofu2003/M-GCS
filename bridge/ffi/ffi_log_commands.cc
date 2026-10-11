#define MGCORE_EXPORTS
/**
 * FFI entrypoints for the four logging families in ffi_api_logging.h:
 *   mg_onboard_log_*  onboard DataFlash list/download
 *   mg_tlog_*         link .tlog recording
 *   mg_session_log_*  local JSONL/CSV session files
 *   mg_fs_*           generic MAVLink FTP file access
 */
#include "ffi_internal.h"
#include "core/mavlink/mavlink_message_names.h"
#include "core/telemetry/json_builder.h"

#include <cstring>

namespace {

/// 回放一次调用最多搬运多少条：太小会让前端频繁过 FFI，太大等于把整段
/// 日志一次性塞进内存。前端的「读取一批」也按这个量级设计。
constexpr int kTlogReplayDefaultStep = 200;
constexpr int kTlogReplayMaxStep = 2000;

template <typename Sender>
char* send_active_request(mg_core_t* core, const char* error_reason, Sender&& sender) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    return sender(*mav, active_target_sys(core), active_target_comp(core))
        ? ok()
        : fail(MG_ERROR_SEND_FAILED, error_reason);
}

}  // namespace

MGCORE_API char* mg_onboard_log_request_list(mg_core_t* core) {
    return send_active_request(core, "log request list send failed",
        [](auto& mav, uint8_t sys, uint8_t comp) {
            return mav.log_request_list(sys, comp);
        });
}

MGCORE_API char* mg_onboard_log_get_entries(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    auto entries = mav->log_get_entries();
    const auto expected = mav->log_list_expected();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    mg::core::telemetry::JsonBuilder logs;
    logs.begin_array();
    for (const auto& entry : entries) {
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("id", static_cast<unsigned int>(entry.id));
        item.field("time_utc", static_cast<unsigned long long>(entry.time_utc));
        item.field("size_bytes", static_cast<unsigned long long>(entry.size_bytes));
        item.end_object();
        logs.append_json(item.str());
    }
    logs.end_array();
    jb.raw_field("logs", logs.str());
    jb.field("total", static_cast<int>(entries.size()));
    // 飞控声明的日志总数（LOG_ENTRY.num_logs）。0 表示还没收到任何 LOG_ENTRY；
    // 前端据此判断列表是否收齐，而不是靠“一段时间没新条目”猜。
    jb.field("expected", static_cast<unsigned int>(expected));
    jb.field("complete", expected > 0 &&
              entries.size() >= static_cast<size_t>(expected));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_onboard_log_download(mg_core_t* core, int log_id, const char* filepath) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!filepath || filepath[0] == '\0' || log_id < 0 || log_id > 65535) {
        return fail(MG_ERROR_INVALID_PARAM, "invalid log id or filepath");
    }
    auto* mav = get_active_mavlink(core);
    if (!mav->log_download_start(active_target_sys(core), active_target_comp(core),
                                 static_cast<uint16_t>(log_id), filepath)) {
        return fail(MG_ERROR_SEND_FAILED, "log download request failed");
    }
    return ok();
}

MGCORE_API char* mg_onboard_log_download_cancel(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!get_active_mavlink(core)->log_download_cancel()) {
        return fail(MG_ERROR_NOT_SUPPORTED, "no active log download");
    }
    return ok();
}

MGCORE_API char* mg_onboard_log_download_status(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    const auto status = get_active_mavlink(core)->log_download_status();
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("active", status.active || status.saving);
    jb.field("saving", status.saving);
    jb.field("completed", status.completed);
    jb.field("cancelled", status.cancelled);
    jb.field("state", status.active ? "downloading" :
                         (status.saving ? "saving" :
                          (status.completed ? "completed" :
                           (status.cancelled ? "cancelled" :
                            (status.error.empty() ? "idle" : "failed")))));
    jb.field("log_id", static_cast<unsigned int>(status.id));
    jb.field("received_bytes", static_cast<unsigned long long>(status.received_bytes));
    jb.field("total_bytes", static_cast<unsigned long long>(status.total_bytes));
    jb.field("size_declared", status.size_declared);
    jb.field("saved_bytes", static_cast<unsigned long long>(status.saved_bytes));
    jb.field("path", status.filepath);
    jb.field("message", status.error);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_onboard_log_erase(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    auto* mav = get_active_mavlink(core);
    // 擦除会作废正在传输的日志；下载进行中直接拒绝，避免半途丢失数据。
    if (mav->log_download_active()) {
        return fail(MG_ERROR_NOT_SUPPORTED, "cannot erase logs while a download is active");
    }
    if (!mav->log_erase(active_target_sys(core), active_target_comp(core))) {
        return fail(MG_ERROR_SEND_FAILED, "log erase request failed");
    }
    return ok();
}

MGCORE_API char* mg_tlog_start(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "null filepath");
    }

    if (get_active_mavlink_log(core).start_recording(filepath)) return ok();
    return fail(MG_ERROR_SEND_FAILED, "failed to start recording");
}

MGCORE_API char* mg_tlog_stop(mg_core_t* core) {
    CHECK_CORE(core);
    get_active_mavlink_log(core).stop_recording();
    return ok();
}

MGCORE_API char* mg_tlog_status(mg_core_t* core) {
    CHECK_CORE(core);
    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("recording", get_active_mavlink_log(core).is_recording());
    jb.field("file", get_active_mavlink_log(core).current_file());
    jb.field("bytes", static_cast<unsigned long long>(get_active_mavlink_log(core).bytes_written()));
    jb.end_object();
    return to_cstr(jb.str());
}

// ================================================================
// 本地 .tlog 回放
// ================================================================

MGCORE_API char* mg_tlog_replay_open(mg_core_t* core, const char* filepath) {
    CHECK_CORE(core);
    if (!filepath || filepath[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "null filepath");
    }

    auto replayer = std::make_shared<mg::core::log::FlightLogReplayer>();
    if (!replayer->open(filepath)) {
        return fail(MG_ERROR_NOT_FOUND, "cannot open tlog file");
    }
    // FFI 是「拉」模型：每次 step 读固定条数并立即返回，不做实时等待。
    // 实时播放节奏（如需）由前端按 timestamp_us 自己控制。
    replayer->set_speed(0);

    const auto info = replayer->file_info();
    // open() 的扫描已经知道文件是否半条记录截断，原样报给调用方。
    const bool truncated = replayer->truncated();

    {
        mg::core::platform::LockGuard lk(core->tlog_replay_mtx);
        core->tlog_replay = std::move(replayer);
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("file", info.filepath);
    jb.field("bytes", static_cast<unsigned long long>(info.total_bytes));
    jb.field("messages", static_cast<unsigned long long>(info.message_count));
    jb.field("duration_us", static_cast<unsigned long long>(info.duration_us));
    jb.field("first_timestamp_us", static_cast<unsigned long long>(info.first_timestamp));
    jb.field("last_timestamp_us", static_cast<unsigned long long>(info.last_timestamp));
    jb.field("truncated", truncated);
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_tlog_replay_step(mg_core_t* core, int max_messages) {
    CHECK_CORE(core);
    if (max_messages <= 0) max_messages = kTlogReplayDefaultStep;
    if (max_messages > kTlogReplayMaxStep) max_messages = kTlogReplayMaxStep;

    // 同一会话可能被两个页面/两个线程同时读：整个读取过程持有会话锁，
    // FlightLogReplayer 自身不持锁。
    mg::core::platform::LockGuard lk(core->tlog_replay_mtx);
    auto* replayer = core->tlog_replay.get();
    if (replayer == nullptr) {
        return fail(MG_ERROR_INVALID_STATE, "no tlog replay session");
    }

    mg::core::telemetry::JsonBuilder messages;
    messages.begin_array();
    int count = 0;
    bool eof = false;
    while (count < max_messages) {
        if (!replayer->next_message()) {
            eof = true;
            break;
        }
        const auto& message = replayer->current_message();
        const char* name = mg::core::mavlink::message_name(message.msgid);
        mg::core::telemetry::JsonBuilder item;
        item.begin_object();
        item.field("msgid", static_cast<unsigned int>(message.msgid));
        item.field("name", name != nullptr ? name : "UNKNOWN");
        item.field("sysid", static_cast<int>(message.sysid));
        item.field("compid", static_cast<int>(message.compid));
        item.field("timestamp_us",
                  static_cast<unsigned long long>(replayer->current_timestamp_us()));
        item.end_object();
        messages.append_json(item.str());
        ++count;
    }
    messages.end_array();

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.raw_field("messages", messages.str());
    jb.field("count", count);
    jb.field("progress", replayer->progress());
    jb.field("last_timestamp_us",
             static_cast<unsigned long long>(replayer->current_timestamp_us()));
    // 读到结尾（eof）与「结尾是半条记录」（truncated）是两件事：
    // 前者是正常读完，后者说明文件不完整，界面必须区分。
    jb.field("eof", eof);
    jb.field("truncated", eof && replayer->truncated());
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_tlog_replay_seek(mg_core_t* core, double percent) {
    CHECK_CORE(core);
    if (percent < 0.0 || percent > 1.0) {
        return fail(MG_ERROR_INVALID_PARAM, "percent must be within [0, 1]");
    }

    mg::core::platform::LockGuard lk(core->tlog_replay_mtx);
    auto* replayer = core->tlog_replay.get();
    if (replayer == nullptr) {
        return fail(MG_ERROR_INVALID_STATE, "no tlog replay session");
    }
    // seek 只按字节定位，落点之后找不到任何完整帧（空文件或已到文件尾）
    // 时报错，而不是假装跳转成功。
    if (!replayer->seek(static_cast<float>(percent))) {
        return fail(MG_ERROR_NOT_FOUND, "no replayable message at that position");
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("success", true);
    jb.field("progress", replayer->progress());
    jb.field("last_timestamp_us",
             static_cast<unsigned long long>(replayer->current_timestamp_us()));
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_tlog_replay_close(mg_core_t* core) {
    CHECK_CORE(core);
    {
        mg::core::platform::LockGuard lk(core->tlog_replay_mtx);
        core->tlog_replay.reset();
    }
    return ok();
}

MGCORE_API char* mg_fs_list_directory(mg_core_t* core, const char* remote_path) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!remote_path || remote_path[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "invalid remote directory");
    }
    if (std::strlen(remote_path) > mg::core::ftp::MavlinkFtpClient::kMaxReadSize) {
        return fail(MG_ERROR_INVALID_PARAM, "remote path too long");
    }
    if (!get_active_mavlink(core)->file_list_start(
            active_target_sys(core), active_target_comp(core), remote_path)) {
        return fail(MG_ERROR_BUSY, "file transfer already in progress");
    }
    return ok();
}

MGCORE_API char* mg_fs_download_file(mg_core_t* core, const char* remote_path,
                                      const char* local_path) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!remote_path || !local_path || remote_path[0] == '\0' ||
        local_path[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "invalid file path");
    }
    if (std::strlen(remote_path) > mg::core::ftp::MavlinkFtpClient::kMaxReadSize) {
        return fail(MG_ERROR_INVALID_PARAM, "remote path too long");
    }
    if (!get_active_mavlink(core)->file_download_start(
            active_target_sys(core), active_target_comp(core),
            remote_path, local_path)) {
        return fail(MG_ERROR_BUSY, "file transfer already in progress");
    }
    return ok();
}

MGCORE_API char* mg_fs_upload_file(mg_core_t* core, const char* local_path,
                                   const char* remote_path) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    if (!local_path || !remote_path || local_path[0] == '\0' ||
        remote_path[0] == '\0') {
        return fail(MG_ERROR_INVALID_PARAM, "invalid file path");
    }
    if (std::strlen(remote_path) > mg::core::ftp::MavlinkFtpClient::kMaxReadSize) {
        return fail(MG_ERROR_INVALID_PARAM, "remote path too long");
    }
    if (!get_active_mavlink(core)->file_upload_start(
            active_target_sys(core), active_target_comp(core),
            local_path, remote_path)) {
        return fail(MG_ERROR_BUSY, "file transfer already in progress");
    }
    return ok();
}

MGCORE_API char* mg_fs_transfer_status(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    // 状态查询在断链后也应可用：传输是因为断链失败还是被取消，界面需要看到
    // 最终结果，而不是在断开瞬间丢掉「上次传到哪」。
    const auto status = get_active_mavlink(core)->file_transfer_status();

    const char* task = "idle";
    switch (status.task) {
        case mg::core::ftp::MavlinkFtpClient::FtpTask::List: task = "list"; break;
        case mg::core::ftp::MavlinkFtpClient::FtpTask::Download: task = "download"; break;
        case mg::core::ftp::MavlinkFtpClient::FtpTask::Upload: task = "upload"; break;
        case mg::core::ftp::MavlinkFtpClient::FtpTask::None:
        default: task = "idle"; break;
    }
    const char* state = "idle";
    if (status.active) {
        state = status.cancelled ? "cancelling" : "running";
    } else if (status.finished) {
        state = status.success ? "completed"
                               : (status.cancelled ? "cancelled" : "failed");
    }

    mg::core::telemetry::JsonBuilder jb;
    jb.begin_object();
    jb.field("task", task);
    jb.field("state", state);
    jb.field("active", status.active);
    jb.field("completed", status.finished && status.success);
    jb.field("cancelled", status.cancelled);
    jb.field("target_sys", static_cast<int>(status.target_sys));
    jb.field("target_component", static_cast<int>(status.target_comp));
    jb.field("remote_path", status.remote_path);
    jb.field("local_path", status.local_path);
    jb.field("received_bytes", status.transferred_bytes);
    jb.field("total_bytes", status.total_bytes);
    jb.field("error_code", static_cast<int>(status.error_code));
    jb.field("message", status.error);

    if (status.task == mg::core::ftp::MavlinkFtpClient::FtpTask::List) {
        mg::core::telemetry::JsonBuilder items;
        items.begin_array();
        for (const auto& entry : status.entries) {
            mg::core::telemetry::JsonBuilder item;
            item.begin_object();
            item.field("name", entry.name);
            item.field("directory", entry.is_directory);
            item.field("size_bytes", static_cast<unsigned long long>(entry.size));
            item.end_object();
            items.append_json(item.str());
        }
        items.end_array();
        jb.raw_field("items", items.str());
    }
    jb.end_object();
    return to_cstr(jb.str());
}

MGCORE_API char* mg_fs_cancel(mg_core_t* core) {
    CHECK_CORE(core);
    CHECK_MAVLINK(core);
    CHECK_CONNECTED(core);
    // 取消只置位请求，真正的收尾在后台线程上完成；状态从 cancelling 变为
    // cancelled 由 mg_fs_transfer_status 汇报，避免界面把「已请求取消」当成
    // 「已经取消」。
    if (!get_active_mavlink(core)->file_transfer_cancel()) {
        return fail(MG_ERROR_NOT_SUPPORTED, "no active file transfer");
    }
    return ok();
}
