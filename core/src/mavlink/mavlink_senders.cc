/**
 * mavlink_senders.cc - 所有发出的 MAVLink 消息函数
 *
 * Split from monolithic mavlink_adapter.cc for maintainability.
 * See mavlink_adapter_internal.h for Impl struct.
 *
 * Contains:
 *   - GCS heartbeat
 *   - Data stream requests
 *   - Parameter read/write
 *   - Autopilot version request
 *   - COMMAND_LONG commands (arm, mode, RTL, takeoff, land, emergency stop)
 *
 * References:
 *   MAVLink: third_party/mavlink/include/mavlink/v2.0/
 *   QGC:     src/Vehicle/Vehicle.cc
 *   MP:      ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

static uint8_t resolve_mode_base_flag(uint8_t base_mode) {
    return base_mode == 0 ? static_cast<uint8_t>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED) : base_mode;
}

// ================================================================
// GCS 心跳 + 数据流请求
// ================================================================

// 发送地面站心跳包
bool MavlinkAdapter::send_heartbeat() {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_heartbeat_pack(
        255, 0, &impl_->msg,
        MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID,
        0, 0, 0);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 请求数据流（旧方式，已被 MAV_CMD_SET_MESSAGE_INTERVAL 替代）
bool MavlinkAdapter::request_data_stream(uint8_t target_system,
                                          uint8_t target_component,
                                          uint8_t stream_id,
                                          uint16_t rate_hz) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_request_data_stream_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        stream_id, rate_hz, 1);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// ================================================================
// 版本和参数请求
// ================================================================

// 请求飞控版本信息
bool MavlinkAdapter::request_autopilot_version(uint8_t target_system,
                                                uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_command_long_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        MAV_CMD_REQUEST_MESSAGE,
        0, static_cast<float>(MAVLINK_MSG_ID_AUTOPILOT_VERSION),
        0, 0, 0, 0, 0, 0);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 请求 home 点：MAV_CMD_GET_HOME_POSITION，飞控会回一条 HOME_POSITION。
// 与 Mission Planner 的 getHomePositionAsync 一致（它在此基础上做 3 次重试）。
bool MavlinkAdapter::request_home_position(uint8_t target_system,
                                            uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_command_long_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        MAV_CMD_GET_HOME_POSITION,
        0, 0, 0, 0, 0, 0, 0, 0);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 请求完整参数列表（带超时重试状态初始化）
bool MavlinkAdapter::request_param_list(uint8_t target_system,
                                         uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    impl_->pc.param_map.clear();
    impl_->pc.expected_param_count = 0;
    impl_->pc.start_download(target_system, target_component);
    mavlink_msg_param_request_list_pack(
        255, 0, &impl_->msg,
        target_system, target_component);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

bool MavlinkAdapter::request_param_read_by_name(uint8_t target_system,
                                                 uint8_t target_component,
                                                 const char* param_id) {
    if (!param_id || param_id[0] == '\0') return false;
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_param_request_read_pack(
        255, 0, &impl_->msg,
        target_system, target_component, param_id, -1);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 发送参数设置命令（委托给 Status 版本）
bool MavlinkAdapter::set_param(uint8_t target_system, uint8_t target_component,
                               const char* param_id, float param_value, uint8_t param_type) {
    return static_cast<bool>(
        set_param_status(target_system, target_component, param_id, param_value, param_type));
}

// ================================================================
// COMMAND_LONG — 通用和特定命令
// ================================================================

// 设置消息发送间隔
bool MavlinkAdapter::request_message_interval(uint8_t target_system, uint8_t target_component,
                                               uint32_t msg_id, int32_t interval_us) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_SET_MESSAGE_INTERVAL, 0,
                             static_cast<float>(msg_id),
                             static_cast<float>(interval_us),
                             0, 0, 0, 0, 0);
}
// 发送通用 COMMAND_LONG（委托给 Status 版本，消除重复）
bool MavlinkAdapter::send_command_long(uint8_t target_system, uint8_t target_component,
                                        uint16_t command, uint8_t confirmation,
                                        float p1, float p2, float p3, float p4,
                                        float p5, float p6, float p7) {
    return static_cast<bool>(
        send_command_long_status(target_system, target_component,
                                 command, confirmation,
                                 p1, p2, p3, p4, p5, p6, p7));
}

// 解锁/锁定电机
bool MavlinkAdapter::arm_disarm(uint8_t target_system, uint8_t target_component,
                                 bool arm, bool force) {
    float force_param = force ? 21196.0f : 0.0f;
    return send_command_long(target_system, target_component,
                             MAV_CMD_COMPONENT_ARM_DISARM, 0,
                             arm ? 1.0f : 0.0f,
                             force_param,
                             0, 0, 0, 0, 0);
}

// 设置飞行模式
bool MavlinkAdapter::set_mode(uint8_t target_system, uint8_t target_component,
                               uint8_t base_mode, uint32_t custom_mode) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_DO_SET_MODE, 0,
                             static_cast<float>(resolve_mode_base_flag(base_mode)),
                             static_cast<float>(custom_mode),
                             0, 0, 0, 0, 0);
}

// 返航命令
bool MavlinkAdapter::cmd_return_to_launch(uint8_t target_system, uint8_t target_component) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_NAV_RETURN_TO_LAUNCH, 0,
                             0, 0, 0, 0, 0, 0, 0);
}

// 起飞命令
bool MavlinkAdapter::cmd_takeoff(uint8_t target_system, uint8_t target_component,
                                  float altitude_m) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_NAV_TAKEOFF, 0,
                             0, 0, 0, 0,
                             0, 0,
                             altitude_m);
}

// 降落命令
bool MavlinkAdapter::cmd_land(uint8_t target_system, uint8_t target_component) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_NAV_LAND, 0,
                             0, 0, 0, 0, 0, 0, 0);
}

// ================================================================
// GUIDED 模式指令
// ================================================================

// GUIDED: 飞往指定经纬度高度位置（使用 COMMAND_INT 保证精度）
bool MavlinkAdapter::cmd_guided_point(uint8_t target_system, uint8_t target_component,
                                      double lat_deg, double lon_deg, float alt_m,
                                      float yaw_deg, float speed_ms) {
    // MAV_CMD_DO_REPOSITION (192)
    // param1: ground speed (< 0 for default)
    // param2: bitmask (bit 1 = GUIDED mode)
    // param3: loiter radius (0 for copter)
    // param4: yaw heading (NaN = current)
    // param5: latitude (degE7)
    // param6: longitude (degE7)
    // param7: altitude (meters MSL)
    float p1 = speed_ms > 0 ? speed_ms : -1.0f;
    float p2 = 1.0f;  // MAV_DO_REPOSITION_FLAGS_MODE_GUIDED
    float p4 = (yaw_deg >= 0) ? yaw_deg : NAN;
    int32_t lat_e7 = static_cast<int32_t>(lat_deg * 1e7);
    int32_t lon_e7 = static_cast<int32_t>(lon_deg * 1e7);

    // Use COMMAND_INT for lat/lon precision (requires int32 fields)
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_command_int_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        MAV_FRAME_GLOBAL,  // AMSL altitude, matching QGC APM guided goto
        MAV_CMD_DO_REPOSITION,
        0,          // current (0=not current)
        1,          // autocontinue
        p1, p2, 0, p4,
        lat_e7, lon_e7, alt_m);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// GUIDED: 以指定速度矢量飞行（SET_POSITION_TARGET_GLOBAL_INT）
bool MavlinkAdapter::cmd_guided_velocity(uint8_t target_system, uint8_t target_component,
                                         float vx_ms, float vy_ms, float vz_ms) {
    // SET_POSITION_TARGET_GLOBAL_INT (86) with velocity-only type mask
    // type_mask: 0b0000_1111_1100_0111 = 0x0FC7
    //   bit 0-2: ignore position (x,y,z)
    //   bit 3-5: ignore velocity (x,y,z) — we SET these
    //   bit 6-8: ignore accel (x,y,z)
    //   bit 9:   ignore yaw, bit 10: ignore yaw rate
    uint16_t type_mask = 0x0FC7;  // velocity only

    // Use global position (0,0,0) since we only care about velocity
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_set_position_target_global_int_pack(
        255, 0, &impl_->msg,
        0,          // time_boot_ms (ignored)
        target_system, target_component,
        MAV_FRAME_GLOBAL,
        type_mask,
        0, 0, 0,    // lat/lon/alt (ignored)
        vx_ms, vy_ms, vz_ms,  // velocity (NED)
        0, 0, 0,    // accel (ignored)
        0, 0);      // yaw, yaw_rate (ignored)
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// GUIDED: 暂停（发送零速度）
bool MavlinkAdapter::cmd_guided_pause(uint8_t target_system, uint8_t target_component) {
    return cmd_guided_velocity(target_system, target_component, 0, 0, 0);
}

// GUIDED: 改变高度（照 QGC `APMFirmwarePlugin::guidedModeChangeAltitude`）
//
// QGC 发的是 SET_POSITION_TARGET_LOCAL_NED + MAV_FRAME_LOCAL_OFFSET_NED，
// type_mask=0xFFF8（只 position 的 x/y/z 有效），x=y=0、z=-高度变化量。
// NED 里 z 向下为正，所以上升要发负值。
bool MavlinkAdapter::cmd_change_altitude(uint8_t target_system, uint8_t target_component,
                                          float altitude_change_m) {
    const uint16_t type_mask = 0xFFF8;  // 只使用 position x/y/z
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_set_position_target_local_ned_pack(
        255, 0, &impl_->msg,
        0,                      // time_boot_ms (ignored)
        target_system, target_component,
        MAV_FRAME_LOCAL_OFFSET_NED,
        type_mask,
        0, 0, -altitude_change_m,   // x/y 不动，z 向下为正
        0, 0, 0,                    // velocity (ignored)
        0, 0, 0,                    // accel (ignored)
        0, 0);                      // yaw / yaw_rate (ignored)
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}


// ================================================================
// 任务协议
// ================================================================

// 从飞控下载任务
bool MavlinkAdapter::mission_download(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (has_active_transfer(impl_->mission, impl_->rally, impl_->fence)) {
        return false;
    }
    return mission_type_download(
        target_system, target_component, impl_->mission, MAV_MISSION_TYPE_MISSION, lk);
}

// 上传任务到飞控
bool MavlinkAdapter::mission_upload(uint8_t target_system, uint8_t target_component,
                                     const mg::core::mission::MissionPlan& plan) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (plan.items.empty()) return false;

    if (has_active_transfer(impl_->mission, impl_->rally, impl_->fence)) {
        return false;
    }
    return mission_type_upload(
        target_system, target_component, impl_->mission, plan, MAV_MISSION_TYPE_MISSION, lk);
}

bool MavlinkAdapter::mission_cancel() {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    return mission_type_cancel(impl_->mission, MAV_MISSION_TYPE_MISSION, lk);
}

bool MavlinkAdapter::mission_type_cancel(MissionTransferState& state,
                                          uint8_t mission_type,
                                          mg::core::platform::UniqueLock& lk) {
    using State = mg::core::mission::MissionState;
    if (state.progress.state == State::Idle) return false;

    const bool was_downloading = state.progress.state == State::Downloading;
    const uint8_t target_system = state.target_sys;
    const uint8_t target_component = state.target_comp;

    state.resend_fn = nullptr;
    state.retry_count = 0;
    state.download_seq = 0;
    state.upload_seq = 0;
    state.progress.state = State::Idle;
    state.progress.result = MAV_MISSION_OPERATION_CANCELLED;
    state.progress.error = mission_type == MAV_MISSION_TYPE_RALLY
        ? "rally transfer cancelled"
        : mission_type == MAV_MISSION_TYPE_FENCE
            ? "geofence transfer cancelled"
            : "mission transfer cancelled";

    // A partial download must never be treated as a valid vehicle mission.
    if (was_downloading) {
        state.plan.clear();
    }

    if (target_system != 0) {
        mavlink_msg_mission_ack_pack(
            255,
            0,
            &impl_->msg,
            target_system,
            target_component,
            MAV_MISSION_OPERATION_CANCELLED,
            mission_type,
            0);
        // KD-22：传输层回调在锁外执行（A3/A4）。
        impl_->send_msg_unlocked(lk);
    }
    return true;
}

// 清除飞控上所有任务
bool MavlinkAdapter::mission_clear(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    impl_->mission.plan.clear();
    ++impl_->mission.plan_revision;
    impl_->mission.progress = mg::core::mission::MissionTransferProgress{};
    
    mavlink_msg_mission_clear_all_pack(
        255, 0, &impl_->msg,
        target_system, target_component, MAV_MISSION_TYPE_MISSION);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 设置当前航点
bool MavlinkAdapter::mission_set_current(uint8_t target_system, uint8_t target_component,
                                          uint16_t seq) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_mission_set_current_pack(
        255, 0, &impl_->msg,
        target_system, target_component, seq);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O）。
    lk.unlock();
    const bool ok = impl_->send_msg();
    if (ok) {
        lk.lock();  // 回写任务进度需要重新加锁
        if (impl_->mission.plan.current_index != seq) {
            impl_->mission.plan.current_index = seq;
            ++impl_->mission.plan_revision;
        }
    }
    return ok;
}

mg::core::mission::MissionPlan MavlinkAdapter::mission_get_plan() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->mission.plan;
}

mg::core::mission::MissionTransferProgress MavlinkAdapter::mission_get_progress() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->mission.progress;
}

int MavlinkAdapter::mission_transfer_retry_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->mission.retry_count;
}

mg::core::mission::MissionPlanProvenance
MavlinkAdapter::mission_get_plan_provenance() const {
    return mission_get_plan_snapshot().provenance;
}

mg::core::mission::MissionPlanSnapshot
MavlinkAdapter::mission_get_plan_snapshot() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    mg::core::mission::MissionPlanSnapshot snapshot;
    snapshot.plan = impl_->mission.plan;
    snapshot.provenance.plan_revision = impl_->mission.plan_revision;
    snapshot.provenance.last_completed_download_revision =
        impl_->mission.last_completed_download_revision;
    snapshot.provenance.last_upload_revision =
        impl_->mission.last_upload_revision;
    snapshot.provenance.completed_download_count =
        impl_->mission.completed_download_count;
    return snapshot;
}

MavlinkAdapter::MissionExecutionProgress
MavlinkAdapter::mission_get_execution_progress() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    MissionExecutionProgress progress;
    const auto& execution = impl_->mission;
    progress.total = static_cast<uint32_t>(execution.plan.items.size());
    progress.valid = progress.total > 0;
    progress.current_seq = execution.plan.current_index;
    progress.reached_count = execution.reached_count;
    progress.last_reached_seq = execution.last_reached_seq;
    progress.has_reached = execution.has_reached;
    // 原始 seq 的最后一项就是任务最后一项，收到它的 REACHED 即代表航段飞完。
    progress.reached_end =
        progress.valid && execution.has_reached &&
        static_cast<uint32_t>(execution.last_reached_seq) + 1 >= progress.total;
    return progress;
}

// ================================================================
// 集结点（MAV_MISSION_TYPE_RALLY = 2）
// ================================================================

// 内部通用下载（消除 mission/rally/fence 三重复制）
bool MavlinkAdapter::mission_type_download(uint8_t sys, uint8_t comp,
                                            MissionTransferState& state, uint8_t mission_type,
                                            mg::core::platform::UniqueLock& lk) {
    state.plan.clear();
    state.clear_execution_progress();
    ++state.plan_revision;
    state.download_total = 0;
    state.download_seq = 0;
    state.progress = mg::core::mission::MissionTransferProgress{};
    state.progress.state = mg::core::mission::MissionState::Downloading;

    mavlink_msg_mission_request_list_pack(255, 0, &impl_->msg, sys, comp, mission_type);
    // KD-22：先清掉重发回调，让临时解锁期间 check_timeout() 不会用旧回调重发。
    state.resend_fn = nullptr;
    state.last_send_time = std::chrono::steady_clock::now();
    bool ok = impl_->send_msg_unlocked(lk);
    if (ok) {
        state.record_send(sys, comp, [this, sys, comp, mission_type]() {
            mavlink_msg_mission_request_list_pack(255, 0, &impl_->msg, sys, comp, mission_type);
            impl_->queue_msg();
        });
    } else {
        state.resend_fn = nullptr;
        state.progress.state = mg::core::mission::MissionState::Idle;
        // 发送失败不是"用户取消"。
        state.progress.result = MAV_MISSION_ERROR;
        state.progress.error = "failed to send mission request list";
    }
    return ok;
}

// 内部通用上传
bool MavlinkAdapter::mission_type_upload(uint8_t sys, uint8_t comp,
                                          MissionTransferState& state,
                                          const mg::core::mission::MissionPlan& plan,
                                          uint8_t mission_type,
                                          mg::core::platform::UniqueLock& lk) {
    if (plan.items.empty()) return false;

    // 先备份：ACK 之前飞控并没有这份任务，失败要能退回去。
    state.begin_upload_backup();
    state.plan = plan;
    state.clear_execution_progress();
    state.reached_flags.assign(plan.items.size(), 0);
    ++state.plan_revision;
    state.last_upload_revision = state.plan_revision;
    state.upload_total = static_cast<uint16_t>(plan.items.size());
    state.upload_seq = 0;
    state.progress = mg::core::mission::MissionTransferProgress{};
    state.progress.state = mg::core::mission::MissionState::Uploading;
    state.progress.total = state.upload_total;

    mavlink_msg_mission_count_pack(255, 0, &impl_->msg, sys, comp, state.upload_total, mission_type, 0);
    // KD-22：同上，临时解锁期间不允许旧回调接管重发。
    state.resend_fn = nullptr;
    state.last_send_time = std::chrono::steady_clock::now();
    bool ok = impl_->send_msg_unlocked(lk);
    if (ok) {
        state.record_send(sys, comp, [this, &state, sys, comp, mission_type]() {
            mavlink_msg_mission_count_pack(255, 0, &impl_->msg, sys, comp, state.upload_total, mission_type, 0);
            impl_->queue_msg();
        });
    } else {
        state.resend_fn = nullptr;
        state.progress.state = mg::core::mission::MissionState::Idle;
        // 发送失败不是"用户取消"。
        state.progress.result = MAV_MISSION_ERROR;
        state.progress.error = "failed to send mission count";
        state.rollback_upload();
    }
    return ok;
}

// 内部通用清除
bool MavlinkAdapter::mission_type_clear(uint8_t sys, uint8_t comp,
                                         MissionTransferState& state, uint8_t mission_type,
                                         mg::core::platform::UniqueLock& lk) {
    // 清除同样是"飞控尚未确认"的事务：先备份，发送失败就退回去，
    // 否则本地任务会凭空消失而飞控里还在。
    state.begin_upload_backup();
    state.plan.clear();
    state.progress = mg::core::mission::MissionTransferProgress{};

    mavlink_msg_mission_clear_all_pack(255, 0, &impl_->msg, sys, comp, mission_type);
    state.resend_fn = nullptr;
    state.last_send_time = std::chrono::steady_clock::now();
    const bool ok = impl_->send_msg_unlocked(lk);
    if (!ok) {
        state.rollback_upload();
        return false;
    }
    // 记录目标飞控，MISSION_ACK 来源校验要用（不设 resend：清除命令不自动重试）。
    state.record_send(sys, comp, nullptr);
    return true;
}

// 公开 API：delegate to generic helpers

bool MavlinkAdapter::rally_download(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (has_active_transfer(impl_->mission, impl_->rally, impl_->fence)) return false;
    return mission_type_download(target_system, target_component, impl_->rally,
                                 MAV_MISSION_TYPE_RALLY, lk);
}

bool MavlinkAdapter::rally_upload(uint8_t target_system, uint8_t target_component,
                                   const mg::core::mission::MissionPlan& plan) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (has_active_transfer(impl_->mission, impl_->rally, impl_->fence)) return false;
    return mission_type_upload(target_system, target_component, impl_->rally, plan,
                               MAV_MISSION_TYPE_RALLY, lk);
}

bool MavlinkAdapter::rally_cancel() {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    return mission_type_cancel(impl_->rally, MAV_MISSION_TYPE_RALLY, lk);
}

bool MavlinkAdapter::rally_clear(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    return mission_type_clear(target_system, target_component, impl_->rally,
                              MAV_MISSION_TYPE_RALLY, lk);
}

mg::core::mission::MissionPlan MavlinkAdapter::rally_get_plan() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->rally.plan;
}

mg::core::mission::MissionPlanSnapshot
MavlinkAdapter::rally_get_plan_snapshot() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    mg::core::mission::MissionPlanSnapshot snapshot;
    snapshot.plan = impl_->rally.plan;
    snapshot.provenance.plan_revision = impl_->rally.plan_revision;
    snapshot.provenance.last_completed_download_revision =
        impl_->rally.last_completed_download_revision;
    snapshot.provenance.last_upload_revision =
        impl_->rally.last_upload_revision;
    snapshot.provenance.completed_download_count =
        impl_->rally.completed_download_count;
    return snapshot;
}

mg::core::mission::MissionTransferProgress MavlinkAdapter::rally_get_progress() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->rally.progress;
}

int MavlinkAdapter::rally_transfer_retry_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->rally.retry_count;
}

// ================================================================
// 地理围栏（MAV_MISSION_TYPE_FENCE = 1）
// ================================================================

bool MavlinkAdapter::fence_download(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (has_active_transfer(impl_->mission, impl_->rally, impl_->fence)) return false;
    return mission_type_download(target_system, target_component, impl_->fence,
                                 MAV_MISSION_TYPE_FENCE, lk);
}

bool MavlinkAdapter::fence_upload(uint8_t target_system, uint8_t target_component,
                                   const mg::core::mission::MissionPlan& plan) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (has_active_transfer(impl_->mission, impl_->rally, impl_->fence)) return false;
    return mission_type_upload(target_system, target_component, impl_->fence, plan,
                               MAV_MISSION_TYPE_FENCE, lk);
}

bool MavlinkAdapter::fence_cancel() {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    return mission_type_cancel(impl_->fence, MAV_MISSION_TYPE_FENCE, lk);
}

bool MavlinkAdapter::fence_clear(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    return mission_type_clear(target_system, target_component, impl_->fence,
                              MAV_MISSION_TYPE_FENCE, lk);
}

mg::core::mission::MissionPlan MavlinkAdapter::fence_get_plan() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->fence.plan;
}

mg::core::mission::MissionPlanSnapshot
MavlinkAdapter::fence_get_plan_snapshot() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    mg::core::mission::MissionPlanSnapshot snapshot;
    snapshot.plan = impl_->fence.plan;
    snapshot.provenance.plan_revision = impl_->fence.plan_revision;
    snapshot.provenance.last_completed_download_revision =
        impl_->fence.last_completed_download_revision;
    snapshot.provenance.last_upload_revision =
        impl_->fence.last_upload_revision;
    snapshot.provenance.completed_download_count =
        impl_->fence.completed_download_count;
    return snapshot;
}

mg::core::mission::MissionTransferProgress MavlinkAdapter::fence_get_progress() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->fence.progress;
}

int MavlinkAdapter::fence_transfer_retry_count() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->fence.retry_count;
}

// ================================================================
// 日志列表（LOG_REQUEST_LIST 协议）
// ================================================================

// 请求日志条目列表
bool MavlinkAdapter::log_request_list(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    impl_->log_state.entries.clear();
    impl_->log_state.list_num_logs = 0;

    mavlink_msg_log_request_list_pack(
        255, 0, &impl_->msg,
        target_system, target_component, 0, 0xFFFF);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

std::vector<MavlinkAdapter::LogEntry> MavlinkAdapter::log_get_entries() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->log_state.entries;
}

uint16_t MavlinkAdapter::log_list_expected() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->log_state.list_num_logs;
}
// ================================================================
// 带 Status 返回值的版本（丰富错误信息）
//
// 委托给现有 bool 返回函数，将结果包装为 Status。
// bool 函数不变，保持向后兼容。
// ================================================================

// 设置参数（带错误上下文）
Status MavlinkAdapter::set_param_status(uint8_t target_system, uint8_t target_component,
                                        const char* param_id, float param_value, uint8_t param_type) {
    if (!param_id) return Status::error(ErrorCode::InvalidParam, "null param_id");
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (!impl_->has_send_callback()) { return Status::error(ErrorCode::NotConnected, "no send callback registered"); }
    const uint64_t history_id = impl_->pc.record_write_request(
        param_id, param_value, param_type,
        target_system, target_component);
    mavlink_msg_param_set_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        param_id, param_value, param_type);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O）。
    lk.unlock();
    if (!impl_->send_msg()) {
        // 失败回执要改参数写入状态，必须重新加锁；history_id 保证只终结这一笔请求。
        lk.lock();
        impl_->pc.complete_write(
            history_id,
            false,
            param_value,
            "send_msg failed for PARAM_SET");
        impl_->pc.pending_writes.erase(param_id);
        return Status::error(ErrorCode::SendFailed, "send_msg failed for PARAM_SET");
    }
    return Status::ok();
}

// 发送 COMMAND_LONG（带错误上下文）
Status MavlinkAdapter::send_command_long_status(uint8_t target_system, uint8_t target_component,
                                                uint16_t command, uint8_t confirmation,
                                                float p1, float p2, float p3, float p4,
                                                float p5, float p6, float p7) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (!impl_->has_send_callback()) { return Status::error(ErrorCode::NotConnected, "no send callback registered"); }
    mavlink_msg_command_long_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        command, confirmation,
        p1, p2, p3, p4, p5, p6, p7);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    if (!impl_->send_msg()) {
        char buf[80];
        snprintf(buf, sizeof(buf), "send_msg failed for COMMAND_LONG cmd=%u", command);
        return Status::error(ErrorCode::SendFailed, buf);
    }
    return Status::ok();
}

Status MavlinkAdapter::arm_disarm_status(uint8_t target_system, uint8_t target_component,
                                         bool arm, bool force) {
    float force_param = force ? 21196.0f : 0.0f;
    auto result = send_command_long_status(target_system, target_component,
                                           MAV_CMD_COMPONENT_ARM_DISARM, 0,
                                           arm ? 1.0f : 0.0f,
                                           force_param,
                                           0, 0, 0, 0, 0);
    if (!result) {
        return Status::error(result.code(),
                             arm ? "arm command send failed" : "disarm command send failed");
    }
    return Status::ok();
}

Status MavlinkAdapter::set_mode_status(uint8_t target_system, uint8_t target_component,
                                       uint8_t base_mode, uint32_t custom_mode) {
    auto result = send_command_long_status(target_system, target_component,
                                           MAV_CMD_DO_SET_MODE, 0,
                                           static_cast<float>(resolve_mode_base_flag(base_mode)),
                                           static_cast<float>(custom_mode),
                                           0, 0, 0, 0, 0);
    if (!result) {
        return Status::error(result.code(), "set_mode command send failed");
    }
    return Status::ok();
}

Status MavlinkAdapter::cmd_guided_point_status(uint8_t target_system, uint8_t target_component,
                                               double lat_deg, double lon_deg, float alt_m,
                                               float yaw_deg, float speed_ms) {
    float p1 = speed_ms > 0 ? speed_ms : -1.0f;
    float p2 = 1.0f;  // MAV_DO_REPOSITION_FLAGS_MODE_GUIDED
    float p4 = (yaw_deg >= 0) ? yaw_deg : NAN;
    int32_t lat_e7 = static_cast<int32_t>(lat_deg * 1e7);
    int32_t lon_e7 = static_cast<int32_t>(lon_deg * 1e7);

    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_command_int_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        MAV_FRAME_GLOBAL_RELATIVE_ALT_INT,
        MAV_CMD_DO_REPOSITION,
        0, 1,
        p1, p2, 0, p4,
        lat_e7, lon_e7, alt_m);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    if (!impl_->send_msg()) {
        return Status::error(ErrorCode::SendFailed, "send_msg failed for DO_REPOSITION");
    }
    return Status::ok();
}


// ================================================================
// 任务启动（MAV_CMD_MISSION_START）
// ================================================================
// 开始执行任务
bool MavlinkAdapter::mission_start(uint8_t target_system, uint8_t target_component) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_MISSION_START, 0,
                             0, 0, 0, 0, 0, 0, 0);
}

// ================================================================
// 日志下载（LOG_REQUEST_DATA 协议）
// ================================================================
// 请求日志数据块
bool MavlinkAdapter::log_request_data(uint8_t target_system, uint8_t target_component,
                                       uint32_t id, uint32_t ofs, uint32_t count) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_log_request_data_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        id, ofs, count);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 结束日志下载
bool MavlinkAdapter::log_request_end(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_log_request_end_pack(
        255, 0, &impl_->msg,
        target_system, target_component);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 擦除所有日志
bool MavlinkAdapter::log_erase(uint8_t target_system, uint8_t target_component) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_log_erase_pack(
        255, 0, &impl_->msg,
        target_system, target_component);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// ================================================================
// 相机命令
// ================================================================
// 拍照
bool MavlinkAdapter::camera_take_photo(uint8_t target_system, uint8_t target_component) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_IMAGE_START_CAPTURE, 0,
                             0,  // start_sequence (0 = single image)
                             0,  // interval (0 = single)
                             1,  // total images (1 = single)
                             0, 0, 0, 0);
}

// 开始视频录制
bool MavlinkAdapter::camera_start_video(uint8_t target_system, uint8_t target_component) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_VIDEO_START_STREAMING, 0,
                             0,  // stream_id (0 = default)
                             0, 0, 0, 0, 0, 0);
}

// 停止视频录制
bool MavlinkAdapter::camera_stop_video(uint8_t target_system, uint8_t target_component) {
    return send_command_long(target_system, target_component,
                             MAV_CMD_VIDEO_STOP_STREAMING, 0,
                             0,  // stream_id (0 = default)
                             0, 0, 0, 0, 0, 0);
}

// 设置相机变焦
bool MavlinkAdapter::camera_set_zoom(uint8_t target_system, uint8_t target_component,
                                      uint8_t zoom_type, float zoom_value) {
    // MAV_CMD_DO_DIGICAM_CONTROL: param2 = zoom step, param5 = zoom type
    return send_command_long(target_system, target_component,
                             MAV_CMD_DO_DIGICAM_CONTROL, 0,
                             0,  // session control
                             zoom_value,  // zoom position/step
                             0, 0, zoom_type, 0, 0);
}

// 请求相机信息
bool MavlinkAdapter::camera_request_info(uint8_t target_sys, uint8_t target_comp) {
    return send_command_long(target_sys, target_comp,
                             MAV_CMD_REQUEST_CAMERA_INFORMATION, 0,
                             1, 0, 0, 0, 0, 0, 0);
}

// 设置相机模式（0=拍照, 1=录像）
bool MavlinkAdapter::camera_set_mode(uint8_t target_sys, uint8_t target_comp,
                                      uint8_t camera_mode) {
    return send_command_long(target_sys, target_comp,
                             MAV_CMD_SET_CAMERA_MODE, 0,
                             0, static_cast<float>(camera_mode), 0, 0, 0, 0, 0);
}

// 停止拍照
bool MavlinkAdapter::camera_stop_capture(uint8_t target_sys, uint8_t target_comp) {
    return send_command_long(target_sys, target_comp,
                             MAV_CMD_IMAGE_STOP_CAPTURE, 0,
                             0, 0, 0, 0, 0, 0, 0);
}

// 设置相机对焦
bool MavlinkAdapter::camera_set_focus(uint8_t target_sys, uint8_t target_comp,
                                       uint8_t focus_type, float focus_value) {
    return send_command_long(target_sys, target_comp,
                             MAV_CMD_SET_CAMERA_FOCUS, 0,
                             static_cast<float>(focus_type), focus_value, 0, 0, 0, 0, 0);
}

// 云台归中
bool MavlinkAdapter::cmd_gimbal_recenter(uint8_t target_sys, uint8_t target_comp) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW, 0,
        0, 0, 0, 0, 0, 0, 0);
}

// 请求云台管理器信息
bool MavlinkAdapter::gimbal_request_info(uint8_t target_sys, uint8_t target_comp) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_REQUEST_MESSAGE, 0,
        541.0f,  // MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION
        0, 0, 0, 0, 0, 0);
}

// 云台配置（设置主控/副控）
bool MavlinkAdapter::cmd_gimbal_configure(uint8_t target_sys, uint8_t target_comp,
                                           uint8_t gimbal_device_id,
                                           uint8_t primary_sys, uint8_t primary_comp,
                                           uint8_t secondary_sys, uint8_t secondary_comp) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_DO_GIMBAL_MANAGER_CONFIGURE, 0,
        static_cast<float>(primary_sys),
        static_cast<float>(primary_comp),
        static_cast<float>(secondary_sys),
        static_cast<float>(secondary_comp),
        static_cast<float>(gimbal_device_id),
        0, 0);
}

// 请求视频流信息（MAV_CMD_REQUEST_MESSAGE for VIDEO_STREAM_INFORMATION）
bool MavlinkAdapter::video_request_stream_info(uint8_t target_sys, uint8_t target_comp) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_REQUEST_MESSAGE, 0,
        269.0f,  // MAVLINK_MSG_ID_VIDEO_STREAM_INFORMATION
        0, 0, 0, 0, 0, 0);
}

// ================================================================
// Firmware Flash (STM32 bootloader)
// Real implementation needs serial bootloader protocol before this path can
// expose production flashing behavior.
// ================================================================


// ================================================================
// 日志下载（LOG_REQUEST_DATA + LOG_DATA 处理）
// ================================================================
// 开始日志下载
bool MavlinkAdapter::log_download_start(uint8_t target_system, uint8_t target_component,
                                         uint16_t log_id, const std::string& filepath) {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    
    // 查找日志条目以获取大小
    uint32_t log_size = 0;
    for (const auto& e : impl_->log_state.entries) {
        if (e.id == log_id) {
            log_size = e.size_bytes;
            break;
        }
    }
    
    impl_->log_state.downloading = true;
    impl_->log_state.download_id = log_id;
    impl_->log_state.download_ofs = 0;
    impl_->log_state.download_size = log_size;
    impl_->log_state.download_size_declared = log_size > 0;
    impl_->log_state.download_data.clear();
    impl_->log_state.download_data.reserve(log_size);
    impl_->log_state.download_received.assign(log_size, 0);
    impl_->log_state.download_received_bytes = 0;
    impl_->log_state.download_last_data_at = std::chrono::steady_clock::now();
    impl_->log_state.download_filepath = filepath;
    impl_->log_state.download_target_sys = target_system;
    impl_->log_state.download_target_comp = target_component;
    impl_->log_state.download_write_pending = false;
    impl_->log_state.download_saved_bytes = 0;
    impl_->log_state.download_completed = false;
    impl_->log_state.download_cancelled = false;
    impl_->log_state.download_error.clear();
    
    // 请求第一个数据块
    mavlink_msg_log_request_data_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        log_id, 0, 90);  // 90 bytes per LOG_DATA message
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O）。
    lk.unlock();
    const bool sent = impl_->send_msg();
    if (!sent) {
        lk.lock();  // 回滚下载状态需要重新加锁
        impl_->log_state.downloading = false;
        impl_->log_state.download_error = "failed to request log data";
    }
    return sent;
}

// 取消日志下载
bool MavlinkAdapter::log_download_cancel() {
    mg::core::platform::UniqueLock lk(impl_->mtx);
    if (!impl_->log_state.downloading) return false;
    
    impl_->log_state.downloading = false;
    impl_->log_state.download_cancelled = true;
    impl_->log_state.download_data.clear();
    
    // 发送 LOG_REQUEST_END 停止下载
    mavlink_msg_log_request_end_pack(
        255, 0, &impl_->msg,
        impl_->log_state.download_target_sys, impl_->log_state.download_target_comp);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 获取日志下载进度
float MavlinkAdapter::log_download_progress() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    if (!impl_->log_state.downloading || impl_->log_state.download_size == 0) return 0.0f;
    return static_cast<float>(impl_->log_state.download_ofs) / impl_->log_state.download_size;
}

// 检查日志下载是否活跃
bool MavlinkAdapter::log_download_active() const {
    mg::core::platform::LockGuard lk(impl_->mtx);
    return impl_->log_state.downloading;
}

MavlinkAdapter::LogDownloadStatus MavlinkAdapter::log_download_status() {
    // P4：状态查询是纯读取，不再补发 LOG_REQUEST_DATA。
    // 丢包补发由 Impl::process_timeouts() 驱动（见 mavlink_adapter_dispatch.cc）。
    mg::core::platform::LockGuard lk(impl_->mtx);
    LogDownloadStatus status;
    status.active = impl_->log_state.downloading;
    status.saving = impl_->log_state.download_write_pending;
    status.completed = impl_->log_state.download_completed;
    status.cancelled = impl_->log_state.download_cancelled;
    status.id = impl_->log_state.download_id;
    status.received_bytes = impl_->log_state.download_received_bytes;
    status.total_bytes = impl_->log_state.download_size;
    status.size_declared = impl_->log_state.download_size_declared;
    status.saved_bytes = impl_->log_state.download_saved_bytes;
    status.filepath = impl_->log_state.download_filepath;
    status.error = impl_->log_state.download_error;
    return status;
}
// 绕圈命令：照 QGC `Vehicle::guidedModeOrbit` 的 COMMAND_INT 形式。
//
//   frame  = MAV_FRAME_GLOBAL（z 是 AMSL，和 goto 一致）
//   param1 = 半径，带符号表示环绕方向
//   param2 = NaN  用飞控默认速度
//   param3 = ORBIT_YAW_BEHAVIOUR_UNCHANGED(5) 保持原有偏航行为
//   param4 = NaN  用飞控默认圈数
//
// 用 COMMAND_LONG 表达不了 NaN，也只能把参数塞进 param5/6，所以必须走 COMMAND_INT。
bool MavlinkAdapter::cmd_orbit(uint8_t target_sys, uint8_t target_comp,
                                double lat, double lon, float radius_m,
                                float velocity_ms, float altitude_m) {
    const int32_t lat_e7 = static_cast<int32_t>(lat * 1e7);
    const int32_t lon_e7 = static_cast<int32_t>(lon * 1e7);
    mg::core::platform::UniqueLock lk(impl_->mtx);
    mavlink_msg_command_int_pack(
        255, 0, &impl_->msg,
        target_sys, target_comp,
        MAV_FRAME_GLOBAL,
        MAV_CMD_DO_ORBIT,
        0, 1,
        radius_m,
        velocity_ms > 0 ? velocity_ms : NAN,
        static_cast<float>(ORBIT_YAW_BEHAVIOUR_UNCHANGED),
        NAN,
        lat_e7, lon_e7, altitude_m);
    // KD-22：send_cb 在锁外执行（A3：持锁路径不做阻塞 I/O，
    // 也避免传输层回调重入 adapter 时死锁）。
    lk.unlock();
    return impl_->send_msg();
}

// 设置感兴趣区域
bool MavlinkAdapter::cmd_set_roi(uint8_t target_sys, uint8_t target_comp,
                                  double lat, double lon, float alt) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_DO_SET_ROI_LOCATION, 0,
        0, 0, 0, 0,
        static_cast<float>(lat), static_cast<float>(lon), alt);
}

// 设置航向：QGC `APMFirmwarePlugin::guidedModeChangeHeading` 用 CONDITION_YAW，
// param1=相对/绝对转角、param2=最大偏航速率、param3=方向(±1)、param4=是否相对当前航向。
bool MavlinkAdapter::cmd_change_heading(uint8_t target_sys, uint8_t target_comp,
                                         float heading_deg, float yaw_rate_dps,
                                         int8_t direction, bool relative) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_CONDITION_YAW, 0,
        heading_deg, yaw_rate_dps, static_cast<float>(direction), relative ? 1.0f : 0.0f,
        0, 0, 0);
}

// 取消感兴趣区域
bool MavlinkAdapter::cmd_roi_stop(uint8_t target_sys, uint8_t target_comp) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_DO_SET_ROI, 0,
        0, 0, 0, 0, 0, 0, 0);
}

// 收放起落架
bool MavlinkAdapter::cmd_landing_gear(uint8_t target_sys, uint8_t target_comp, bool deploy) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_AIRFRAME_CONFIGURATION, 0,
        deploy ? 1.0f : 0.0f, 0, 0, 0, 0, 0, 0);
}

// 控制云台
bool MavlinkAdapter::cmd_gimbal_control(uint8_t target_sys, uint8_t target_comp,
                                         float pitch_deg, float yaw_deg) {
    return send_command_long(target_sys, target_comp,
        MAV_CMD_DO_GIMBAL_MANAGER_PITCHYAW, 0,
        pitch_deg, yaw_deg, 0, 0, 0, 0, 0);
}

// ================================================================
// 命令队列 API — ACK 超时重发
// ================================================================

CommandTicket MavlinkAdapter::send_command_queued(
    uint8_t target_system, uint8_t target_component,
    uint16_t command,
    float p1, float p2, float p3, float p4,
    float p5, float p6, float p7,
    std::function<void(const CommandAckInfo&)> result_cb) {
    mg::core::platform::UniqueLock lk(impl_->mtx);

    // 分类命令策略
    CommandRetryPolicy policy;
    int max_tries, timeout_ms;
    classify_command(command, policy, max_tries, timeout_ms);

    const auto now = std::chrono::steady_clock::now();
    if (impl_->cmd_queue.has_recent_timeout(target_system, target_component, command, now)) {
        CommandQueueEntry blocked{};
        blocked.id = impl_->cmd_queue.allocate_id();
        blocked.command = command;
        blocked.target_sys = target_system;
        blocked.target_comp = target_component;
        blocked.state = CommandQueueEntry::State::TimedOut;
        blocked.timed_out = true;
        blocked.timed_out_at = now;
        blocked.outcome = CommandOutcome::Timeout;
        blocked.ack_result = 4;  // MAV_RESULT_FAILED
        blocked.result_text = "previous command timed out; late ACK quarantine active";
        blocked.result_cb = std::move(result_cb);
        const std::string text = blocked.result_text;
        auto cb = blocked.result_cb;
        CommandTicket ticket{blocked.id, command};
        impl_->cmd_queue.entries.emplace_back(std::move(blocked));
        impl_->cmd_queue.cv.notify_all();
        if (cb) {
            CommandAckInfo info{};
            info.command = command;
            info.result = 4;
            info.result_text = text;
            info.outcome = CommandOutcome::Timeout;
            lk.unlock();
            cb(info);
        }
        return ticket;
    }

    // 创建队列条目
    CommandQueueEntry entry{};
    entry.id = impl_->cmd_queue.allocate_id();
    entry.command = command;
    entry.target_sys = target_system;
    entry.target_comp = target_component;
    if (command == MAV_CMD_DO_SET_MODE && p1 == 0.0f) {
        p1 = static_cast<float>(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED);
    }

    entry.params[0] = p1; entry.params[1] = p2; entry.params[2] = p3;
    entry.params[3] = p4; entry.params[4] = p5; entry.params[5] = p6;
    entry.params[6] = p7;
    entry.max_tries = max_tries;
    entry.try_count = 1;  // 第一次发送
    entry.ack_timeout_ms = timeout_ms;
    entry.send_time = std::chrono::steady_clock::now();
    entry.policy = policy;
    entry.result_cb = std::move(result_cb);

    CommandTicket ticket{entry.id, command};
    impl_->cmd_queue.entries.emplace_back(std::move(entry));

    // Register the queue entry before sending. In-memory links and fast serial
    // peers can deliver COMMAND_ACK synchronously before this function returns.
    mavlink_msg_command_long_pack(
        255, 0, &impl_->msg,
        target_system, target_component,
        command, 0,
        p1, p2, p3, p4, p5, p6, p7);
    // KD-22：发送在锁外执行（A3/A4），失败后回锁把这一笔标成超时。
    const bool sent = impl_->send_msg_unlocked(lk);
    if (!sent) {
        for (auto& queued : impl_->cmd_queue.entries) {
            if (queued.id != ticket.id) continue;
            queued.state = CommandQueueEntry::State::TimedOut;
            queued.outcome = CommandOutcome::SendFailed;
            queued.ack_result = 4;  // MAV_RESULT_FAILED
            queued.result_text = "command send failed";
            break;
        }
        impl_->cmd_queue.cv.notify_all();
        std::function<void(const CommandAckInfo&)> cb;
        for (auto& queued : impl_->cmd_queue.entries) {
            if (queued.id != ticket.id) continue;
            cb = std::move(queued.result_cb);
            break;
        }
        if (cb) {
            CommandAckInfo info{};
            info.command = command;
            info.result = 4;
            info.result_text = "command send failed";
            info.outcome = CommandOutcome::SendFailed;
            lk.unlock();
            cb(info);
        }
    }
    return ticket;
}

CommandTicket MavlinkAdapter::send_command_int_queued(
    uint8_t target_system, uint8_t target_component,
    uint16_t command, uint8_t frame,
    float p1, float p2, float p3, float p4,
    int32_t x, int32_t y, float z,
    std::function<void(const CommandAckInfo&)> result_cb) {
    mg::core::platform::UniqueLock lk(impl_->mtx);

    CommandRetryPolicy policy;
    int max_tries, timeout_ms;
    classify_command(command, policy, max_tries, timeout_ms);

    const auto now = std::chrono::steady_clock::now();
    if (impl_->cmd_queue.has_recent_timeout(target_system, target_component, command, now)) {
        CommandQueueEntry blocked{};
        blocked.id = impl_->cmd_queue.allocate_id();
        blocked.command = command;
        blocked.target_sys = target_system;
        blocked.target_comp = target_component;
        blocked.use_command_int = true;
        blocked.frame = frame;
        blocked.state = CommandQueueEntry::State::TimedOut;
        blocked.timed_out = true;
        blocked.timed_out_at = now;
        blocked.outcome = CommandOutcome::Timeout;
        blocked.ack_result = 4;  // MAV_RESULT_FAILED
        blocked.result_text = "previous command timed out; late ACK quarantine active";
        blocked.result_cb = std::move(result_cb);
        const std::string text = blocked.result_text;
        auto cb = blocked.result_cb;
        CommandTicket ticket{blocked.id, command};
        impl_->cmd_queue.entries.emplace_back(std::move(blocked));
        impl_->cmd_queue.cv.notify_all();
        if (cb) {
            CommandAckInfo info{};
            info.command = command;
            info.result = 4;
            info.result_text = text;
            info.outcome = CommandOutcome::Timeout;
            lk.unlock();
            cb(info);
        }
        return ticket;
    }

    CommandQueueEntry entry{};
    entry.id = impl_->cmd_queue.allocate_id();
    entry.command = command;
    entry.target_sys = target_system;
    entry.target_comp = target_component;
    entry.use_command_int = true;
    entry.frame = frame;
    entry.params[0] = p1;
    entry.params[1] = p2;
    entry.params[2] = p3;
    entry.params[3] = p4;
    entry.x = x;
    entry.y = y;
    entry.z = z;
    entry.max_tries = max_tries;
    entry.try_count = 1;
    entry.ack_timeout_ms = timeout_ms;
    entry.send_time = std::chrono::steady_clock::now();
    entry.policy = policy;
    entry.result_cb = std::move(result_cb);

    CommandTicket ticket{entry.id, command};
    impl_->cmd_queue.entries.emplace_back(std::move(entry));

    // KD-22：发送在锁外执行（A3/A4），失败后回锁把这一笔标成超时。
    for (const auto& queued : impl_->cmd_queue.entries) {
        if (queued.id == ticket.id) {
            impl_->pack_command_queue_entry(queued);
            break;
        }
    }
    const bool sent = impl_->send_msg_unlocked(lk);
    if (!sent) {
        for (auto& queued : impl_->cmd_queue.entries) {
            if (queued.id != ticket.id) continue;
            queued.state = CommandQueueEntry::State::TimedOut;
            queued.outcome = CommandOutcome::SendFailed;
            queued.ack_result = 4;  // MAV_RESULT_FAILED
            queued.result_text = "command send failed";
            break;
        }
        impl_->cmd_queue.cv.notify_all();
        std::function<void(const CommandAckInfo&)> cb;
        for (auto& queued : impl_->cmd_queue.entries) {
            if (queued.id != ticket.id) continue;
            cb = std::move(queued.result_cb);
            break;
        }
        if (cb) {
            CommandAckInfo info{};
            info.command = command;
            info.result = 4;
            info.result_text = "command send failed";
            info.outcome = CommandOutcome::SendFailed;
            lk.unlock();
            cb(info);
        }
    }
    return ticket;
}

CommandTicket MavlinkAdapter::arm_disarm_queued(
    uint8_t target_system, uint8_t target_component,
    bool arm, bool force,
    std::function<void(const CommandAckInfo&)> result_cb) {
    float force_param = force ? 21196.0f : 0.0f;
    return send_command_queued(target_system, target_component,
                               MAV_CMD_COMPONENT_ARM_DISARM,
                               arm ? 1.0f : 0.0f, force_param,
                               0, 0, 0, 0, 0,
                               std::move(result_cb));
}

CommandTicket MavlinkAdapter::set_mode_queued(
    uint8_t target_system, uint8_t target_component,
    uint8_t base_mode, uint32_t custom_mode,
    std::function<void(const CommandAckInfo&)> result_cb) {
    return send_command_queued(target_system, target_component,
                               MAV_CMD_DO_SET_MODE,
                               static_cast<float>(base_mode),
                               static_cast<float>(custom_mode),
                               0, 0, 0, 0, 0,
                               std::move(result_cb));
}

} // namespace mg::core::mavlink
