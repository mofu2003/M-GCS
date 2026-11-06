/**
 * tlog_format.h - 官方 .tlog 记录格式（QGC / Mission Planner 通用）
 *
 * 记录布局（每条消息一条记录）：
 *   [uint64 big-endian timestamp_us] [完整 MAVLink 帧字节 ...]
 *
 * - 时间戳为 Unix epoch 起的微秒数，**大端**存储。QGC 在
 *   `MAVLinkProtocol::logSentMessage/logReceivedMessage` 用
 *   `qToBigEndian(QDateTime::currentMSecsSinceEpoch()*1000)` 写入；
 *   Mission Planner 在 `MAVLinkInterface` 读取时 `Array.Reverse` 后再
 *   `BitConverter.ToUInt64`，即同样按大端解析。
 * - 帧字节为完整 MAVLink 帧（`STX..payload..CRC`），**没有长度前缀**：
 *   MAVLink 自身的 magic/len 字段已界定帧边界，解析方逐字节喂给
 *   `mavlink_parse_char` 直到组帧完成。
 *
 * 该头文件是写入器与回放器共用的唯一格式契约，避免两侧各写一套格式导致
 * 「录了但读不回来」。改动此处即改动磁盘格式，需同步 writer / replayer / 测试。
 *
 * 参考版本：QGC `bf0096d97`（`src/Comms/MAVLinkProtocol.cc`、`LogReplayLink.cc`），
 * MP `a2fcd74d6`（`ExtLibs/ArduPilot/Mavlink/MAVLinkInterface.cs`）。
 */
#ifndef MGCORE_TLOG_FORMAT_H
#define MGCORE_TLOG_FORMAT_H

#include <cstdint>
#include <cstddef>
#include <chrono>

namespace mg::core::log::tlog {

/// 每条记录前缀的时间戳字节数（uint64 大端）。
constexpr std::size_t kTimestampBytes = 8;

/// 把 Unix epoch 微秒时间戳按大端写入 8 字节缓冲。
inline void encode_timestamp(uint64_t timestamp_us, uint8_t out[kTimestampBytes]) {
    for (std::size_t i = 0; i < kTimestampBytes; ++i) {
        out[i] = static_cast<uint8_t>((timestamp_us >> (56 - 8 * i)) & 0xFFu);
    }
}

/// 从 8 字节大端缓冲解析 Unix epoch 微秒时间戳。
inline uint64_t decode_timestamp(const uint8_t in[kTimestampBytes]) {
    uint64_t value = 0;
    for (std::size_t i = 0; i < kTimestampBytes; ++i) {
        value = (value << 8) | static_cast<uint64_t>(in[i]);
    }
    return value;
}

/// 当前 Unix epoch 微秒时间戳（写入记录前缀用）。
inline uint64_t now_unix_micros() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

} // namespace mg::core::log::tlog

#endif /* MGCORE_TLOG_FORMAT_H */
