#ifndef MGCORE_FFI_API_LOGGING_H
#define MGCORE_FFI_API_LOGGING_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 四组导出按对象命名，前缀即职责；不要再用 mg_log_* 同时表示多件事。
 * 逐项状态、上游对照与缺陷记录见 docs/features/flight-logging-and-replay.md。 */

/* 机载 DataFlash 日志：列表、下载、取消、进度/结果。 */
MGCORE_API char* mg_onboard_log_request_list(mg_core_t* core);
MGCORE_API char* mg_onboard_log_get_entries(mg_core_t* core);
MGCORE_API char* mg_onboard_log_download(mg_core_t* core, int log_id, const char* filepath);
MGCORE_API char* mg_onboard_log_download_cancel(mg_core_t* core);
MGCORE_API char* mg_onboard_log_download_status(mg_core_t* core);
/* 擦除飞控上的全部机载日志（LOG_ERASE）；下载进行中会被拒绝。 */
MGCORE_API char* mg_onboard_log_erase(mg_core_t* core);

/* 链路 .tlog 录制：把链路上的 MAVLink 帧写成飞行日志文件。 */
MGCORE_API char* mg_tlog_start(mg_core_t* core, const char* filepath);
MGCORE_API char* mg_tlog_stop(mg_core_t* core);
MGCORE_API char* mg_tlog_status(mg_core_t* core);

/* 本地 .tlog 回放：离线打开文件、按批读取、跳转、关闭。
 * 与录制共用同一份磁盘格式（core/log/tlog_format.h），只读本地文件，
 * 与当前连接/车辆无关，因此断开状态下也能用。
 * 每个 core 同时只有一个回放会话；再次 open 会替换上一个，
 * 不做实时等待——读取节奏由调用方决定。
 * 耗时：open 会完整扫描文件（O(文件大小)）以统计数据与时间范围；
 * step 只读 max_messages 条；seek 按字节比例定位到次一条完整记录的起点，
 * 跳转后的第一次 step 返回该记录本身。 */
MGCORE_API char* mg_tlog_replay_open(mg_core_t* core, const char* filepath);
MGCORE_API char* mg_tlog_replay_step(mg_core_t* core, int max_messages);
MGCORE_API char* mg_tlog_replay_seek(mg_core_t* core, double percent);
MGCORE_API char* mg_tlog_replay_close(mg_core_t* core);

/* 本地会话日志：把事件写成 JSONL/CSV 会话文件。 */

/* 事件日志服务：STATUSTEXT / 连接 / 命令结果的结构化事件流。 */

/* Generic vehicle file-system access through MAVLink FTP (FILE_TRANSFER_PROTOCOL).
 *
 * 传输跑在 core 的 FTP 后台线程上，三个 start 入口立即返回，不阻塞调用线程：
 *   1. mg_fs_list_directory / mg_fs_download_file / mg_fs_upload_file 发起传输；
 *   2. mg_fs_transfer_status 轮询进度、结果（列表条目也在里面）和错误；
 *   3. mg_fs_cancel 取消正在进行的传输。
 * 同一时刻只允许一个传输（MAVLink FTP 是单会话协议）；重复发起返回 BUSY，
 * 没有活动传输时 cancel 返回 NOT_SUPPORTED。远端路径上限 239 字节。 */
MGCORE_API char* mg_fs_list_directory(mg_core_t* core, const char* remote_path);
MGCORE_API char* mg_fs_download_file(mg_core_t* core, const char* remote_path,
                                      const char* local_path);
MGCORE_API char* mg_fs_upload_file(mg_core_t* core, const char* local_path,
                                    const char* remote_path);
MGCORE_API char* mg_fs_transfer_status(mg_core_t* core);
MGCORE_API char* mg_fs_cancel(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_LOGGING_H */
