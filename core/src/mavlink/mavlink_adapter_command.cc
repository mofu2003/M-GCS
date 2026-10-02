/**
 * mavlink_adapter_command.cc - Command queue send helper
 *
 * Definitions are out of line. Dispatch, locking, timeout checks,
 * and callback execution semantics are unchanged.
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

bool MavlinkAdapter::Impl::send_command_queue_entry(const CommandQueueEntry& e)
{
    if (e.use_command_int) {
        mavlink_msg_command_int_pack(255, 0, &msg, e.target_sys, e.target_comp,
                                     e.frame, e.command, 0, 0,
                                     e.params[0], e.params[1], e.params[2], e.params[3],
                                     e.x, e.y, e.z);
    } else {
        mavlink_msg_command_long_pack(255, 0, &msg, e.target_sys, e.target_comp,
                                      e.command, e.confirmation,
                                      e.params[0], e.params[1], e.params[2], e.params[3],
                                      e.params[4], e.params[5], e.params[6]);
    }
    return send_msg();
}

} // namespace mg::core::mavlink
