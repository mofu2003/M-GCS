/**
 * test_transport_links.cc - TCP/UDP transport layer tests
 *
 * Tests: link creation, connection failure, send-while-closed, close semantics.
 * Does NOT require a real server — tests error paths and lifecycle.
 */
#include "core/transport/tcp_link.h"
#include "core/transport/udp_link.h"
#include "core/transport/serial_link.h"
#include <cstdio>
#include <cstring>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; fprintf(stderr, "  [PASS] %s\n", msg); } \
    else { g_fail++; fprintf(stderr, "  [FAIL] %s\n", msg); } \
} while(0)

using namespace mg::core::transport;

static void test_tcp_link() {
    fprintf(stderr, "\n[Group] TCP Link\n");

    // 创建 TCP link（不连接）
    LinkDescriptor desc;
    desc.id = "tcp://127.0.0.1:5760";
    desc.type = "tcp";
    desc.endpoint = "127.0.0.1:5760";
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
    desc.id = "udp://127.0.0.1:14550";
    desc.type = "udp";
    desc.endpoint = "127.0.0.1:14550";
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

static void test_tcp_reconnect() {
    fprintf(stderr, "\n[Group] TCP Reconnect\n");

    LinkDescriptor desc;
    desc.id = "tcp://127.0.0.1:19999";
    desc.type = "tcp";
    desc.endpoint = "127.0.0.1:19999";
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
    test_serial_link();
    test_tcp_reconnect();

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
