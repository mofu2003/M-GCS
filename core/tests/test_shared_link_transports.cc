/**
 * test_shared_link_transports.cc - local shared-link transport validation.
 *
 * Uses loopback TCP and a POSIX pseudo-terminal. No physical flight
 * controller, serial USB device, or user SITL/UDP port is touched.
 */
#include "core/vehicle/multi_vehicle_manager.h"
#include "core/transport/link_config.h"
#include "core/platform/platform_socket.h"
#include "ardupilotmega/mavlink.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

int g_passed = 0;
int g_failed = 0;

#define CHECK(condition, message) do { \
    if (condition) { ++g_passed; fprintf(stderr, "  [PASS] %s\n", message); } \
    else { ++g_failed; fprintf(stderr, "  [FAIL] %s\n", message); } \
} while (0)

std::vector<uint8_t> pack_heartbeat(uint8_t system_id) {
    mavlink_message_t message{};
    mavlink_msg_heartbeat_pack(
        system_id, 1, &message, MAV_TYPE_QUADROTOR,
        MAV_AUTOPILOT_ARDUPILOTMEGA, MAV_MODE_FLAG_CUSTOM_MODE_ENABLED,
        0, MAV_STATE_ACTIVE);
    uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
    const uint16_t length = mavlink_msg_to_send_buffer(buffer, &message);
    return std::vector<uint8_t>(buffer, buffer + length);
}

bool wait_for_devices(const mg::core::vehicle::MultiVehicleManager& manager,
                      size_t expected,
                      int timeout_ms = 4000) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (manager.vehicle_count() >= expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return manager.vehicle_count() >= expected;
}

#ifndef _WIN32
enum class IoResult { Ok, Retry, Closed };

void drain_fd(int fd) {
    uint8_t buffer[1024];
    while (::read(fd, buffer, sizeof(buffer)) > 0) {
    }
}

IoResult write_all_fd(int fd, const uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = ::write(fd, data + offset, size - offset);
        if (written > 0) {
            offset += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        return written < 0 && errno == EAGAIN ? IoResult::Retry : IoResult::Closed;
    }
    return IoResult::Ok;
}

IoResult send_all_socket(mg_socket_t socket, const uint8_t* data, size_t size) {
    size_t offset = 0;
    while (offset < size) {
#ifdef MSG_NOSIGNAL
        const ssize_t written =
            ::send(socket, data + offset, size - offset, MSG_NOSIGNAL);
#else
        const ssize_t written = ::send(socket, data + offset, size - offset, 0);
#endif
        if (written > 0) {
            offset += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }
        return written < 0 && errno == EAGAIN ? IoResult::Retry : IoResult::Closed;
    }
    return IoResult::Ok;
}

bool create_tcp_listener(mg_socket_t* listener, uint16_t* port) {
    if (!mg::core::platform::socket_init()) return false;
    const mg_socket_t socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == MG_INVALID_SOCKET) return false;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
            MG_SOCKET_ERR ||
        ::listen(socket, 1) == MG_SOCKET_ERR) {
        mg::core::platform::socket_close(socket);
        return false;
    }

    socklen_t size = sizeof(address);
    if (getsockname(socket, reinterpret_cast<sockaddr*>(&address), &size) ==
        MG_SOCKET_ERR) {
        mg::core::platform::socket_close(socket);
        return false;
    }
    *listener = socket;
    *port = ntohs(address.sin_port);
    return true;
}

bool create_pty(int* master, std::string* slave_path) {
    const int fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return false;
    if (grantpt(fd) != 0 || unlockpt(fd) != 0) {
        ::close(fd);
        return false;
    }
    char* name = ptsname(fd);
    if (!name || name[0] == '\0') {
        ::close(fd);
        return false;
    }
    *master = fd;
    *slave_path = name;
    return true;
}

