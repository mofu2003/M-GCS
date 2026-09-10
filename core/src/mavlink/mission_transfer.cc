/**
 * mission_transfer.cc - 任务传输状态机实现
 *
 * 从 mavlink_adapter_internal.h 提取，可独立编译和测试。
 */
#include "mission_transfer.h"
#include "core/log/logging.h"
#include "ardupilotmega/mavlink.h"

namespace mg::core::mavlink {

void MissionTransferState::reset() {
    download_total = 0; download_seq = 0;
    upload_total = 0; upload_seq = 0;
    retry_count = 0;
    resend_fn = nullptr;
    progress = mg::core::mission::MissionTransferProgress{};
    plan = mg::core::mission::MissionPlan{};
}

void MissionTransferState::record_send(uint8_t sys, uint8_t comp,
                                        std::function<void()> resend) {
    target_sys = sys;
    target_comp = comp;
    last_send_time = std::chrono::steady_clock::now();
    resend_fn = std::move(resend);
}

void MissionTransferState::record_recv() {
    retry_count = 0;  // 收到消息，重置重试计数
}

bool MissionTransferState::check_timeout() {
    using State = mg::core::mission::MissionState;
    if (progress.state == State::Idle) return false;
    if (!resend_fn) return false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_send_time).count();
    if (elapsed < MISSION_ACK_TIMEOUT_MS) return false;

    // 超时
    if (retry_count < MISSION_MAX_RETRIES) {
        retry_count++;
        last_send_time = now;
        resend_fn();
        return true;
    } else {
        progress.state = State::Idle;
        progress.error = "mission transfer timeout - no response after "
                         + std::to_string(MISSION_MAX_RETRIES) + " retries";
        progress.result = MAV_MISSION_OPERATION_CANCELLED;
        resend_fn = nullptr;
        return true;
    }
}

} // namespace mg::core::mavlink
