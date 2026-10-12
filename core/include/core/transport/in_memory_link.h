/**
 * in_memory_link.h - 用于测试的内存传输适配器
 *
 * Implements ILink interface without any OS handles or threads.
 * Allows tests to inject MAVLink bytes directly and capture outgoing bytes.
 *
 * This creates a real seam at ILink: two adapters (SerialLink + InMemoryLink)
 * means the interface earns its keep.
 *
 * Usage:
 *   InMemoryLink link;
 *   link.open();
 *   link.inject_incoming(mavlink_bytes, len);  // simulate FC response
 *   auto sent = link.take_outgoing();           // inspect what was sent
 */

#ifndef MGCORE_IN_MEMORY_LINK_H
#define MGCORE_IN_MEMORY_LINK_H

#include "core/transport/link_interface.h"
#include <vector>
#include "core/platform/platform_mutex.h"
#include <mutex>

namespace mg::core::transport {

// 内存链路，用于单元测试，无操作系统句柄和线程，纯内存缓冲区
class InMemoryLink : public ILink {
public:
    InMemoryLink();
    ~InMemoryLink() override;

    // ILink interface
    bool open() override;
    void close() override;
    bool is_open() const override;
    bool send(const uint8_t* data, size_t size) override;
    void set_receive_callback(ReceiveCallback cb) override;

    // 测试辅助方法

    /// 注入字节模拟从传输层接收（模拟飞控发送数据）
    void inject_incoming(const uint8_t* data, size_t size);

    /// 取出通过 send() 发送的所有字节并清空内部缓冲区
    std::vector<uint8_t> take_outgoing();

    /// 获取通过 send() 发送的总字节数
    size_t total_sent() const;

private:
    bool opened_ = false;
    ReceiveCallback receive_cb_;
    std::vector<uint8_t> outgoing_buf_;  // 发送缓冲区
    size_t total_sent_ = 0;              // 累计发送字节数
    mutable mg::core::platform::Mutex mtx_;  // 加锁保护所有共享状态
};

} // namespace mg::core::transport

#endif // MGCORE_IN_MEMORY_LINK_H

