/**
 * udp_link.cc - UDP transport implementation
 *
 * Platform-abstracted UDP link. The read thread owns a shared ReadState and
 * never captures the UdpLink object; this keeps close/destructor ordering safe
 * even when a Windows recvfrom() takes a long time to unwind.
 */
#include "core/transport/udp_link.h"
#include "core/log/logging.h"
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#ifndef _WIN32
#include <fcntl.h>
#endif

namespace mg::core::transport {

struct UdpLink::ReadState {
    std::atomic<bool> opened{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> thread_started{false};
    std::atomic<bool> finished{false};
    std::atomic<mg_socket_t> sock{MG_INVALID_SOCKET};

    ReceiveCallback receive_cb;
    PeerReceiveCallback peer_receive_cb;
    mg::core::platform::Mutex cb_mtx;

    sockaddr_in remote_addr{};
    std::atomic<bool> has_remote{false};
    mg::core::platform::Mutex remote_mtx;

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

UdpLink::UdpLink(const LinkDescriptor& descriptor)
    : descriptor_(descriptor) {}

UdpLink::~UdpLink() {
    close();
}

// ================================================================
// Parse host:port
// ================================================================

std::string UdpLink::host() const {
    const size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return descriptor_.endpoint;
    return descriptor_.endpoint.substr(0, colon);
}

uint16_t UdpLink::port() const {
    const size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return 0;
    try {
        return static_cast<uint16_t>(std::stoi(descriptor_.endpoint.substr(colon + 1)));
    } catch (const std::exception& e) {
        MGC_LOG_ERROR("[UdpLink] Invalid port: %s", e.what());
        return 0;
    }
}

std::shared_ptr<UdpLink::ReadState> UdpLink::state_snapshot() const {
    mg::core::platform::LockGuard lk(state_mtx_);
    return read_state_;
}

bool UdpLink::is_open() const {
    const auto state = state_snapshot();
    return state && state->opened.load();
}

bool UdpLink::start_read_thread(const std::shared_ptr<ReadState>& state) {
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
        MGC_LOG_ERROR("[UdpLink] Failed to create read thread: %lu",
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
        MGC_LOG_ERROR("[UdpLink] Failed to create read thread");
        return false;
    }
    pthread_detach(thread);
#endif
    return true;
}

void UdpLink::wait_for_read_thread(const std::shared_ptr<ReadState>& state) {
    if (!state || !state->thread_started.load()) return;

    mg::core::platform::UniqueLock lk(state->done_mtx);
    bool warned = false;
    while (!state->finished.load()) {
        if (!state->done_cv.wait_for(lk, std::chrono::seconds(5), [&] {
                return state->finished.load();
            })) {
            if (!warned) {
                MGC_LOG_WARN("[UdpLink] Read thread has not exited yet; "
                             "retaining socket/state until it does");
                warned = true;
            }
        }
    }
}

void UdpLink::set_receive_callback(ReceiveCallback cb) {
    const bool should_start = static_cast<bool>(cb);
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        pending_receive_cb_ = cb;
        if (should_start) pending_peer_receive_cb_ = nullptr;
    }

    const auto state = state_snapshot();
    if (!state) return;

    {
        mg::core::platform::LockGuard lk(state->cb_mtx);
        state->receive_cb = std::move(cb);
        if (state->receive_cb) state->peer_receive_cb = nullptr;
    }

    if (should_start && state->opened.load() && !state->thread_started.load()) {
        (void)start_read_thread(state);
    }
}

void UdpLink::set_peer_receive_callback(PeerReceiveCallback cb) {
    const bool should_start = static_cast<bool>(cb);
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        pending_peer_receive_cb_ = cb;
        if (should_start) pending_receive_cb_ = nullptr;
    }

    const auto state = state_snapshot();
    if (!state) return;

    {
        mg::core::platform::LockGuard lk(state->cb_mtx);
        state->peer_receive_cb = std::move(cb);
        if (state->peer_receive_cb) state->receive_cb = nullptr;
    }

    if (should_start && state->opened.load() && !state->thread_started.load()) {
        (void)start_read_thread(state);
    }
}

// ================================================================
// Open (bind to local port)
// ================================================================

bool UdpLink::open() {
    if (is_open()) return true;

    if (!mg::core::platform::socket_init()) {
        MGC_LOG_ERROR("[UdpLink] Winsock init failed");
        return false;
    }

    const std::string h = host();
    const uint16_t p = port();
    if (p == 0) {
        MGC_LOG_ERROR("[UdpLink] Invalid port in endpoint: %s", descriptor_.endpoint.c_str());
        mg::core::platform::socket_cleanup();
        return false;
    }

    const mg_socket_t socket = static_cast<mg_socket_t>(
        ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (socket == MG_INVALID_SOCKET) {
        MGC_LOG_ERROR("[UdpLink] Socket creation failed: %d",
                      mg::core::platform::socket_error());
        mg::core::platform::socket_cleanup();
        return false;
    }

    auto state = std::make_shared<ReadState>();
    state->sock.store(socket);

    int reuse = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse)) != 0) {
        MGC_LOG_WARN("[UdpLink] setsockopt SO_REUSEADDR failed: %d",
                     mg::core::platform::socket_error());
    }

    sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(p);
    if (h.empty() || h == "0.0.0.0") {
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, h.c_str(), &bind_addr.sin_addr) != 1) {
        MGC_LOG_ERROR("[UdpLink] inet_pton failed for %s", h.c_str());
        state->close_socket();
        mg::core::platform::socket_cleanup();
        return false;
    }

    if (::bind(socket, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) == MG_SOCKET_ERR) {
        MGC_LOG_ERROR("[UdpLink] Bind to %s:%u failed: %d",
                      h.c_str(), p, mg::core::platform::socket_error());
        state->close_socket();
        mg::core::platform::socket_cleanup();
        return false;
    }

