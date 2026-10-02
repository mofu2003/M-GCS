#pragma once

// TCP 链路实现，支持客户端模式连接

#include "core/transport/link_interface.h"
#include "core/platform/platform_socket.h"
#include "core/platform/platform_mutex.h"
#include <atomic>
#include <memory>

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
