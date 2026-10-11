/**
 * tcp_link.cc - TCP transport implementation
 *
 * Platform-abstracted TCP client link.
 * Win32: Winsock2 API, POSIX: BSD sockets API.
 *
 * The read thread never captures the TcpLink object. It owns a shared
 * ReadState instead, so close/destructor ordering cannot turn a delayed
 * Windows read-thread exit into a use-after-free.
 */
#include "core/transport/tcp_link.h"
#include "core/log/logging.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <sstream>

#ifndef _WIN32
  #include <fcntl.h>
  #include <poll.h>
#endif

namespace mg::core::transport {

namespace {

/// 连接建立的等待上限。默认的阻塞 connect 在不可达地址上会一直等到操作
/// 系统超时（可达 75 s），而看门狗要等连接返回后才启动（KD-25）。
constexpr int kConnectTimeoutMs = 3000;

#ifdef _WIN32
bool connect_with_timeout(mg_socket_t sock, const sockaddr_in& addr, int timeout_ms) {
    u_long nonblocking = 1;
    if (ioctlsocket(sock, FIONBIO, &nonblocking) != 0) return false;

    if (::connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
        u_long blocking = 0;
        (void)ioctlsocket(sock, FIONBIO, &blocking);
        return true;
    }
    const int err = mg::core::platform::socket_error();
    if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS && err != WSAEINVAL) return false;

    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (select(0, nullptr, &write_set, nullptr, &tv) <= 0) return false;

    int so_error = 0;
    int len = sizeof(so_error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                   reinterpret_cast<char*>(&so_error), &len) != 0 ||
        so_error != 0) {
        return false;
    }
    u_long blocking = 0;
    (void)ioctlsocket(sock, FIONBIO, &blocking);
    return true;
}
#else
bool connect_with_timeout(mg_socket_t sock, const sockaddr_in& addr, int timeout_ms) {
    const int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) return false;

    if (::connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
        (void)fcntl(sock, F_SETFL, flags);
        return true;
    }
    if (errno != EINPROGRESS) {
        (void)fcntl(sock, F_SETFL, flags);
        return false;
    }

    pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLOUT;
    const int ready = ::poll(&pfd, 1, timeout_ms);
    if (ready <= 0) {
        (void)fcntl(sock, F_SETFL, flags);
        return false;
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    const bool ok = getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len) == 0 &&
                    so_error == 0;
    // 读取线程用阻塞 recv，连接成功后恢复原来的阻塞标志。
    (void)fcntl(sock, F_SETFL, flags);
    return ok;
}
#endif

}  // namespace

