/**
 * udp_link.cc - UDP transport implementation
 *
 * Platform-abstracted UDP link.
 * Binds to local host:port, receives datagrams from any source.
 * Automatically learns remote address from first incoming packet.
 *
 * v2.0: Thread safety fixes:
 *   - set_receive_callback() starts read thread if already open
 *   - receive_cb_ protected by cb_mtx_ for cross-thread safety
 *   - has_remote_ is atomic; remote_addr_ written once before thread start
 *   - CreateThread/WaitForSingleObject return values checked
 *   - std::stoi wrapped in try-catch
 */
#include "core/transport/udp_link.h"
#include "core/log/logging.h"
#include <chrono>
#include <cstring>
#include <thread>
#ifndef _WIN32
#include <fcntl.h>
#endif

namespace mg::core::transport {

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
    size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return descriptor_.endpoint;
    return descriptor_.endpoint.substr(0, colon);
}

uint16_t UdpLink::port() const {
    size_t colon = descriptor_.endpoint.find(':');
    if (colon == std::string::npos) return 0;
    try {
        return static_cast<uint16_t>(std::stoi(descriptor_.endpoint.substr(colon + 1)));
    } catch (const std::exception& e) {
        MGC_LOG_ERROR("[UdpLink] Invalid port: %s", e.what());
        return 0;
    }
}

bool UdpLink::is_open() const {
    return opened_.load();
}

void UdpLink::set_receive_callback(ReceiveCallback cb) {
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
            MGC_LOG_ERROR("[UdpLink] Failed to create read thread: %lu", static_cast<unsigned long>(GetLastError()));
            return;
        }
        read_thread_ = h;
#else
        pthread_t tid = 0;
        if (pthread_create(&tid, nullptr, read_thread_proc, this) != 0) {
            MGC_LOG_ERROR("[UdpLink] Failed to create read thread");
            return;
        }
        read_thread_ = tid;
#endif
    }
}

// ================================================================
// Open (bind to local port)
// ================================================================

bool UdpLink::open() {
    if (opened_.load()) return true;

    if (!mg::core::platform::socket_init()) {
        MGC_LOG_ERROR("[UdpLink] Winsock init failed");
        return false;
    }

    std::string h = host();
    uint16_t p = port();
    if (p == 0) {
        MGC_LOG_ERROR("[UdpLink] Invalid port in endpoint: %s", descriptor_.endpoint.c_str());
        return false;
    }

    // Create UDP socket
    sock_ = static_cast<mg_socket_t>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (sock_ == MG_INVALID_SOCKET) {
        MGC_LOG_ERROR("[UdpLink] Socket creation failed: %d", mg::core::platform::socket_error());
        return false;
    }

    // Allow address reuse
    int reuse = 1;
    if (setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) != 0) {
        MGC_LOG_WARN("[UdpLink] setsockopt SO_REUSEADDR failed: %d", mg::core::platform::socket_error());
    }

    // Bind to local address
    struct sockaddr_in bind_addr{};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(p);
    if (h.empty() || h == "0.0.0.0") {
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, h.c_str(), &bind_addr.sin_addr) != 1) {
            MGC_LOG_ERROR("[UdpLink] inet_pton failed for %s", h.c_str());
            mg::core::platform::socket_close(sock_);
            sock_ = MG_INVALID_SOCKET;
            return false;
        }
    }

    if (::bind(sock_, reinterpret_cast<struct sockaddr*>(&bind_addr), sizeof(bind_addr)) == MG_SOCKET_ERR) {
        MGC_LOG_ERROR("[UdpLink] Bind to %s:%u failed: %d", h.c_str(), p, mg::core::platform::socket_error());
        mg::core::platform::socket_close(sock_);
        sock_ = MG_INVALID_SOCKET;
        return false;
    }

    // Set socket to non-blocking for reliable shutdown
#ifdef _WIN32
    u_long nonblock = 1;
    ioctlsocket(sock_, FIONBIO, &nonblock);
#else
    int flags = fcntl(sock_, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
    }
#endif

    stop_requested_.store(false);
    opened_.store(true);

    // Start read thread (only if callback already set ? e.g. from wire_callbacks before open)
    if (receive_cb_) {
#ifdef _WIN32
        HANDLE h = CreateThread(nullptr, 0, read_thread_proc, this, 0, nullptr);
        if (!h) {
            MGC_LOG_ERROR("[UdpLink] Failed to create read thread: %lu", static_cast<unsigned long>(GetLastError()));
            mg::core::platform::socket_close(sock_);
            sock_ = MG_INVALID_SOCKET;
            opened_.store(false);
            return false;
        }
        read_thread_ = h;
#else
        pthread_t tid = 0;
        if (pthread_create(&tid, nullptr, read_thread_proc, this) != 0) {
            MGC_LOG_ERROR("[UdpLink] Failed to create read thread");
            mg::core::platform::socket_close(sock_);
            sock_ = MG_INVALID_SOCKET;
            opened_.store(false);
            return false;
        }
        read_thread_ = tid;
#endif
    }

    MGC_LOG_INFO("[UdpLink] Bound to %s:%u", h.c_str(), p);
    return true;
}

