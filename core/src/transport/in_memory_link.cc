/**
 * in_memory_link.cc - 内存传输适配器实现
 *
 * No OS handles, no threads, no COM ports.
 * Pure in-memory byte buffers for testing.
 */

#include "core/transport/in_memory_link.h"

namespace mg::core::transport {

InMemoryLink::InMemoryLink() = default;
InMemoryLink::~InMemoryLink() { close(); }

// 打开内存链路，清空发送缓冲区
bool InMemoryLink::open() {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
    opened_ = true;
    outgoing_buf_.clear();
    return true;
}

// 关闭内存链路
void InMemoryLink::close() {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
    opened_ = false;
}

// 查询是否已打开
bool InMemoryLink::is_open() const {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
    return opened_;
}

// 发送数据，追加到发送缓冲区
bool InMemoryLink::send(const uint8_t* data, size_t size) {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
    if (!opened_) return false;
    outgoing_buf_.insert(outgoing_buf_.end(), data, data + size);
    total_sent_ += size;
    return true;
}

// 设置接收回调
void InMemoryLink::set_receive_callback(ReceiveCallback cb) {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护回调
    receive_cb_ = std::move(cb);
}

// 注入模拟接收到的数据，在锁外调用回调以避免死锁
void InMemoryLink::inject_incoming(const uint8_t* data, size_t size) {
    ReceiveCallback cb;
    {
        mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
        cb = receive_cb_;
    }
    if (cb) {
        cb(data, size);
    }
}

// 取出并清空发送缓冲区
std::vector<uint8_t> InMemoryLink::take_outgoing() {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
    std::vector<uint8_t> result;
    result.swap(outgoing_buf_);
    return result;
}

// 获取累计发送字节数
size_t InMemoryLink::total_sent() const {
    mg::core::platform::LockGuard lk(mtx_);  // 加锁保护共享状态
    return total_sent_;
}

} // namespace mg::core::transport