struct TcpLink::ReadState {
    std::atomic<bool> opened{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> thread_started{false};
    std::atomic<bool> finished{false};
    std::atomic<mg_socket_t> sock{MG_INVALID_SOCKET};

    ReceiveCallback receive_cb;
    mg::core::platform::Mutex cb_mtx;

    mg::core::platform::Mutex done_mtx;
    mg::core::platform::ConditionVariable done_cv;

    ~ReadState() {
        const mg_socket_t socket = sock.exchange(MG_INVALID_SOCKET);
        if (socket != MG_INVALID_SOCKET) {
            mg::core::platform::socket_close(socket);
        }
    }

    void close_socket() {
        const mg_socket_t socket = sock.exchange(MG_INVALID_SOCKET);
        if (socket != MG_INVALID_SOCKET) {
            mg::core::platform::socket_close(socket);
        }
    }
};

// ================================================================
// Constructor / Destructor
// ================================================================

TcpLink::TcpLink(const LinkDescriptor& descriptor, TcpMode mode)
    : descriptor_(descriptor) {
    (void)mode;
}

TcpLink::~TcpLink() {
    close();
}

// ================================================================
// Parse host:port
// ================================================================

std::string TcpLink::host() const {
    const size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return descriptor_.endpoint;
    return descriptor_.endpoint.substr(0, colon);
}

uint16_t TcpLink::port() const {
    const size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return 0;
    try {
        return static_cast<uint16_t>(std::stoi(descriptor_.endpoint.substr(colon + 1)));
    } catch (const std::exception& e) {
        MGC_LOG_ERROR("[TcpLink] Invalid port: %s", e.what());
        return 0;
    }
}

std::shared_ptr<TcpLink::ReadState> TcpLink::state_snapshot() const {
    mg::core::platform::LockGuard lk(state_mtx_);
    return read_state_;
}

bool TcpLink::is_open() const {
    const auto state = state_snapshot();
    return state && state->opened.load();
}

bool TcpLink::start_read_thread(const std::shared_ptr<ReadState>& state) {
    if (!state || state->thread_started.exchange(true)) return true;

    auto* arg = new std::shared_ptr<ReadState>(state);
#ifdef _WIN32
    HANDLE thread = CreateThread(nullptr, 0, read_thread_proc, arg, 0, nullptr);
    if (!thread) {
        const DWORD error = GetLastError();
        delete arg;
        state->thread_started.store(false);
        state->finished.store(true);
        state->done_cv.notify_all();
        MGC_LOG_ERROR("[TcpLink] Failed to create read thread: %lu",
                      static_cast<unsigned long>(error));
        return false;
    }
    // The thread owns the state and does not need the Win32 handle after
    // creation. Closing the handle here cannot terminate the running thread.
    CloseHandle(thread);
#else
    pthread_t thread = 0;
    if (pthread_create(&thread, nullptr, read_thread_proc, arg) != 0) {
        delete arg;
        state->thread_started.store(false);
        state->finished.store(true);
        state->done_cv.notify_all();
        MGC_LOG_ERROR("[TcpLink] Failed to create read thread");
        return false;
    }
    pthread_detach(thread);
#endif
    return true;
}

void TcpLink::wait_for_read_thread(const std::shared_ptr<ReadState>& state) {
    if (!state || !state->thread_started.load()) return;

    mg::core::platform::UniqueLock lk(state->done_mtx);
    bool warned = false;
    while (!state->finished.load()) {
        if (!state->done_cv.wait_for(lk, std::chrono::seconds(5), [&] {
                return state->finished.load();
            })) {
            if (!warned) {
                MGC_LOG_WARN("[TcpLink] Read thread has not exited yet; "
                             "retaining socket/state until it does");
                warned = true;
            }
        }
    }
}

void TcpLink::set_receive_callback(ReceiveCallback cb) {
    const bool should_start = static_cast<bool>(cb);
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        pending_receive_cb_ = cb;
    }

    const auto state = state_snapshot();
    if (!state) return;

    {
        mg::core::platform::LockGuard lk(state->cb_mtx);
        state->receive_cb = std::move(cb);
    }

    if (should_start && state->opened.load() && !state->thread_started.load()) {
        (void)start_read_thread(state);
    }
}

// ================================================================
// Open (client mode)
// ================================================================

