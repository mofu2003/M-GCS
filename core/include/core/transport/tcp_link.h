#pragma once

// TCP 链路实现，支持客户端模式连接

#include "core/transport/link_interface.h"
#include "core/platform/platform_socket.h"
#include "core/platform/platform_mutex.h"
#include <atomic>

namespace mg::core::transport {

// TCP 连接模式
enum class TcpMode {
    Client,
};

// TCP 通信链路，通过 socket 实现网络数据收发
class TcpLink : public ILink {
public:
    explicit TcpLink(const LinkDescriptor& descriptor, TcpMode mode = TcpMode::Client);
    ~TcpLink() override;

    TcpLink(const TcpLink&) = delete;
    TcpLink& operator=(const TcpLink&) = delete;

    bool open() override;
    void close() override;
    bool is_open() const override;
    bool send(const uint8_t* data, size_t size) override;
    void set_receive_callback(ReceiveCallback cb) override;

    std::string host() const;
    uint16_t port() const;

private:
    void join_read_thread();
#ifdef _WIN32
    static DWORD WINAPI read_thread_proc(LPVOID lpParam);
#else
    static void* read_thread_proc(void* lpParam);
#endif

    LinkDescriptor descriptor_;
    TcpMode mode_;
    std::atomic<bool> opened_{false};
    std::atomic<bool> stop_requested_{false};
    mg_socket_t sock_ = MG_INVALID_SOCKET;
#ifdef _WIN32
    HANDLE read_thread_ = nullptr;
#else
    pthread_t read_thread_ = 0;
#endif
    ReceiveCallback receive_cb_;
    mg::core::platform::Mutex cb_mtx_;
};

} // namespace mg::core::transport
