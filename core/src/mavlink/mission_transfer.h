/**
 * mission_transfer.h - 任务传输状态机
 *
 * 参照 QGC PlanManager 实现。
 * 独立于 MavlinkAdapter，可单独编译和测试。
 *
 * 上传流程：发送 MISSION_COUNT → 等待 FC 请求 ITEM_INT → 发送 → 等待 ACK
 * 下载流程：发送 REQUEST_LIST → 收到 COUNT → 逐个请求 ITEM_INT → 发送 ACK
 * 超时：1500ms 无响应则重发，最多 5 次
 */
#ifndef MGCORE_MISSION_TRANSFER_H
#define MGCORE_MISSION_TRANSFER_H

#include "core/mission/mission_types.h"
#include <functional>
#include <chrono>
#include <cstdint>

namespace mg::core::mavlink {

// 超时常量（参照 QGC PlanManager）
static constexpr int MISSION_ACK_TIMEOUT_MS = 1500;
static constexpr int MISSION_MAX_RETRIES = 5;

/// 任务传输状态 — 带超时重试
/// 用于 mission、rally、fence 三种传输类型
struct MissionTransferState {
    mg::core::mission::MissionPlan  plan;
    mg::core::mission::MissionTransferProgress progress;
    uint16_t download_total = 0;
    uint16_t download_seq   = 0;
    uint16_t upload_total   = 0;
    uint16_t upload_seq     = 0;
    uint64_t plan_revision = 0;
    uint64_t last_completed_download_revision = 0;
    uint64_t last_upload_revision = 0;
    uint64_t completed_download_count = 0;

    // 超时重试
    int         retry_count = 0;
    std::chrono::steady_clock::time_point last_send_time;
    uint8_t     target_sys = 0;
    uint8_t     target_comp = 0;
    std::function<void()> resend_fn;  // 重发回调

    /// 重置所有状态
    void reset();

    /// 记录一次发送（设置目标和重发回调）
    void record_send(uint8_t sys, uint8_t comp, std::function<void()> resend);

    /// 收到消息后重置重试计数
    void record_recv();

    /// 检查超时，返回 true 表示超时且已重试或放弃
    bool check_timeout();
};

/// 按 mission_type 选择对应的传输状态
/// MAV_MISSION_TYPE_RALLY → rally, MAV_MISSION_TYPE_FENCE → fence, 其他 → mission
inline MissionTransferState& select_transfer(
    MissionTransferState& mission,
    MissionTransferState& rally,
    MissionTransferState& fence,
    uint8_t mission_type) {
    if (mission_type == 2) return rally;   // MAV_MISSION_TYPE_RALLY
    if (mission_type == 1) return fence;   // MAV_MISSION_TYPE_FENCE
    return mission;                        // MAV_MISSION_TYPE_MISSION (0) 或其他
}

/// 查找当前正在上传的传输状态
inline MissionTransferState* find_uploading(
    MissionTransferState& mission,
    MissionTransferState& rally,
    MissionTransferState& fence) {
    using State = mg::core::mission::MissionState;
    if (rally.progress.state == State::Uploading) return &rally;
    if (fence.progress.state == State::Uploading) return &fence;
    if (mission.progress.state == State::Uploading) return &mission;
    return nullptr;
}

} // namespace mg::core::mavlink

#endif /* MGCORE_MISSION_TRANSFER_H */
