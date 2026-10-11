#pragma once

// Win32 串口链路实现，通过后台读取线程接收数据

#include "core/transport/link_interface.h"
#include "core/platform/platform_mutex.h"
#include <atomic>
#include <memory>
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

    /// 读取线程启动失败时关闭端口并标记链路未打开（KD-05）。
    void fail_read_thread_start();

    /// 读线程自持的状态（定义在 .cc）。读循环只访问这个对象，不访问 SerialLink
    /// 本身；因此 join 超时后即使 SerialLink 被销毁，线程也不会读到已释放的内存。
    struct Reader;

#ifdef _WIN32
    static DWORD WINAPI read_thread_proc(LPVOID lpParam);
#else
    static void read_thread_proc(std::shared_ptr<Reader> reader);
#endif

    LinkDescriptor descriptor_;
    std::shared_ptr<Reader> reader_;

    /// join 超时后置位：句柄所有权留给仍在运行的读线程，close() 不再关闭它。
    bool abandoned_ = false;

#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;  // 串口句柄
    HANDLE read_thread_ = nullptr;          // 后台读取线程句柄
#else
    int fd_ = -1;                            // POSIX serial file descriptor
    std::thread read_thread_;                // 后台读取线程
#endif
};

} // namespace mg::core::transport
