/**
 * mavlink_adapter_command.cc - Command queue packing helper
 *
 * Definitions are out of line. 只负责把队列条目打包进 Impl::msg；发送由调用方
 * 决定（持锁派发路径入队，公开路径临时解锁直发，见 ADR-0005）。
 */

#include "mavlink_adapter_internal.h"

namespace mg::core::mavlink {

void MavlinkAdapter::Impl::pack_command_queue_entry(const CommandQueueEntry& e)
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
}

} // namespace mg::core::mavlink
