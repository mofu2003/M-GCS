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
}

} // namespace mg::core::mavlink