#ifdef _WIN32
    u_long nonblock = 1;
    ioctlsocket(socket, FIONBIO, &nonblock);
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    }
#endif

    {
        // Publish the state while holding cb_mtx_ so a concurrent
        // set_receive_callback() cannot be lost between these two steps.
        mg::core::platform::LockGuard lk(cb_mtx_);
        {
            mg::core::platform::LockGuard cb_lk(state->cb_mtx);
            state->receive_cb = pending_receive_cb_;
            state->peer_receive_cb = pending_peer_receive_cb_;
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

    MGC_LOG_INFO("[UdpLink] Bound to %s:%u", h.c_str(), p);
    return true;
}

// ================================================================
// Close
// ================================================================

void UdpLink::close() {
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
        state->peer_receive_cb = nullptr;
    }
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        pending_receive_cb_ = nullptr;
        pending_peer_receive_cb_ = nullptr;
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
    state->has_remote.store(false);
    {
        mg::core::platform::LockGuard lk(state->remote_mtx);
        state->remote_addr = {};
    }
    MGC_LOG_INFO("[UdpLink] Closed");
}

// ================================================================
// Send (to learned remote or specified address)
// ================================================================

bool UdpLink::send(const uint8_t* data, size_t size) {
    const auto state = state_snapshot();
    if (!state || !state->opened.load() || !state->has_remote.load()) return false;

    const mg_socket_t socket = state->sock.load();
    if (socket == MG_INVALID_SOCKET) return false;

    sockaddr_in remote_addr{};
    {
        mg::core::platform::LockGuard lk(state->remote_mtx);
        remote_addr = state->remote_addr;
    }

    const auto sent = ::sendto(socket, reinterpret_cast<const char*>(data),
                               static_cast<int>(size), 0,
                               reinterpret_cast<sockaddr*>(&remote_addr),
                               sizeof(remote_addr));
    return sent != MG_SOCKET_ERR;
}

bool UdpLink::send_to(const LinkPeer& peer,
                      const uint8_t* data,
                      size_t size) {
    if (!peer.valid() || !data) return false;
    const auto state = state_snapshot();
    if (!state || !state->opened.load()) return false;

    const mg_socket_t socket = state->sock.load();
    if (socket == MG_INVALID_SOCKET) return false;

    sockaddr_in remote_addr{};
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(peer.port);
    if (inet_pton(AF_INET, peer.host.c_str(), &remote_addr.sin_addr) != 1) {
        return false;
    }

    const auto sent = ::sendto(socket, reinterpret_cast<const char*>(data),
                               static_cast<int>(size), 0,
                               reinterpret_cast<sockaddr*>(&remote_addr),
                               sizeof(remote_addr));
    return sent != MG_SOCKET_ERR;
}

// ================================================================
// Read thread
// ================================================================

#ifdef _WIN32
DWORD WINAPI UdpLink::read_thread_proc(LPVOID lpParam) {
#else
void* UdpLink::read_thread_proc(void* lpParam) {
#endif
    auto* arg = static_cast<std::shared_ptr<ReadState>*>(lpParam);
    std::shared_ptr<ReadState> state = std::move(*arg);
    delete arg;

    uint8_t buffer[4096];
    while (!state->stop_requested.load()) {
        const mg_socket_t socket = state->sock.load();
        if (socket == MG_INVALID_SOCKET) break;

        sockaddr_in sender_addr{};
#ifdef _WIN32
        int sender_len = sizeof(sender_addr);
#else
        socklen_t sender_len = sizeof(sender_addr);
#endif
        const auto received = ::recvfrom(
            socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
            reinterpret_cast<sockaddr*>(&sender_addr), &sender_len);

        if (received > 0) {
            char addr_str[INET_ADDRSTRLEN]{};
            inet_ntop(AF_INET, &sender_addr.sin_addr, addr_str, sizeof(addr_str));
            if (!state->has_remote.load()) {
                {
                    mg::core::platform::LockGuard lk(state->remote_mtx);
                    state->remote_addr = sender_addr;
                    state->has_remote.store(true);
                }
                MGC_LOG_INFO("[UdpLink] Learned remote: %s:%u",
                             addr_str, ntohs(sender_addr.sin_port));
            }

            const LinkPeer peer{
                addr_str,
                static_cast<uint16_t>(ntohs(sender_addr.sin_port))};
            PeerReceiveCallback peer_cb_copy;
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(state->cb_mtx);
                peer_cb_copy = state->peer_receive_cb;
                cb_copy = state->receive_cb;
            }
            if (peer_cb_copy) {
                peer_cb_copy(peer, buffer, static_cast<size_t>(received));
            } else if (cb_copy) {
                cb_copy(buffer, static_cast<size_t>(received));
            }
        } else if (received == MG_SOCKET_ERR) {
            const int err = mg::core::platform::socket_error();
#ifdef _WIN32
            if (err == WSAEINTR || err == WSAEWOULDBLOCK) {
                Sleep(10);
                continue;
            }
#else
            if (err == EINTR || err == EAGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
#endif
            if (!state->stop_requested.load()) {
                MGC_LOG_ERROR("[UdpLink] Recvfrom error: %d", err);
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