void test_shared_tcp_transport() {
    fprintf(stderr, "\n[Group] Shared TCP transport\n");
    mg_socket_t listener = MG_INVALID_SOCKET;
    uint16_t port = 0;
    CHECK(create_tcp_listener(&listener, &port), "loopback TCP listener opens");

    std::atomic<bool> stop{false};
    std::atomic<bool> accepted{false};
    std::thread peer;
    if (listener != MG_INVALID_SOCKET) {
        peer = std::thread([&]() {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(listener, &read_set);
            timeval timeout{2, 0};
            if (select(listener + 1, &read_set, nullptr, nullptr, &timeout) <= 0) {
                return;
            }
            sockaddr_in remote{};
            socklen_t remote_size = sizeof(remote);
            const mg_socket_t client = accept(
                listener, reinterpret_cast<sockaddr*>(&remote), &remote_size);
            if (client == MG_INVALID_SOCKET) return;
            accepted.store(true, std::memory_order_release);
#ifdef SO_NOSIGPIPE
            int enabled = 1;
            setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &enabled,
                       sizeof(enabled));
#endif
            const auto first = pack_heartbeat(61);
            const auto second = pack_heartbeat(62);
            while (!stop.load(std::memory_order_acquire)) {
                if (send_all_socket(client, first.data(), first.size()) ==
                        IoResult::Closed ||
                    send_all_socket(client, second.data(), second.size()) ==
                        IoResult::Closed) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            mg::core::platform::socket_close(client);
        });
    }

    mg::core::vehicle::MultiVehicleManager manager;
    mg::core::transport::LinkConfig config;
    config.type = mg::core::transport::LinkType::TCP;
    config.endpoint = "127.0.0.1:" + std::to_string(port);
    const int link_id = manager.open_shared_link(config);
    CHECK(link_id > 0, "shared TCP link opens");

    const bool discovered = wait_for_devices(manager, 2);
    CHECK(accepted.load(std::memory_order_acquire),
          "TCP peer accepted the shared connection");
    CHECK(discovered, "TCP link discovers two sysids");
    CHECK(manager.get_vehicle_by_system_id(61) != nullptr &&
              manager.get_vehicle_by_system_id(62) != nullptr,
          "TCP sysids map to distinct vehicles");
    CHECK(manager.managed_links().size() == 2,
          "TCP route map exposes both vehicles");

    stop.store(true, std::memory_order_release);
    if (peer.joinable()) peer.join();
    if (link_id > 0) manager.close_link(link_id);
    if (listener != MG_INVALID_SOCKET) {
        mg::core::platform::socket_close(listener);
    }
    mg::core::platform::socket_cleanup();
}

void test_shared_serial_transport() {
    fprintf(stderr, "\n[Group] Shared serial PTY transport\n");
    int master = -1;
    std::string slave_path;
    const bool pty_ready = create_pty(&master, &slave_path);
    CHECK(pty_ready, "POSIX PTY opens");
    if (!pty_ready) return;

    std::atomic<bool> stop{false};
    mg::core::vehicle::MultiVehicleManager manager;
    mg::core::transport::LinkConfig config;
    config.type = mg::core::transport::LinkType::Serial;
    config.endpoint = slave_path;
    config.baud_rate = 115200;
    const int link_id = manager.open_shared_link(config);
    CHECK(link_id > 0, "shared serial link opens on PTY");

    std::thread peer([&]() {
        const auto first = pack_heartbeat(71);
        const auto second = pack_heartbeat(72);
        while (!stop.load(std::memory_order_acquire)) {
            drain_fd(master);
            const auto first_result =
                write_all_fd(master, first.data(), first.size());
            const auto second_result =
                write_all_fd(master, second.data(), second.size());
            if (first_result == IoResult::Closed ||
                second_result == IoResult::Closed) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    const bool discovered = wait_for_devices(manager, 2);
    CHECK(discovered, "serial link discovers two sysids");
    CHECK(manager.get_vehicle_by_system_id(71) != nullptr &&
              manager.get_vehicle_by_system_id(72) != nullptr,
          "serial sysids map to distinct vehicles");
    CHECK(manager.managed_links().size() == 2,
          "serial route map exposes both vehicles");

    if (link_id > 0) manager.close_link(link_id);
    stop.store(true, std::memory_order_release);
    peer.join();
    ::close(master);
}
#else
void test_shared_tcp_transport() {
    fprintf(stderr, "\n[Group] Shared TCP transport\n");
    fprintf(stderr, "  [SKIP] POSIX socket test not configured on Windows\n");
}

void test_shared_serial_transport() {
    fprintf(stderr, "\n[Group] Shared serial PTY transport\n");
    fprintf(stderr, "  [SKIP] POSIX PTY test not configured on Windows\n");
}
#endif

} // namespace

int main() {
    fprintf(stderr, "=== Shared Link Transport Tests ===\n");
    test_shared_tcp_transport();
    test_shared_serial_transport();
    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n",
            g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
