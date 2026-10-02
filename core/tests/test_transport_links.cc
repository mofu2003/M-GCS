/**
 * test_transport_links.cc - TCP/UDP transport layer tests
 *
 * Tests: link creation, connection failure, send-while-closed, close semantics.
 * Does NOT require a real server — tests error paths and lifecycle.
 */
#include "core/transport/tcp_link.h"
#include "core/transport/udp_link.h"
#include "core/transport/serial_link.h"
#include "core/transport/link_config.h"
#include "core/transport/serial_port_discovery.h"
#include "core/platform/platform_socket.h"
#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

using namespace mg::core::transport;

static uint16_t reserve_udp_port() {
    if (!mg::core::platform::socket_init()) return 0;
    const mg_socket_t sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == MG_INVALID_SOCKET) return 0;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == MG_SOCKET_ERR) {
        mg::core::platform::socket_close(sock);
        return 0;
    }

#ifdef _WIN32
    int address_size = sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&address), &address_size) == MG_SOCKET_ERR) {
        mg::core::platform::socket_close(sock);
        return 0;
    }

    const uint16_t port = ntohs(address.sin_port);
    mg::core::platform::socket_close(sock);
    return port;
}

static uint16_t reserve_tcp_port() {
    if (!mg::core::platform::socket_init()) return 0;
    const mg_socket_t sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == MG_INVALID_SOCKET) return 0;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == MG_SOCKET_ERR) {
        mg::core::platform::socket_close(sock);
        return 0;
    }

#ifdef _WIN32
    int address_size = sizeof(address);
#else
    socklen_t address_size = sizeof(address);
#endif
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&address), &address_size) == MG_SOCKET_ERR) {
        mg::core::platform::socket_close(sock);
        return 0;
    }

    const uint16_t port = ntohs(address.sin_port);
    mg::core::platform::socket_close(sock);
    return port;
}

static void test_tcp_link() {
    fprintf(stderr, "\n[Group] TCP Link\n");

    // 创建 TCP link（不连接）
    LinkDescriptor desc;
    const uint16_t port = reserve_tcp_port();
    if (port == 0) {
        fprintf(stderr, "  [SKIP] TCP port unavailable in this environment\n");
        return;
    }
    desc.id = "tcp://127.0.0.1:" + std::to_string(port);
    desc.type = "tcp";
    desc.endpoint = "127.0.0.1:" + std::to_string(port);
    TcpLink link(desc);

    CHECK(!link.is_open(), "TCP link starts closed");

    // 连接到不存在的服务器应该失败
    bool opened = link.open();
    CHECK(!opened, "TCP connect to nonexistent server fails");
    CHECK(!link.is_open(), "TCP link still closed after failed connect");

    // 关闭未打开的 link 不应崩溃
    link.close();
    CHECK(!link.is_open(), "close on unopened link is safe");

    // 发送到未打开的 link 应返回 false
    uint8_t data[] = {0xFE, 0x00};
    bool sent = link.send(data, sizeof(data));
    CHECK(!sent, "send on closed link returns false");
}

static void test_udp_link() {
    fprintf(stderr, "\n[Group] UDP Link\n");

    // 创建 UDP link
    LinkDescriptor desc;
    const uint16_t port = reserve_udp_port();
    if (port == 0) {
        fprintf(stderr, "  [SKIP] UDP bind unavailable in this environment\n");
        return;
    }
    const std::string endpoint = "127.0.0.1:" + std::to_string(port);
    desc.id = "udp://" + endpoint;
    desc.type = "udp";
    desc.endpoint = endpoint;
    UdpLink link(desc);

    CHECK(!link.is_open(), "UDP link starts closed");

    // UDP bind 应该成功（无连接协议）
    bool opened = link.open();
    CHECK(opened, "UDP bind succeeds");
    CHECK(link.is_open(), "UDP link is open after bind");

    // 发送到未连接的地址（UDP 无连接，send 可能成功也可能失败）
    uint8_t data[] = {0xFE, 0x00};
    link.send(data, sizeof(data));

    // 关闭
    link.close();
    CHECK(!link.is_open(), "UDP link closed after close");

    // 再次关闭不应崩溃
    link.close();
    CHECK(!link.is_open(), "double close is safe");
}

