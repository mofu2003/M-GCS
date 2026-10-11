/**
 * mavlink_message_names.h - MAVLink 消息 ID 到方言名称的查询
 *
 * 名称直接来自生成的方言头（`ardupilotmega.h` 的 `MAVLINK_MESSAGE_NAMES` /
 * `MAVLINK_MESSAGE_INFO`），不手工维护映射表：方言升级后名称自动跟着变。
 */
#ifndef MGCORE_MAVLINK_MESSAGE_NAMES_H
#define MGCORE_MAVLINK_MESSAGE_NAMES_H

#include <cstdint>

namespace mg::core::mavlink {

/// 返回消息 ID 的方言名称（如 `HEARTBEAT`、`GLOBAL_POSITION_INT`）。
/// 当前方言里未定义该 ID 时返回 nullptr，调用方自行决定兜底文本。
const char* message_name(uint32_t msgid) noexcept;

} // namespace mg::core::mavlink

#endif /* MGCORE_MAVLINK_MESSAGE_NAMES_H */