// ================================================================
// Join read thread ? safe to call from any thread
// ================================================================

void UdpLink::join_read_thread() {
#ifdef _WIN32
    if (!read_thread_) return;
    if (sock_ != MG_INVALID_SOCKET) {
        ::shutdown(sock_, SD_BOTH);
    }
    DWORD wait_ret = WaitForSingleObject(read_thread_, 5000);
    if (wait_ret == WAIT_TIMEOUT) {
        MGC_LOG_WARN("[UdpLink] Read thread did not exit within 5s");
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
        MGC_LOG_WARN("[UdpLink] Read thread join failed (%d)", ret);
    }
    read_thread_ = 0;
#endif
}

// ================================================================
// Close
// ================================================================

void UdpLink::close() {
#ifdef _WIN32
    if (!opened_.load() && read_thread_ == nullptr && sock_ == MG_INVALID_SOCKET) return;
#else
    if (!opened_.load() && read_thread_ == 0 && sock_ == MG_INVALID_SOCKET) return;
#endif

    stop_requested_.store(true);
    set_receive_callback(nullptr);

    // Interrupt blocked recvfrom() and wait for thread exit
    join_read_thread();

    if (sock_ != MG_INVALID_SOCKET) {
        mg::core::platform::socket_close(sock_);
        sock_ = MG_INVALID_SOCKET;
    }

    mg::core::platform::socket_cleanup();
    opened_.store(false);
    has_remote_.store(false);
    MGC_LOG_INFO("[UdpLink] Closed");
}

// ================================================================
// Send (to learned remote or specified address)
// ================================================================

bool UdpLink::send(const uint8_t* data, size_t size) {
    if (!opened_.load() || sock_ == MG_INVALID_SOCKET) return false;
    if (!has_remote_.load()) return false;

    struct sockaddr_in remote_addr{};
    {
        mg::core::platform::LockGuard lk(remote_mtx_);
        remote_addr = remote_addr_;
    }

    auto sent = ::sendto(sock_, reinterpret_cast<const char*>(data),
                         static_cast<int>(size), 0,
                         reinterpret_cast<struct sockaddr*>(&remote_addr),
                         sizeof(remote_addr));
    return sent != MG_SOCKET_ERR;
}

// ================================================================
// Set remote address
// ================================================================

void UdpLink::set_remote_address(const std::string& host, uint16_t port) {
    mg::core::platform::LockGuard lk(remote_mtx_);
    remote_addr_.sin_family = AF_INET;
    remote_addr_.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &remote_addr_.sin_addr);
    has_remote_.store(true);
}

// ================================================================
// Read thread
// ================================================================

#ifdef _WIN32
DWORD WINAPI UdpLink::read_thread_proc(LPVOID lpParam) {
#else
void* UdpLink::read_thread_proc(void* lpParam) {
#endif
    auto* self = static_cast<UdpLink*>(lpParam);
    uint8_t buffer[4096];

    while (!self->stop_requested_.load()) {
        struct sockaddr_in sender_addr{};
#ifdef _WIN32
        int sender_len = sizeof(sender_addr);
#else
        socklen_t sender_len = sizeof(sender_addr);
#endif

        auto received = ::recvfrom(self->sock_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                   reinterpret_cast<struct sockaddr*>(&sender_addr), &sender_len);

        if (received > 0) {
            // Learn remote address from first incoming packet
            if (!self->has_remote_.load()) {
                char addr_str[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &sender_addr.sin_addr, addr_str, sizeof(addr_str));
                self->set_remote_address(addr_str, ntohs(sender_addr.sin_port));
                MGC_LOG_INFO("[UdpLink] Learned remote: %s:%u", addr_str, ntohs(sender_addr.sin_port));
            }

            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(self->cb_mtx_);
                cb_copy = self->receive_cb_;
            }
            if (cb_copy) {
                cb_copy(buffer, static_cast<size_t>(received));
            }
        } else if (received == MG_SOCKET_ERR) {
            int err = mg::core::platform::socket_error();
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
            if (!self->stop_requested_.load()) {
                MGC_LOG_ERROR("[UdpLink] Recvfrom error: %d", err);
                self->opened_.store(false);
            }
            break;
        }
    }

    if (!self->stop_requested_.load()) {
        self->opened_.store(false);
    }

#ifdef _WIN32
    return 0;
#else
    return nullptr;
#endif
}

} // namespace mg::core::transport
