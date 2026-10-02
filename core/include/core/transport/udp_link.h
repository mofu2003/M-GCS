#pragma once

// UDP 链路实现，支持无连接数据报收发

#include "core/transport/link_interface.h"
#include "core/platform/platform_socket.h"
#include "core/platform/platform_mutex.h"
#include <atomic>
#include <memory>

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
    struct ReadState;

    bool start_read_thread(const std::shared_ptr<ReadState>& state);
    void wait_for_read_thread(const std::shared_ptr<ReadState>& state);
    std::shared_ptr<ReadState> state_snapshot() const;
#ifdef _WIN32
    static DWORD WINAPI read_thread_proc(LPVOID lpParam);
#else
    static void* read_thread_proc(void* lpParam);
#endif

    LinkDescriptor descriptor_;
    // The read thread owns a shared reference to its state. The link may be
    // destroyed while a blocked OS read is still unwinding; the thread will
    // release the final state reference itself and close the socket safely.
    mutable mg::core::platform::Mutex state_mtx_;
    std::shared_ptr<ReadState> read_state_;
    ReceiveCallback pending_receive_cb_;
    mg::core::platform::Mutex cb_mtx_;

};

} // namespace mg::core::transport