bool TcpLink::open() {
    if (is_open()) return true;

    if (!mg::core::platform::socket_init()) {
        MGC_LOG_ERROR("[TcpLink] Winsock init failed");
        return false;
    }

    const std::string h = host();
    const uint16_t p = port();
    if (h.empty() || p == 0) {
        MGC_LOG_ERROR("[TcpLink] Invalid endpoint: %s", descriptor_.endpoint.c_str());
        mg::core::platform::socket_cleanup();
        return false;
    }

    const mg_socket_t socket = static_cast<mg_socket_t>(
        ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (socket == MG_INVALID_SOCKET) {
        MGC_LOG_ERROR("[TcpLink] Socket creation failed: %d",
                      mg::core::platform::socket_error());
        mg::core::platform::socket_cleanup();
        return false;
    }

    auto state = std::make_shared<ReadState>();
    state->sock.store(socket);

    int flag = 1;
    if (setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0) {
        MGC_LOG_WARN("[TcpLink] setsockopt TCP_NODELAY failed");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(p);
    if (inet_pton(AF_INET, h.c_str(), &addr.sin_addr) != 1) {
        MGC_LOG_ERROR("[TcpLink] inet_pton failed for %s", h.c_str());
        state->close_socket();
        mg::core::platform::socket_cleanup();
        return false;
    }

    if (!connect_with_timeout(socket, addr, kConnectTimeoutMs)) {
        MGC_LOG_ERROR("[TcpLink] Connect to %s:%u failed or timed out (%d ms budget): %d",
                      h.c_str(), p, kConnectTimeoutMs,
                      mg::core::platform::socket_error());
        state->close_socket();
        mg::core::platform::socket_cleanup();
        return false;
    }

    {
        // Publish the state while holding cb_mtx_ so a concurrent
        // set_receive_callback() cannot be lost between these two steps.
        mg::core::platform::LockGuard lk(cb_mtx_);
        {
            mg::core::platform::LockGuard cb_lk(state->cb_mtx);
            state->receive_cb = pending_receive_cb_;
        }
        state->opened.store(true);
        {
            mg::core::platform::LockGuard state_lk(state_mtx_);
            read_state_ = state;
        }
    }

    bool has_callback = false;
    {
        mg::core::platform::LockGuard lk(state->cb_mtx);
        has_callback = static_cast<bool>(state->receive_cb);
    }
    if (has_callback && !start_read_thread(state)) {
        state->opened.store(false);
        state->close_socket();
        {
            mg::core::platform::LockGuard lk(state_mtx_);
            if (read_state_ == state) read_state_.reset();
        }
        mg::core::platform::socket_cleanup();
        return false;
    }

    MGC_LOG_INFO("[TcpLink] Connected to %s:%u", h.c_str(), p);
    return true;
}

// ================================================================
// Close
// ================================================================

void TcpLink::close() {
    const auto state = state_snapshot();
    if (!state) return;

    {
        mg::core::platform::LockGuard lk(state_mtx_);
        if (read_state_ == state) read_state_.reset();
    }

    state->stop_requested.store(true);
    state->opened.store(false);
    {
        mg::core::platform::LockGuard lk(state->cb_mtx);
        state->receive_cb = nullptr;
    }
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        pending_receive_cb_ = nullptr;
    }

    const mg_socket_t socket = state->sock.load();
    if (socket != MG_INVALID_SOCKET) {
        mg::core::platform::socket_shutdown(socket);
    }

    // Never release the socket/state while the OS read is still running.
    // If a driver ignores shutdown, this blocks rather than trading safety
    // for a destructor race.
    wait_for_read_thread(state);
    state->close_socket();
    mg::core::platform::socket_cleanup();
    MGC_LOG_INFO("[TcpLink] Closed");
}

// ================================================================
// Send (synchronous)
// ================================================================

bool TcpLink::send(const uint8_t* data, size_t size) {
    const auto state = state_snapshot();
    if (!state || !state->opened.load()) return false;

    const mg_socket_t socket = state->sock.load();
    if (socket == MG_INVALID_SOCKET) return false;

    size_t total_sent = 0;
    while (total_sent < size) {
        const auto sent = ::send(socket, reinterpret_cast<const char*>(data + total_sent),
                                 static_cast<int>(size - total_sent), 0);
        if (sent == MG_SOCKET_ERR) {
            MGC_LOG_ERROR("[TcpLink] Send failed: %d", mg::core::platform::socket_error());
            return false;
        }
        total_sent += static_cast<size_t>(sent);
    }
    return true;
}

// ================================================================
// Read thread
// ================================================================

#ifdef _WIN32
DWORD WINAPI TcpLink::read_thread_proc(LPVOID lpParam) {
#else
void* TcpLink::read_thread_proc(void* lpParam) {
#endif
    auto* arg = static_cast<std::shared_ptr<ReadState>*>(lpParam);
    std::shared_ptr<ReadState> state = std::move(*arg);
    delete arg;

    uint8_t buffer[4096];
    while (!state->stop_requested.load()) {
        const mg_socket_t socket = state->sock.load();
        if (socket == MG_INVALID_SOCKET) break;

        const auto received = ::recv(socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
        if (received > 0) {
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(state->cb_mtx);
                cb_copy = state->receive_cb;
            }
            if (cb_copy) {
                cb_copy(buffer, static_cast<size_t>(received));
            }
        } else if (received == 0) {
            MGC_LOG_INFO("[TcpLink] Remote closed connection");
            state->opened.store(false);
            break;
        } else {
            const int err = mg::core::platform::socket_error();
#ifdef _WIN32
            if (err == WSAEINTR || err == WSAEWOULDBLOCK) continue;
#else
            if (err == EINTR || err == EAGAIN) continue;
#endif
            if (!state->stop_requested.load()) {
                MGC_LOG_ERROR("[TcpLink] Recv error: %d", err);
                state->opened.store(false);
            }
            break;
        }
    }

    if (!state->stop_requested.load()) {
        state->opened.store(false);
    }
    state->finished.store(true);
    state->done_cv.notify_all();

#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

} // namespace mg::core::transport
