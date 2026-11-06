/**
 * mavlink_adapter_mission.cc - MAVLink mission protocol handlers
 *
 * Mission transfer handlers and their send helpers are defined out of line.
 * MavlinkAdapter::Impl::process() keeps dispatch, locking, timeout checks,
 * and callback execution unchanged.
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

namespace {

/// MISSION_CURRENT / MISSION_ITEM_REACHED 不是任务传输报文，而是飞控主动上报的
/// 任务执行进度。它们同样必须确认来源是本机，否则一链路多机时别的飞机能把本机
/// 的进度改写掉：
///   1. 本机已经和某台飞控建立过任务事务 → 用事务目标（与其余 5 个 handler 同一
///      套 transfer_matches_target 规则）；
///   2. 还没有任何事务（target_sys == 0，例如刚连上就观察在飞的任务）→ 回退到
///      本机已识别的心跳身份；
///   3. 两者都不知道 → 不接受，与 handle_mission_ack 的 target_sys == 0 同一语义。
bool execution_source_matches(const MissionTransferState& state,
                              const std::optional<HeartbeatInfo>& heartbeat,
                              uint8_t sysid, uint8_t compid) {
    if (state.target_sys != 0) {
        return transfer_matches_target(state, sysid, compid);
    }
    if (heartbeat.has_value() && heartbeat->system_id != 0) {
        return heartbeat->system_id == sysid &&
               heartbeat->component_id == compid;
    }
    return false;
}

} // namespace

MissionTransferState& MavlinkAdapter::Impl::select_transfer(uint8_t mission_type)
{
    return mg::core::mavlink::select_transfer(mission, rally, fence, mission_type);
}

MissionTransferState* MavlinkAdapter::Impl::find_uploading()
{
    return mg::core::mavlink::find_uploading(mission, rally, fence);
}

void MavlinkAdapter::Impl::handle_mission_count(const mavlink_message_t& m)
{
    mavlink_mission_count_t mc;
    mavlink_msg_mission_count_decode(&m, &mc);

    if (!is_valid_mission_type(mc.mission_type)) return;
    auto& state = select_transfer(mc.mission_type);
    if (state.progress.state != mg::core::mission::MissionState::Downloading ||
        state.download_total != 0 ||
        !transfer_matches_target(state, m.sysid, m.compid)) {
        return;
    }

    state.record_recv();
    state.download_total = mc.count;
    state.download_seq = 0;
    state.plan.items.clear();
    ++state.plan_revision;
    state.plan.items.reserve(mc.count);
    state.progress = mg::core::mission::MissionTransferProgress{};
    state.progress.state = mg::core::mission::MissionState::Downloading;
    state.progress.total = mc.count;
    state.progress.received = 0;

    if (mc.count == 0) {
        state.progress.state = mg::core::mission::MissionState::Idle;
        state.plan.valid = true;
        state.last_completed_download_revision = state.plan_revision;
        ++state.completed_download_count;
    } else {
        send_mission_request_int(m.sysid, m.compid, 0, mc.mission_type);
        state.record_send(
            m.sysid, m.compid,
            [this, &state, sys = m.sysid, comp = m.compid,
             mission_type = mc.mission_type]() {
                send_mission_request_int(sys, comp, state.download_seq,
                                         mission_type);
            });
    }
}

void MavlinkAdapter::Impl::handle_mission_item_int(const mavlink_message_t& m)
{
    mavlink_mission_item_int_t mi;
    mavlink_msg_mission_item_int_decode(&m, &mi);

    if (!is_valid_mission_type(mi.mission_type)) return;
    auto& state = select_transfer(mi.mission_type);
    if (state.progress.state != mg::core::mission::MissionState::Downloading ||
        !transfer_matches_target(state, m.sysid, m.compid)) {
        return;
    }

    auto request_expected_item = [this, &state, &mi, &m]() {
        send_mission_request_int(m.sysid, m.compid, state.download_seq,
                                 mi.mission_type);
        state.record_recv();
        state.record_send(
            m.sysid, m.compid,
            [this, &state, sys = m.sysid, comp = m.compid,
             mission_type = mi.mission_type]() {
                send_mission_request_int(sys, comp, state.download_seq,
                                         mission_type);
            });
    };

    if (state.download_total == 0 || mi.seq != state.download_seq) {
        request_expected_item();
        return;
    }

    mg::core::mission::MissionItem item{};
    item.seq = mi.seq;
    item.frame = mi.frame;
    item.command = mi.command;
    item.current = mi.current;
    item.autocontinue = mi.autocontinue;
    item.param1 = mi.param1;
    item.param2 = mi.param2;
    item.param3 = mi.param3;
    item.param4 = mi.param4;
    item.x = mi.x;
    item.y = mi.y;
    item.z = mi.z;
    item.mission_type = mi.mission_type;

    if (mi.seq >= state.plan.items.size()) state.plan.items.resize(mi.seq + 1);
    state.plan.items[mi.seq] = item;
    ++state.plan_revision;
    state.download_seq = static_cast<uint16_t>(mi.seq + 1);
    state.progress.received = state.download_seq;
    state.record_recv();

    if (state.download_seq < state.download_total) {
        send_mission_request_int(m.sysid, m.compid, state.download_seq,
                                 mi.mission_type);
        state.record_send(
            m.sysid, m.compid,
            [this, &state, sys = m.sysid, comp = m.compid,
             mission_type = mi.mission_type]() {
                send_mission_request_int(sys, comp, state.download_seq,
                                         mission_type);
            });
    } else {
        state.plan.valid = true;
        state.last_completed_download_revision = state.plan_revision;
        ++state.completed_download_count;
        send_mission_ack(m.sysid, m.compid, MAV_MISSION_ACCEPTED,
                         mi.mission_type);
        state.progress.state = mg::core::mission::MissionState::Idle;
        state.resend_fn = nullptr;
    }
}

void MavlinkAdapter::Impl::handle_mission_item(const mavlink_message_t& m)
{
    mavlink_mission_item_t mi;
    mavlink_msg_mission_item_decode(&m, &mi);

    if (!is_valid_mission_type(mi.mission_type)) return;
    auto& state = select_transfer(mi.mission_type);
    if (state.progress.state != mg::core::mission::MissionState::Downloading ||
        !transfer_matches_target(state, m.sysid, m.compid)) {
        return;
    }

    auto request_expected_item = [this, &state, &mi, &m]() {
        send_mission_request_int(m.sysid, m.compid, state.download_seq,
                                 mi.mission_type);
        state.record_recv();
        state.record_send(
            m.sysid, m.compid,
            [this, &state, sys = m.sysid, comp = m.compid,
             mission_type = mi.mission_type]() {
                send_mission_request_int(sys, comp, state.download_seq,
                                         mission_type);
            });
    };

    if (state.download_total == 0 || mi.seq != state.download_seq) {
        request_expected_item();
        return;
    }

    mg::core::mission::MissionItem item{};
    item.seq = mi.seq;
    item.frame = mi.frame;
    item.command = mi.command;
    item.current = mi.current;
    item.autocontinue = mi.autocontinue;
    item.param1 = mi.param1;
    item.param2 = mi.param2;
    item.param3 = mi.param3;
    item.param4 = mi.param4;
    if (mi.frame == MAV_FRAME_GLOBAL || mi.frame == MAV_FRAME_GLOBAL_RELATIVE_ALT ||
        mi.frame == MAV_FRAME_GLOBAL_TERRAIN_ALT) {
        item.x = static_cast<int32_t>(mi.x * 1e7f);
        item.y = static_cast<int32_t>(mi.y * 1e7f);
    } else {
        item.x = static_cast<int32_t>(mi.x);
        item.y = static_cast<int32_t>(mi.y);
    }
    item.z = mi.z;
    item.mission_type = mi.mission_type;

    if (mi.seq >= state.plan.items.size()) state.plan.items.resize(mi.seq + 1);
    state.plan.items[mi.seq] = item;
    ++state.plan_revision;
    state.download_seq = static_cast<uint16_t>(mi.seq + 1);
    state.progress.received = state.download_seq;
    state.record_recv();

    if (state.download_seq < state.download_total) {
        send_mission_request_int(m.sysid, m.compid, state.download_seq,
                                 mi.mission_type);
        state.record_send(
            m.sysid, m.compid,
            [this, &state, sys = m.sysid, comp = m.compid,
             mission_type = mi.mission_type]() {
                send_mission_request_int(sys, comp, state.download_seq,
                                         mission_type);
            });
    } else {
        state.plan.valid = true;
        state.last_completed_download_revision = state.plan_revision;
        ++state.completed_download_count;
        send_mission_ack(m.sysid, m.compid, MAV_MISSION_ACCEPTED,
                         mi.mission_type);
        state.progress.state = mg::core::mission::MissionState::Idle;
        state.resend_fn = nullptr;
    }
}

void MavlinkAdapter::Impl::mark_mission_item_sent(MissionTransferState& state, uint16_t seq)
{
    state.upload_seq = seq + 1;
    state.progress.sent = state.upload_seq;
}

void MavlinkAdapter::Impl::handle_mission_request_int(const mavlink_message_t& m)
{
    mavlink_mission_request_int_t mr;
    mavlink_msg_mission_request_int_decode(&m, &mr);

    auto* state = mg::core::mavlink::find_uploading(
        mission, rally, fence, mr.mission_type);
    if (!state ||
        !transfer_matches_target(*state, m.sysid, m.compid) ||
        mr.seq >= state->upload_total ||
        mr.seq >= state->plan.items.size()) {
        return;
    }

    state->record_recv();
    auto item = state->plan.items[mr.seq];
    send_mission_item_int(m.sysid, m.compid, item);
    mark_mission_item_sent(*state, mr.seq);
    state->record_send(m.sysid, m.compid,
                       [this, sys = m.sysid, comp = m.compid, item]() { send_mission_item_int(sys, comp, item); });
}

void MavlinkAdapter::Impl::handle_mission_request(const mavlink_message_t& m)
{
    mavlink_mission_request_t mr;
    mavlink_msg_mission_request_decode(&m, &mr);

    auto* state = mg::core::mavlink::find_uploading(
        mission, rally, fence, mr.mission_type);
    if (!state ||
        !transfer_matches_target(*state, m.sysid, m.compid) ||
        mr.seq >= state->upload_total ||
        mr.seq >= state->plan.items.size()) {
        return;
    }

    state->record_recv();
    auto item = state->plan.items[mr.seq];
    send_mission_item(m.sysid, m.compid, item);
    mark_mission_item_sent(*state, mr.seq);
    state->record_send(m.sysid, m.compid,
                       [this, sys = m.sysid, comp = m.compid, item]() { send_mission_item(sys, comp, item); });
}

void MavlinkAdapter::Impl::handle_mission_ack(const mavlink_message_t& m)
{
    mavlink_mission_ack_t ack;
    mavlink_msg_mission_ack_decode(&m, &ack);

    // 只接受本机发起、且来自该事务目标飞控/组件的 ACK。
    // 多机共用一条链路时，别的飞机（或别的组件）的同类型 ACK 不能结束本机事务；
    // target 仍为 0 说明本机没有发起过带目标的传输，同样不接受。
    // 事务空闲时收到 ACK 一律忽略（QGC 的做法：State machine is idle. Vehicle is
    // confused.），否则一条游离 ACK 会把正在进行的下载打回空闲。
    if (!mg::core::mavlink::is_valid_mission_type(ack.mission_type)) return;
    auto& state = select_transfer(ack.mission_type);
    const bool expecting_ack = state.progress.state == mg::core::mission::MissionState::Uploading ||
                               state.progress.state == mg::core::mission::MissionState::WaitingAck ||
                               state.upload_backup.armed;  // 清除任务没有 Uploading 状态
    if (!expecting_ack) return;
    if (state.target_sys == 0 ||
        !mg::core::mavlink::transfer_matches_target(state, m.sysid, m.compid)) {
        return;
    }

    state.record_recv();
    state.resend_fn = nullptr;
    state.progress.result = ack.type;
    state.progress.state = mg::core::mission::MissionState::Idle;

    if (ack.type == MAV_MISSION_ACCEPTED) {
        if (state.progress.received > 0 || state.progress.sent >= state.progress.total) {
            state.plan.valid = true;
        }
        state.commit_upload();
    } else {
        const char* label = (ack.mission_type == MAV_MISSION_TYPE_RALLY)   ? "Rally"
                            : (ack.mission_type == MAV_MISSION_TYPE_FENCE) ? "Fence"
                                                                           : "Task";
        state.progress.error = std::string(label) + " rejected: " + mg::core::mission::mission_result_message(ack.type);
        // 飞控拒绝：把"飞控里的任务"退回上传前的状态。上传一开始就覆盖了 plan，
        // 不回滚会让界面显示一份飞控从未接受的任务。
        state.rollback_upload();
    }
}

void MavlinkAdapter::Impl::handle_mission_current(const mavlink_message_t& m)
{
    mavlink_mission_current_t mc;
    mavlink_msg_mission_current_decode(&m, &mc);
    // 非本机来源的进度帧一律忽略（多机共用一条链路时别的飞机会改写本机进度）。
    if (!execution_source_matches(mission, tel.heartbeat, m.sysid, m.compid)) return;
    if (mission.restart_pending) {
        mission.restart_pending = false;
        const uint16_t first = tel.heartbeat &&
            tel.heartbeat->autopilot_type == MAV_AUTOPILOT_ARDUPILOTMEGA ? 1 : 0;
        if (mc.seq <= first) mission.clear_execution_progress();
    }
    // 返回较早目标后，旧的末项到达通知不再代表当前航段已经结束。
    if (static_cast<size_t>(mc.seq) + 1 < mission.plan.items.size()) {
        mission.route_reached_end = false;
    }
    mission.plan.current_index = mc.seq;
}

void MavlinkAdapter::Impl::handle_mission_item_reached(const mavlink_message_t& m)
{
    mavlink_mission_item_reached_t mr;
    mavlink_msg_mission_item_reached_decode(&m, &mr);
    // 同上：到达事件也只认本机。
    if (!execution_source_matches(mission, tel.heartbeat, m.sysid, m.compid)) return;
    // 记录任务执行进度：同一个 seq 可能被重发，用 flags 去重避免计数偏大。
    auto& execution = mission;
    if (mr.seq >= execution.reached_flags.size()) {
        execution.reached_flags.resize(mr.seq + 1, 0);
    }
    if (execution.reached_flags[mr.seq] == 0) {
        execution.reached_flags[mr.seq] = 1;
        ++execution.reached_count;
    }
    execution.last_reached_seq = mr.seq;
    execution.has_reached = true;
    if (static_cast<size_t>(mr.seq) + 1 == execution.plan.items.size()) {
        execution.route_reached_end = true;
    }
    if (tel.mission_reached_cb)
        deferred_cbs.push_back([cb = tel.mission_reached_cb, seq = mr.seq]() { cb(seq); });
}

bool MavlinkAdapter::Impl::send_mission_request_int(uint8_t sysid, uint8_t compid, uint16_t seq,
                                                    uint8_t mission_type)
{
    mavlink_msg_mission_request_int_pack(255, 0, &msg, sysid, compid, seq, mission_type);
    return queue_msg();
}

bool MavlinkAdapter::Impl::send_mission_ack(uint8_t sysid, uint8_t compid, uint8_t type,
                                            uint8_t mission_type)
{
    mavlink_msg_mission_ack_pack(255, 0, &msg, sysid, compid, type, mission_type, 0);
    return queue_msg();
}

bool MavlinkAdapter::Impl::send_mission_item_int(uint8_t sysid, uint8_t compid,
                                                 const mg::core::mission::MissionItem& item)
{
    mavlink_msg_mission_item_int_pack(255, 0, &msg, sysid, compid, item.seq, item.frame, item.command, item.current,
                                      item.autocontinue, item.param1, item.param2, item.param3, item.param4, item.x,
                                      item.y, item.z, item.mission_type);
    return queue_msg();
}

bool MavlinkAdapter::Impl::send_mission_item(uint8_t sysid, uint8_t compid, const mg::core::mission::MissionItem& item)
{
    float x = static_cast<float>(item.x);
    float y = static_cast<float>(item.y);
    if (item.frame == MAV_FRAME_GLOBAL || item.frame == MAV_FRAME_GLOBAL_RELATIVE_ALT ||
        item.frame == MAV_FRAME_GLOBAL_TERRAIN_ALT) {
        x = static_cast<float>(static_cast<double>(item.x) / 1e7);
        y = static_cast<float>(static_cast<double>(item.y) / 1e7);
    }
    mavlink_msg_mission_item_pack(255, 0, &msg, sysid, compid, item.seq, item.frame, item.command, item.current,
                                  item.autocontinue, item.param1, item.param2, item.param3, item.param4, x, y, item.z,
                                  item.mission_type);
    return queue_msg();
}

} // namespace mg::core::mavlink
