/**
 * mavlink_message_names.cc - MAVLink 消息 ID 名称查询实现
 *
 * `MAVLINK_USE_MESSAGE_INFO` 会把整个方言的名称表实例化成函数内静态数组，
 * 因此只在这一个翻译单元打开：公共头保持轻量，其它 core 文件只调用
 * message_name()，不为名称表付出编译时间。
 */
#include "core/mavlink/mavlink_message_names.h"

#include <cstddef>

#define MAVLINK_USE_MESSAGE_INFO
#include "ardupilotmega/mavlink.h"

namespace mg::core::mavlink {

const char* message_name(uint32_t msgid) noexcept {
    mavlink_message_t message{};
    message.msgid = msgid;
    const mavlink_message_info_t* info = mavlink_get_message_info(&message);
    return info != nullptr ? info->name : nullptr;
}

} // namespace mg::core::mavlink
