/**
 * param_cache.h - 参数缓存状态机
 *
 * 参照 QGC ParameterManager 实现。
 * 独立于 MavlinkAdapter，可单独编译和测试。
 *
 * 下载流程：Idle → WaitingList → Receiving → Recovering → Complete/Failed
 * 写入确认：PARAM_SET → 等待 PARAM_VALUE 回传比对 → 超时重发
 */
#ifndef MGCORE_PARAM_CACHE_H
#define MGCORE_PARAM_CACHE_H

#include "core/mavlink/mavlink_adapter.h"  // for ParamInfo
#include "core/firmware/firmware_adapter.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <chrono>
#include <cstdint>
#include <string>

namespace mg::core::mavlink {

// ================================================================
// 超时常量（参照 QGC ParameterManager）
// ================================================================
static constexpr int PARAM_LIST_TIMEOUT_MS = 5000;     // PARAM_REQUEST_LIST 等待首个回复
static constexpr int PARAM_LIST_MAX_RETRIES = 4;       // 列表请求最大重试
static constexpr int PARAM_RECEIVING_STALL_TIMEOUT_MS = 3000;  // 参数流停滞后开始补包
static constexpr int PARAM_MISSING_TIMEOUT_MS = 3000;  // 缺失参数等待超时
static constexpr int PARAM_MISSING_MAX_RETRIES = 5;    // 单个缺失参数最大重试
static constexpr int PARAM_WRITE_TIMEOUT_MS = 1000;    // PARAM_SET 确认超时
static constexpr int PARAM_WRITE_MAX_RETRIES = 2;      // PARAM_SET 最大重试
static constexpr int PARAM_BATCH_SIZE = 10;            // 每批请求缺失参数数量

// ================================================================
// 参数缓存
// ================================================================

struct ParamCache {
    std::unordered_map<std::string, ParamInfo> param_map;
    firmware::FirmwareAdapter fw_adapter;
    uint16_t expected_param_count = 0;

    // === 下载状态机 ===
    enum class DownloadState : uint8_t {
        Idle,           // 未在下载
        WaitingList,    // 已发 PARAM_REQUEST_LIST，等待回复
        Receiving,      // 正在接收参数流
        Recovering,     // 正在逐个请求缺失参数
        Complete,       // 全部接收完成
        Failed,         // 重试耗尽，下载失败
    };
    DownloadState download_state = DownloadState::Idle;
    bool download_cancelled = false;
    bool download_failed = false;
    std::string download_error;
    int list_retry_count = 0;
    std::chrono::steady_clock::time_point last_send_time;
    std::chrono::steady_clock::time_point last_receive_time;
    uint8_t target_sys = 0;
    uint8_t target_comp = 0;
    std::unordered_set<uint16_t> received_indices;

    // === 缺失参数跟踪 ===
    std::unordered_map<uint16_t, int> missing_indices;
    std::vector<uint16_t> batch_queue;
    std::unordered_set<uint16_t> failed_indices;
    std::chrono::steady_clock::time_point missing_timeout_start;

    // === 写入确认跟踪 ===
    struct PendingWrite {
        std::string name;
        float value;
        uint8_t type;
        uint8_t target_sys = 0;
        uint8_t target_comp = 0;
        uint64_t history_id = 0;
        int retry_count = 0;
        std::chrono::steady_clock::time_point send_time;
        std::function<void(bool success)> result_cb;
    };
    std::unordered_map<std::string, PendingWrite> pending_writes;
    std::vector<PendingWrite> write_retry_queue;

    enum class WriteState : uint8_t {
        Pending = 0,
        Confirmed = 1,
        Failed = 2,
    };

    struct WriteRecord {
        uint64_t id = 0;
        std::string name;
        float requested_value = 0.0f;
        float actual_value = 0.0f;
        uint8_t type = 0;
        WriteState state = WriteState::Pending;
        std::string error;
        int64_t requested_at_ms = 0;
        int64_t completed_at_ms = 0;
    };

    static constexpr size_t kMaxWriteHistory = 100;
    std::vector<WriteRecord> write_history;
    uint64_t next_write_id = 1;

    /// 启动下载
    void start_download(uint8_t sys, uint8_t comp);

    /// 取消当前下载；没有活动下载时返回 false。
    bool cancel_download();

    /// 记录一次参数写入请求并进入 pending 状态。
    uint64_t record_write_request(
        const std::string& name,
        float value,
        uint8_t type,
        uint8_t target_sys = 0,
        uint8_t target_comp = 0);

    /// 更新参数写入终态；找不到记录时返回 false。
    bool complete_write(
        uint64_t history_id,
        bool success,
        float actual_value,
        const std::string& error = {});

    /// 收到 PARAM_VALUE 后更新状态
    void on_param_received(uint16_t index);

    /// 检查超时
    /// 返回值: 0=无操作, 1=重试列表请求, 2=请求缺失参数批次, 3=超时放弃
    /// 写入超时通过 deferred_cbs 回调通知
    int check_timeouts(std::vector<std::function<void()>>& deferred_cbs);
};

} // namespace mg::core::mavlink

#endif /* MGCORE_PARAM_CACHE_H */
