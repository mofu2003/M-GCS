/**
 * mavlink_adapter_params.cc - Parameter protocol helpers
 *
 * Definitions are out of line. Dispatch, locking, timeout checks,
 * and callback execution semantics are unchanged.
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

bool MavlinkAdapter::Impl::send_param_request_read_by_index(uint8_t sysid, uint8_t compid, int16_t param_index)
{
    char empty_name[16] = {};
    mavlink_msg_param_request_read_pack(255, 0, &msg, sysid, compid, empty_name, param_index);
    return send_msg();
}

void MavlinkAdapter::Impl::process_param_timeouts()
{
    int action = pc.check_timeouts(deferred_cbs);
    switch (action) {
    case 1: {
        // 重试 PARAM_REQUEST_LIST
        mavlink_msg_param_request_list_pack(255, 0, &msg, pc.target_sys, pc.target_comp);
        send_msg();
        break;
    }
    case 2: {
        // 请求缺失参数批次
        for (uint16_t idx : pc.batch_queue) {
            send_param_request_read_by_index(pc.target_sys, pc.target_comp, idx);
        }
        break;
    }
    default:
        break;
    }

    if (!pc.write_retry_queue.empty()) {
        auto retries = std::move(pc.write_retry_queue);
        pc.write_retry_queue.clear();
        for (const auto& pending : retries) {
            const auto current = pc.pending_writes.find(pending.name);
            if (current == pc.pending_writes.end() ||
                current->second.history_id != pending.history_id) {
                continue;
            }
            mavlink_msg_param_set_pack(
                255, 0, &msg,
                pending.target_sys, pending.target_comp,
                pending.name.c_str(), pending.value, pending.type);
            if (!send_msg()) {
                pc.complete_write(
                    pending.history_id,
                    false,
                    pending.value,
                    "send_msg failed for PARAM_SET retry");
                pc.pending_writes.erase(current);
            }
        }
    }

    if (!pc.refresh_retry_queue.empty()) {
        auto retries = std::move(pc.refresh_retry_queue);
        pc.refresh_retry_queue.clear();
        for (const auto& request : retries) {
            mavlink_msg_param_request_read_pack(
                255, 0, &msg,
                request.target_sys, request.target_comp,
                request.name.c_str(), -1);
            if (!send_msg()) {
                pc.fail_refresh(request.name);
            }
        }
    }
}

} // namespace mg::core::mavlink