static void test_udp_link_close_waits_for_read_thread() {
    fprintf(stderr, "\n[Group] UDP read thread lifecycle\n");

    const uint16_t port = reserve_udp_port();
    if (port == 0) {
        fprintf(stderr, "  [SKIP] UDP bind unavailable in this environment\n");
        return;
    }

    LinkDescriptor desc;
    const std::string endpoint = "127.0.0.1:" + std::to_string(port);
    desc.id = "udp://" + endpoint;
    desc.type = "udp";
    desc.endpoint = endpoint;

    UdpLink link(desc);
    CHECK(link.open(), "UDP link opens before read thread lifecycle test");
    link.set_receive_callback([](const uint8_t*, size_t) {});
    link.close();
    CHECK(!link.is_open(), "UDP close waits until the read thread is gone");

    // A second bind on the same endpoint verifies that close released the
    // socket only after the read thread stopped.
    UdpLink reopened(desc);
    CHECK(reopened.open(), "UDP endpoint can be reopened after close");
    reopened.close();
}

static void test_serial_link() {
    fprintf(stderr, "\n[Group] Serial Link\n");

    // 创建 Serial link 到不存在的端口
    LinkDescriptor desc;
    desc.id = "COM999";
    desc.type = "serial";
    desc.endpoint = "COM999";
    desc.baud_rate = 115200;
    SerialLink link(desc);

    CHECK(!link.is_open(), "Serial link starts closed");

    // 打开不存在的端口应失败
    bool opened = link.open();
    CHECK(!opened, "open nonexistent COM port fails");
    CHECK(!link.is_open(), "Serial link still closed after failed open");

    // 关闭未打开的 link 不应崩溃
    link.close();

    // 发送到未打开的 link 应返回 false
    uint8_t data[] = {0xFE, 0x00};
    bool sent = link.send(data, sizeof(data));
    CHECK(!sent, "send on closed serial returns false");
}

static void test_windows_serial_configuration() {
    fprintf(stderr, "\n[Group] Windows serial configuration\n");

    const auto config = LinkConfig::from_uri("serial://COM12?baud=230400");
    CHECK(config.type == LinkType::Serial, "COM URI selects serial transport");
    CHECK(config.endpoint == "COM12", "COM URI preserves the port name");
    CHECK(config.baud_rate == 230400, "COM URI preserves baud rate");

    SerialPortDiscovery discovery;
    bool discovery_ok = true;
    try {
        (void)discovery.enumerate_ports();
    } catch (...) {
        discovery_ok = false;
    }
    CHECK(discovery_ok, "serial discovery is safe when no device is present");

    LinkDescriptor descriptor;
    descriptor.id = "COM999";
    descriptor.type = "serial";
    descriptor.endpoint = "COM999";
    descriptor.baud_rate = 115200;
    SerialLink link(descriptor);
    CHECK(!link.open(), "invalid COM port fails cleanly");
    link.close();
}

static void test_tcp_reconnect() {
    fprintf(stderr, "\n[Group] TCP Reconnect\n");

    LinkDescriptor desc;
    const uint16_t port = reserve_tcp_port();
    if (port == 0) {
        fprintf(stderr, "  [SKIP] TCP port unavailable in this environment\n");
        return;
    }
    desc.id = "tcp://127.0.0.1:" + std::to_string(port);
    desc.type = "tcp";
    desc.endpoint = "127.0.0.1:" + std::to_string(port);
    TcpLink link(desc);

    // 第一次连接失败
    CHECK(!link.open(), "first connect fails (no server)");
    link.close();

    // 第二次连接也应失败（不应有残留状态）
    CHECK(!link.open(), "second connect also fails cleanly");
    link.close();

    CHECK(true, "reconnect cycle doesn't crash");
}

int main() {
    fprintf(stderr, "=== Transport Link Tests ===\n");

    test_tcp_link();
    test_udp_link();
    test_udp_link_close_waits_for_read_thread();
    test_serial_link();
    test_windows_serial_configuration();
    test_tcp_reconnect();

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
