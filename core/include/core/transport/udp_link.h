#pragma once

// UDP 链路实现，支持无连接数据报收发

#include "core/transport/link_interface.h"
#include "core/platform/platform_socket.h"
#include "core/platform/platform_mutex.h"
#include <atomic>

namespace mg::core::transport {

// UDP 通信链路，通过 socket 实现数据报收发，支持动态设置远端地址
class UdpLink : public ILink {
public:
    explicit UdpLink(const LinkDescriptor& descriptor);
    ~UdpLink() override;

    UdpLink(const UdpLink&) = delete;
    UdpLink& operator=(const UdpLink&) = delete;

    bool open() override;
    void close() override;
    bool is_open() const override;
    bool send(const uint8_t* data, size_t size) override;
    void set_receive_callback(ReceiveCallback cb) override;

    void set_remote_address(const std::string& host, uint16_t port);
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

    struct sockaddr_in remote_addr_{};
    std::atomic<bool> has_remote_{false};
    mg::core::platform::Mutex remote_mtx_;
};

} // namespace mg::core::transport
