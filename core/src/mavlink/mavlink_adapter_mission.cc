/**
 * mavlink_adapter_mission.cc - MAVLink mission protocol handlers
 *
 * Mission transfer handlers and their send helpers are defined out of line.
 * MavlinkAdapter::Impl::process() keeps dispatch, locking, timeout checks,
 * and callback execution unchanged.
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

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

    auto& state = select_transfer(mc.mission_type);
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
        send_mission_request_int(m.sysid, m.compid, 0);
        state.record_send(m.sysid, m.compid, [this, &state, sys = m.sysid, comp = m.compid]() {
            send_mission_request_int(sys, comp, state.download_seq);
        });
    }
}

void MavlinkAdapter::Impl::handle_mission_item_int(const mavlink_message_t& m)
{
    mavlink_mission_item_int_t mi;
    mavlink_msg_mission_item_int_decode(&m, &mi);

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

    auto& state = select_transfer(mi.mission_type);
    if (mi.seq >= state.plan.items.size()) {
        state.plan.items.resize(mi.seq + 1);
    }
    state.plan.items[mi.seq] = item;
    ++state.plan_revision;
    state.download_seq = mi.seq + 1;
    state.progress.received = state.download_seq;
    state.record_recv();

    if (state.download_seq < state.download_total) {
        send_mission_request_int(m.sysid, m.compid, state.download_seq);
        state.record_send(m.sysid, m.compid, [this, &state, sys = m.sysid, comp = m.compid]() {
            send_mission_request_int(sys, comp, state.download_seq);
        });
    } else {
        state.plan.valid = true;
        state.last_completed_download_revision = state.plan_revision;
        ++state.completed_download_count;
        send_mission_ack(m.sysid, m.compid, MAV_MISSION_ACCEPTED);
        state.progress.state = mg::core::mission::MissionState::Idle;
        state.resend_fn = nullptr;
    }
}

void MavlinkAdapter::Impl::handle_mission_item(const mavlink_message_t& m)
{
    mavlink_mission_item_t mi;
    mavlink_msg_mission_item_decode(&m, &mi);

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

    auto& state = select_transfer(mi.mission_type);
    if (mi.seq >= state.plan.items.size()) {
        state.plan.items.resize(mi.seq + 1);
    }
    state.plan.items[mi.seq] = item;
    ++state.plan_revision;
    state.download_seq = mi.seq + 1;
    state.progress.received = state.download_seq;
    state.record_recv();

    if (state.download_seq < state.download_total) {
        send_mission_request_int(m.sysid, m.compid, state.download_seq);
        state.record_send(m.sysid, m.compid, [this, &state, sys = m.sysid, comp = m.compid]() {
            send_mission_request_int(sys, comp, state.download_seq);
        });
    } else {
        state.plan.valid = true;
        state.last_completed_download_revision = state.plan_revision;
        ++state.completed_download_count;
        send_mission_ack(m.sysid, m.compid, MAV_MISSION_ACCEPTED);
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

    auto* state = find_uploading();
    if (!state || mr.seq >= state->upload_total)
        return;

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

    auto* state = find_uploading();
    if (!state || mr.seq >= state->upload_total)
        return;

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

    auto& state = select_transfer(ack.mission_type);
    state.record_recv();
    state.resend_fn = nullptr;
    state.progress.result = ack.type;
    state.progress.state = mg::core::mission::MissionState::Idle;

    if (ack.type == MAV_MISSION_ACCEPTED) {
        if (state.progress.received > 0 || state.progress.sent >= state.progress.total) {
            state.plan.valid = true;
        }
    } else {
        const char* label = (ack.mission_type == MAV_MISSION_TYPE_RALLY)   ? "Rally"
                            : (ack.mission_type == MAV_MISSION_TYPE_FENCE) ? "Fence"
                                                                           : "Task";
        state.progress.error = std::string(label) + " rejected: " + mg::core::mission::mission_result_message(ack.type);
    }
}

void MavlinkAdapter::Impl::handle_mission_current(const mavlink_message_t& m)
{
    mavlink_mission_current_t mc;
    mavlink_msg_mission_current_decode(&m, &mc);
    mission.plan.current_index = mc.seq;
}

void MavlinkAdapter::Impl::handle_mission_item_reached(const mavlink_message_t& m)
{
    mavlink_mission_item_reached_t mr;
    mavlink_msg_mission_item_reached_decode(&m, &mr);
    if (tel.mission_reached_cb)
        deferred_cbs.push_back([cb = tel.mission_reached_cb, seq = mr.seq]() { cb(seq); });
}

bool MavlinkAdapter::Impl::send_mission_request_int(uint8_t sysid, uint8_t compid, uint16_t seq)
{
    mavlink_msg_mission_request_int_pack(255, 0, &msg, sysid, compid, seq, 0);
    return send_msg();
}

bool MavlinkAdapter::Impl::send_mission_ack(uint8_t sysid, uint8_t compid, uint8_t type)
{
    mavlink_msg_mission_ack_pack(255, 0, &msg, sysid, compid, type, 0, 0);
    return send_msg();
}

bool MavlinkAdapter::Impl::send_mission_item_int(uint8_t sysid, uint8_t compid,
                                                 const mg::core::mission::MissionItem& item)
{
    mavlink_msg_mission_item_int_pack(255, 0, &msg, sysid, compid, item.seq, item.frame, item.command, item.current,
                                      item.autocontinue, item.param1, item.param2, item.param3, item.param4, item.x,
                                      item.y, item.z, item.mission_type);
    return send_msg();
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
    return send_msg();
}

} // namespace mg::core::mavlink
