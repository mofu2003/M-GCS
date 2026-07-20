/**
 * tcp_link.cc - TCP transport implementation
 *
 * Platform-abstracted TCP client link.
 * Win32: Winsock2 API, POSIX: BSD sockets API.
 *
 * v2.0: Thread safety fixes:
 *   - set_receive_callback() starts read thread if already open
 *   - receive_cb_ protected by cb_mtx_ for cross-thread safety
 *   - CreateThread/WaitForSingleObject return values checked
 *   - std::stoi wrapped in try-catch
 *   - setsockopt/inet_pton return values checked
 */
#include "core/transport/tcp_link.h"
#include "core/log/logging.h"
#include <cstring>
#include <sstream>

namespace mg::core::transport {

// ================================================================
// Constructor / Destructor
// ================================================================

TcpLink::TcpLink(const LinkDescriptor& descriptor, TcpMode mode)
    : descriptor_(descriptor), mode_(mode) {}

TcpLink::~TcpLink() {
    close();
}

// ================================================================
// Parse host:port
// ================================================================

std::string TcpLink::host() const {
    size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return descriptor_.endpoint;
    return descriptor_.endpoint.substr(0, colon);
}

uint16_t TcpLink::port() const {
    size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return 0;
    try {
        return static_cast<uint16_t>(std::stoi(descriptor_.endpoint.substr(colon + 1)));
    } catch (const std::exception& e) {
        MGC_LOG_ERROR("[TcpLink] Invalid port: %s", e.what());
        return 0;
    }
}

bool TcpLink::is_open() const {
    return opened_.load();
}

void TcpLink::set_receive_callback(ReceiveCallback cb) {
    const bool should_start = static_cast<bool>(cb);
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        receive_cb_ = std::move(cb);
    }
    // If already open and no thread yet, start one
    if (opened_.load() && should_start && !read_thread_) {
#ifdef _WIN32
        HANDLE h = CreateThread(nullptr, 0, read_thread_proc, this, 0, nullptr);
        if (!h) {
            MGC_LOG_ERROR("[TcpLink] Failed to create read thread: %lu", static_cast<unsigned long>(GetLastError()));
            return;
        }
        read_thread_ = h;
#else
        pthread_t tid = 0;
        if (pthread_create(&tid, nullptr, read_thread_proc, this) != 0) {
            MGC_LOG_ERROR("[TcpLink] Failed to create read thread");
            return;
        }
        read_thread_ = tid;
#endif
    }
}

// ================================================================
// Open (client mode)
// ================================================================

