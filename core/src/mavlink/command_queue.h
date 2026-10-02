/**
 * command_queue.h - 命令队列与 ACK 超时重发机制
 *
 * 参照 QGC MavCommandQueue 实现。
 * 独立于 MavlinkAdapter，可单独编译和测试。
 *
 * 核心功能：
 *   - 命令入队，返回唯一 ticket ID
 *   - ACK 匹配（按 compId + command）
 *   - 超时重发（按命令类型分级策略）
 *   - IN_PROGRESS 处理（重置超时，停止重试）
 *   - 自动清理已完成条目
 */
#ifndef MGCORE_COMMAND_QUEUE_H
#define MGCORE_COMMAND_QUEUE_H

#include "core/mavlink/mavlink_adapter.h"  // for CommandAckInfo
#include "core/platform/platform_mutex.h"
#include <vector>
#include <functional>
#include <chrono>
#include <cstdint>
#include <algorithm>

namespace mg::core::mavlink {

// ================================================================
// 命令重试安全策略（参照 QGC MavCommandQueue._shouldRetry）
// ================================================================

enum class CommandRetryPolicy : uint8_t {
    SafeToRetry,    // 查询类命令：可安全重试
    NoRetry,        // 安全关键命令：ARM/DISARM/模式切换/紧急停止，不重试
    AlwaysRetry,    // 流式命令：SET_MESSAGE_INTERVAL 等，可重复发送
};

/// 根据 MAV_CMD 返回重试策略、最大重试次数和超时时间
/// 参照 QGC: src/Vehicle/MavCommandQueue.cc _shouldRetry()
inline void classify_command(uint16_t cmd, CommandRetryPolicy& policy,
                             int& max_tries, int& timeout_ms) {
    switch (cmd) {
        // 安全关键：不重试
        case 400:  // MAV_CMD_COMPONENT_ARM_DISARM
        case 176:  // MAV_CMD_DO_SET_MODE
        case 185:  // MAV_CMD_DO_FLIGHTTERMINATION
            policy = CommandRetryPolicy::NoRetry;
            max_tries = 1; timeout_ms = 1500; return;

        // 导航命令：不重试，但给更长超时
        case 22:   // MAV_CMD_NAV_TAKEOFF
        case 21:   // MAV_CMD_NAV_LAND
            policy = CommandRetryPolicy::NoRetry;
            max_tries = 1; timeout_ms = 3000; return;
        case 20:   // MAV_CMD_NAV_RETURN_TO_LAUNCH
            policy = CommandRetryPolicy::NoRetry;
            max_tries = 1; timeout_ms = 2000; return;
        case 192:  // MAV_CMD_DO_REPOSITION
        case 31:   // MAV_CMD_DO_ORBIT
            policy = CommandRetryPolicy::NoRetry;
            max_tries = 1; timeout_ms = 2000; return;

        // 流式命令：可重复
        case 511:  // MAV_CMD_SET_MESSAGE_INTERVAL
            policy = CommandRetryPolicy::AlwaysRetry;
            max_tries = 3; timeout_ms = 1200; return;

        // 参数存储：长超时
        case 245:  // MAV_CMD_PREFLIGHT_STORAGE
            policy = CommandRetryPolicy::SafeToRetry;
            max_tries = 3; timeout_ms = 5000; return;

        // 默认：可重试
        default:
            policy = CommandRetryPolicy::SafeToRetry;
            max_tries = 3; timeout_ms = 1200; return;
    }
}

// ================================================================
// 命令队列条目
// ================================================================

/// 命令队列中的单个条目
struct CommandQueueEntry {
    uint64_t    id;               // 唯一递增 ID
    uint16_t    command;          // MAV_CMD_xxx
    uint8_t     target_sys;       // 目标系统 ID
    uint8_t     target_comp;      // 目标组件 ID
    bool        use_command_int = false;
    uint8_t     frame = 0;        // MAV_FRAME（COMMAND_INT）
    float       params[7];        // p1-p7，用于重发
    int32_t     x = 0;            // COMMAND_INT 的位置字段
    int32_t     y = 0;
    float       z = 0;
    uint8_t     confirmation = 0; // MAVLink 确认字段（重试时递增）
    int         max_tries = 3;    // 最大尝试次数
    int         try_count = 0;    // 已发送次数
    int         ack_timeout_ms = 1200;  // 超时时间
    std::chrono::steady_clock::time_point send_time;  // 最近一次发送时间
    CommandRetryPolicy policy = CommandRetryPolicy::SafeToRetry;

    enum class State : uint8_t {
        Pending,     // 等待 ACK
        Acked,       // 已收到 ACK
        TimedOut,    // 重试耗尽，无响应
        InProgress,  // 收到 IN_PROGRESS，超时已重置
    };
    State state = State::Pending;
    uint8_t ack_result = 0;       // MAV_RESULT_xxx
    uint8_t ack_progress = 0;     // 0-100 for IN_PROGRESS
    bool timed_out = false;

    // 完成回调（ACK 到达或超时时触发）
    std::function<void(const CommandAckInfo&)> result_cb;
};

// ================================================================
// 命令队列
// ================================================================

/// 命令队列 — 管理待确认的命令，处理超时重发
/// 参照 QGC MavCommandQueue，适配 mgroundcontrol 架构
struct CommandQueue {
    std::vector<CommandQueueEntry> entries;
    uint64_t next_id = 1;
    platform::ConditionVariable cv;  // 用于 wait_for_command

    /// 分配下一个唯一 ID
    uint64_t allocate_id() { return next_id++; }

    /// 按 (compId, command) 查找 Pending/InProgress 条目
    /// MAVLink ACK 只携带 (command, result)，不携带确认字段
    int find_by_command(uint8_t compid, uint16_t command) const;

    /// 按 ticket ID 查找
    int find_by_id(uint64_t id) const;

    /// 处理 ACK — 匹配 (compid, command)，更新状态
    /// IN_PROGRESS: 重置超时到 30 秒，停止重试
    /// 最终结果: 标记 Acked，通知 wait_for_command
    /// 返回匹配的条目 ID，0 表示无匹配
    uint64_t handle_ack(uint8_t compid, uint16_t command, uint8_t result, uint8_t progress);

    /// 扫描超时条目，重发或标记失败
    /// 返回已完成（TimedOut）的条目列表，由调用者通过 deferred_cbs 触发回调
    std::vector<CommandQueueEntry> check_timeouts(
        const std::function<bool(const CommandQueueEntry&)>& resend_fn);

    /// 清除已完成且超过 max_age 的条目，防止队列无限增长
    void purge(std::chrono::milliseconds max_age = std::chrono::seconds(30));
};

} // namespace mg::core::mavlink

#endif /* MGCORE_COMMAND_QUEUE_H */
