#pragma once

// Win32 串口链路实现，通过后台读取线程接收数据

#include "core/transport/link_interface.h"
#include "core/platform/platform_mutex.h"
#include <atomic>
#ifndef _WIN32
#include <thread>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace mg::core::transport {

// 串口通信链路，管理 COM 端口的打开/关闭、读写和后台接收线程
class SerialLink : public ILink {
public:
    explicit SerialLink(const LinkDescriptor& descriptor);
    ~SerialLink() override;

    // Non-copyable, non-movable (owns OS handles and threads)
    SerialLink(const SerialLink&) = delete;
    SerialLink& operator=(const SerialLink&) = delete;

    bool open() override;
    void close() override;
    bool is_open() const override;
    bool send(const uint8_t* data, size_t size) override;
    void set_receive_callback(ReceiveCallback cb) override;

    void join_read_thread();

#ifdef _WIN32
    static DWORD WINAPI read_thread_proc(LPVOID lpParam);
#else
    static void read_thread_proc(SerialLink* self);
#endif

    LinkDescriptor descriptor_;
    std::atomic<bool> opened_{false};
    std::atomic<bool> stop_requested_{false};
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;  // 串口句柄
    HANDLE read_thread_ = nullptr;          // 后台读取线程句柄
#else
    int fd_ = -1;                            // POSIX serial file descriptor
    std::thread read_thread_;                // 后台读取线程
#endif
    ReceiveCallback receive_cb_;
    mg::core::platform::Mutex cb_mtx_;  // 加锁保护回调函数
};

} // namespace mg::core::transport