bool TcpLink::open() {
    if (opened_.load()) return true;

    if (!mg::core::platform::socket_init()) {
        MGC_LOG_ERROR("[TcpLink] Winsock init failed");
        return false;
    }

    std::string h = host();
    uint16_t p = port();
    if (h.empty() || p == 0) {
        MGC_LOG_ERROR("[TcpLink] Invalid endpoint: %s", descriptor_.endpoint.c_str());
        return false;
    }

    // Create socket
    sock_ = static_cast<mg_socket_t>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (sock_ == MG_INVALID_SOCKET) {
        MGC_LOG_ERROR("[TcpLink] Socket creation failed: %d", mg::core::platform::socket_error());
        return false;
    }

    // Disable Nagle's algorithm for low-latency
    int flag = 1;
    if (setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0) {
        MGC_LOG_WARN("[TcpLink] setsockopt TCP_NODELAY failed");
    }

    // Connect
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(p);
    if (inet_pton(AF_INET, h.c_str(), &addr.sin_addr) != 1) {
        MGC_LOG_ERROR("[TcpLink] inet_pton failed for %s", h.c_str());
        mg::core::platform::socket_close(sock_);
        sock_ = MG_INVALID_SOCKET;
        return false;
    }

    if (::connect(sock_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == MG_SOCKET_ERR) {
        MGC_LOG_ERROR("[TcpLink] Connect to %s:%u failed: %d", h.c_str(), p, mg::core::platform::socket_error());
        mg::core::platform::socket_close(sock_);
        sock_ = MG_INVALID_SOCKET;
        return false;
    }

    stop_requested_.store(false);
    opened_.store(true);

    // Start read thread (only if callback already set)
    if (receive_cb_) {
#ifdef _WIN32
        HANDLE h = CreateThread(nullptr, 0, read_thread_proc, this, 0, nullptr);
        if (!h) {
            MGC_LOG_ERROR("[TcpLink] Failed to create read thread: %lu", static_cast<unsigned long>(GetLastError()));
            mg::core::platform::socket_close(sock_);
            sock_ = MG_INVALID_SOCKET;
            opened_.store(false);
            return false;
        }
        read_thread_ = h;
#else
        pthread_t tid = 0;
        if (pthread_create(&tid, nullptr, read_thread_proc, this) != 0) {
            MGC_LOG_ERROR("[TcpLink] Failed to create read thread");
            mg::core::platform::socket_close(sock_);
            sock_ = MG_INVALID_SOCKET;
            opened_.store(false);
            return false;
        }
        read_thread_ = tid;
#endif
    }

    MGC_LOG_INFO("[TcpLink] Connected to %s:%u", h.c_str(), p);
    return true;
}

// ================================================================
// Join read thread ? safe to call from any thread
// ================================================================

void TcpLink::join_read_thread() {
#ifdef _WIN32
    if (!read_thread_) return;
    if (sock_ != MG_INVALID_SOCKET) {
        ::shutdown(sock_, SD_BOTH);
    }
    DWORD wait_ret = WaitForSingleObject(read_thread_, 5000);
    if (wait_ret == WAIT_TIMEOUT) {
        MGC_LOG_WARN("[TcpLink] Read thread did not exit within 5s");
    }
    CloseHandle(read_thread_);
    read_thread_ = nullptr;
#else
    if (!read_thread_) return;
    if (sock_ != MG_INVALID_SOCKET) {
        ::shutdown(sock_, SHUT_RDWR);
    }
    int ret = pthread_join(read_thread_, nullptr);
    if (ret != 0) {
        MGC_LOG_WARN("[TcpLink] Read thread join failed (%d)", ret);
    }
    read_thread_ = 0;
#endif
}

// ================================================================
// Close
// ================================================================

void TcpLink::close() {
    if (!opened_.load()) return;

    stop_requested_.store(true);
    set_receive_callback(nullptr);

    // Interrupt blocked recv() and wait for thread exit
    join_read_thread();

    if (sock_ != MG_INVALID_SOCKET) {
        mg::core::platform::socket_close(sock_);
        sock_ = MG_INVALID_SOCKET;
    }

    mg::core::platform::socket_cleanup();
    opened_.store(false);
    MGC_LOG_INFO("[TcpLink] Closed");
}

// ================================================================
// Send (synchronous)
// ================================================================

bool TcpLink::send(const uint8_t* data, size_t size) {
    if (!opened_.load() || sock_ == MG_INVALID_SOCKET) return false;

    size_t total_sent = 0;
    while (total_sent < size) {
        auto sent = ::send(sock_, reinterpret_cast<const char*>(data + total_sent),
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
    auto* self = static_cast<TcpLink*>(lpParam);
    uint8_t buffer[4096];

    while (!self->stop_requested_.load()) {
        auto received = ::recv(self->sock_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
        if (received > 0) {
            // Copy callback under lock, invoke outside
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(self->cb_mtx_);
                cb_copy = self->receive_cb_;
            }
            if (cb_copy) {
                cb_copy(buffer, static_cast<size_t>(received));
            }
        } else if (received == 0) {
            MGC_LOG_INFO("[TcpLink] Remote closed connection");
            break;
        } else {
            int err = mg::core::platform::socket_error();
#ifdef _WIN32
            if (err == WSAEINTR || err == WSAEWOULDBLOCK) continue;
#else
            if (err == EINTR || err == EAGAIN) continue;
#endif
            if (!self->stop_requested_.load()) {
                MGC_LOG_ERROR("[TcpLink] Recv error: %d", err);
            }
            break;
        }
    }

#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

} // namespace mg::core::transport
