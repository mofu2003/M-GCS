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
#include "core/transport/connection_manager.h"
#include "core/transport/in_memory_link.h"
#include "core/mavlink/mavlink_adapter.h"
#include "core/platform/platform_socket.h"
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include "mgcs_test_check.h"

// 断言计数由共享头 mgcs_test_check.h 累加；这里沿用本文件原有的计数器名字，
// 汇总行与退出码保持原样。
static int& g_pass = mgcs_test::passed();
static int& g_fail = mgcs_test::failed();

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

// USBBoardInfo.json 的 boardDescriptionFallback 是一张按端口描述匹配的兜底表。
// 手写扫描器曾用 find(']') 找数组结尾，遇到正则里的 ']'（"^fmuv[2345]$"）就提前
// 收尾，只读到 35/51 条，静默丢弃后面 16 条板卡（ARK 系列、TROPIC、
// mRoControlZeroF7、FT231X 等）。这里用那些被丢弃条目才能匹配的描述做行为断言。
static void test_board_info_fallback_table_is_loaded_completely() {
    fprintf(stderr, "\n[Group] USBBoardInfo description fallbacks are all loaded\n");

    SerialPortDiscovery discovery;
    std::string board_class;
    std::string board_name;

    // 旧实现丢掉的条目（索引 43）：ARK BL FMU v6S。
    SerialPortInfo ark;
    ark.port_name = "/dev/ttyACM0";
    ark.description = "ARK BL FMU v6S";
    board_class.clear();
    board_name.clear();
    CHECK(discovery.get_board_info(ark, board_class, board_name),
          "a description from the tail of the fallback table still matches");
    CHECK(board_class == "Pixhawk",
          "ARK BL FMU v6S resolves to Pixhawk");

    // 旧实现丢掉的条目（索引 49）：^FT231X USB UART$。
    SerialPortInfo ft231x;
    ft231x.port_name = "/dev/ttyUSB0";
    ft231x.description = "FT231X USB UART";
    board_class.clear();
    board_name.clear();
    CHECK(discovery.get_board_info(ft231x, board_class, board_name),
          "FT231X USB UART falls back to the SiK Radio class");
    CHECK(board_class == "SiK Radio", "FT231X USB UART resolves to SiK Radio");

#ifndef __ANDROID__
    // 索引 50 的 `USB UART$` 带 androidOnly=true，注释写明
    // "Very broad fallback, too dangerous for non-android"。修好截断后它会被载入，
    // 但在这条路径上必须被跳过——否则桌面端会把任何含 "USB UART" 的端口都当成 SiK。
    SerialPortInfo generic;
    generic.port_name = "/dev/ttyUSB1";
    generic.description = "USB UART";
    board_class.clear();
    board_name.clear();
    CHECK(!discovery.get_board_info(generic, board_class, board_name),
          "an androidOnly fallback is not applied on desktop");
#endif
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

    CHECK(!link.is_open(), "failed reconnect leaves the link closed");
}

/// KD-25：不可达地址上的 connect 必须有界返回，不能挂到操作系统超时
/// （默认可达 75 s，而看门狗要等连接返回后才启动）。
static void test_tcp_connect_timeout_is_bounded() {
    fprintf(stderr, "\n[Group] TCP connect timeout\n");

    LinkDescriptor desc;
    // 192.0.2.0/24 是 TEST-NET-1，保留给文档使用，正常不会被路由到。
    desc.id = "tcp://192.0.2.1:9";
    desc.type = "tcp";
    desc.endpoint = "192.0.2.1:9";
    TcpLink link(desc);

    const auto start = std::chrono::steady_clock::now();
    const bool opened = link.open();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    CHECK(!opened, "connect to an unroutable address fails");
    CHECK(elapsed < 6000,
          "connect failure returns within the bounded timeout (not the OS default)");
    link.close();
    CHECK(!link.is_open(), "a failed connect leaves the link closed");
}

/// 并发缺陷 1 的所有权语义：`shared_mavlink()` 交出的是持有生命周期的句柄。
/// 断链（reset()）会从 ConnectionManager 摘掉适配器并在锁外析构它；调用方只要
/// 还握着 shared_mavlink() 的返回副本，适配器就必须继续存活 —— 这正是
/// ffi_cmd_result_queued() 等"最长阻塞 5 秒等 ACK"路径赖以不悬垂的语义。
/// 旧代码只有不带所有权的适配器访问，无法表达该保证。
static void test_connection_manager_adapter_lease_survives_reset() {
    fprintf(stderr, "\n[Group] ConnectionManager adapter lease\n");

    ConnectionManager conn;
    auto link = std::make_unique<InMemoryLink>();
    link->open();
    auto mav = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    auto* raw_mav = mav.get();
    auto* raw_link = link.get();
    mav->set_send_callback([raw_link](const uint8_t* data, size_t size) {
        return raw_link->send(data, size);
    });
    conn.wire_for_test(std::move(link), std::move(mav));

    auto lease = conn.shared_mavlink();
    CHECK(lease != nullptr, "shared_mavlink() hands out an owning handle");
    CHECK(lease.get() == raw_mav, "the lease is the adapter wired to the manager");
    // 断链：reset() 在锁内 move 出 shared_ptr、在锁外析构。
    conn.reset();
    CHECK(conn.shared_mavlink() == nullptr, "reset() detaches the adapter from the manager");
    CHECK(lease.get() == raw_mav, "the caller's lease keeps the adapter object alive");

    // 触碰已断链的适配器：有租约就必须安全（ASan 下不得报 use-after-free）。
    (void)lease->latest_heartbeat();
    (void)lease->send_heartbeat();
    CHECK(lease != nullptr, "using the leased adapter after reset() is safe");

    // 同一语义可重复拿取：恢复接线后新的租约指向新适配器，旧租约仍持有旧适配器。
    auto link2 = std::make_unique<InMemoryLink>();
    link2->open();
    auto mav2 = std::make_unique<mg::core::mavlink::MavlinkAdapter>();
    auto* raw_mav2 = mav2.get();
    conn.wire_for_test(std::move(link2), std::move(mav2));
    auto lease2 = conn.shared_mavlink();
    CHECK(lease2.get() == raw_mav2 && lease2.get() != raw_mav,
          "a re-wired manager hands out a lease on the new adapter");
    CHECK(lease.get() == raw_mav, "the old lease still owns the old adapter");
}

/// 把源码里的注释与字符串字面量替换成空格（保留换行，位置不变）。
///
/// 守卫要在源码上做花括号配对，先屏蔽注释/字符串就不会把说明文字里的 `{`、`}`、
/// 引号当成代码。返回的字符串与原串**等长**，所以下标可以直接混用。
static std::string mask_comments_and_literals(const std::string& text) {
    std::string out = text;
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '/') {
            while (i < out.size() && out[i] != '\n') out[i++] = ' ';
            continue;
        }
        if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '*') {
            out[i++] = ' ';
            out[i++] = ' ';
            while (i + 1 < out.size() && !(out[i] == '*' && out[i + 1] == '/')) {
                if (out[i] != '\n') out[i] = ' ';
                ++i;
            }
            if (i + 1 < out.size()) {
                out[i] = ' ';
                out[i + 1] = ' ';
                ++i;
            }
            continue;
        }
        if (out[i] == '"' || out[i] == '\'') {
            const char quote = out[i];
            out[i] = ' ';
            ++i;
            while (i < out.size() && out[i] != quote) {
                if (out[i] == '\\' && i + 1 < out.size()) {
                    out[i] = ' ';
                    out[i + 1] = ' ';
                    i += 2;
                    continue;
                }
                if (out[i] != '\n') out[i] = ' ';
                ++i;
            }
            if (i < out.size()) out[i] = ' ';
        }
    }
    return out;
}

/// `masked[open]` 是 `{`，返回配对 `}` 的下标；找不到返回 npos。
static size_t matching_brace(const std::string& masked, size_t open) {
    if (open >= masked.size() || masked[open] != '{') return std::string::npos;
    int depth = 0;
    for (size_t i = open; i < masked.size(); ++i) {
        if (masked[i] == '{') ++depth;
        else if (masked[i] == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

/// 返回包含 `index` 的最内层 `{` 的下标（用于求 RAII 锁所在的作用域）。
static size_t enclosing_open_brace(const std::string& masked, size_t index) {
    int depth = 0;
    for (size_t i = index; i > 0; --i) {
        const char c = masked[i - 1];
        if (c == '}') {
            ++depth;
        } else if (c == '{') {
            if (depth == 0) return i - 1;
            --depth;
        }
    }
    return std::string::npos;
}

/// 在 `masked[from, to)` 里找回调调用，返回它的起下标（找不到返回 npos）。
///
/// "回调"按命名习惯识别：标识符含 `callback`，或以 `cb`/`cb_`/`_cb` 结尾。
/// 只看代码（注释与字符串已屏蔽），且只认后面跟 `(` 的标识符，所以
/// `callback = link->receive_cb_;` 这种**拷贝**不会被当成调用。
static size_t find_callback_invocation(const std::string& masked,
                                       size_t from, size_t to) {
    for (size_t i = from; i < to && i < masked.size(); ++i) {
        const char c = masked[i];
        if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) continue;
        size_t end = i;
        while (end < masked.size() &&
               (std::isalnum(static_cast<unsigned char>(masked[end])) || masked[end] == '_')) {
            ++end;
        }
        const std::string name = masked.substr(i, end - i);
        size_t next = end;
        while (next < masked.size() && (masked[next] == ' ' || masked[next] == '\t')) ++next;
        const bool is_call = next < masked.size() && masked[next] == '(';
        const bool looks_like_callback =
            name.find("callback") != std::string::npos ||
            (name.size() >= 2 && name.compare(name.size() - 2, 2, "cb") == 0) ||
            (name.size() >= 3 && name.compare(name.size() - 3, 3, "cb_") == 0) ||
            (name.size() >= 3 && name.compare(name.size() - 3, 3, "_cb") == 0);
        if (is_call && looks_like_callback) return i;
        i = end - 1;
    }
    return std::string::npos;
}

/// 并发缺陷 2：`AndroidUsbSerialLink::on_java_new_data` 不得在持有
/// g_registry_mutex 期间调用接收回调（AGENTS.md：core 持锁路径不得调用外部回调；
/// 且该锁同时保护注册表，回调里若触发 on_java_disconnected 会自死锁）。
///
/// 该文件整体在 `#ifdef __ANDROID__` 内，本机（桌面/CI）既不能编译也不能运行，
/// 所以按 test_bridge_architecture_guards.cc 的既有手段做源码级守卫。
///
/// 判据**不依赖任何注释措辞**：先屏蔽注释与字符串，再按花括号配对求出
/// `g_registry_mutex` 那次加锁的**词法作用域**（RAII 锁在作用域结束时析构；
/// 若实现改成显式 lock()/unlock()，则按两者之间求作用域），然后断言：
///   1) 作用域内没有任何回调调用；
///   2) 回调调用出现在作用域结束之后；
///   3) 作用域内确实只做了"查表 + 拷贝"。
/// 改注释、加日志、调整语句顺序都不会让它变红；只有把回调挪回锁内（真正的缺陷）
/// 才会。旧判据依赖 `// g_registry_mutex released here` 这个标记，改一下注释措辞
/// 就会误报红——这正是本用例要修掉的问题。
static void test_android_new_data_calls_callback_outside_registry_lock() {
    fprintf(stderr, "\n[Group] Android JNI callback lock discipline\n");

    const auto path = std::filesystem::path(MGCORE_TEST_ROOT) /
        "core/src/transport/android_usb_serial_link.cc";
    std::ifstream in(path);
    if (!in.is_open()) {
        g_fail++;
        fprintf(stderr, "  [FAIL] cannot open %s (guard would be vacuous)\n",
                path.string().c_str());
        return;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();
    CHECK(!text.empty(), "android link source was read");
    if (text.empty()) return;
    const std::string masked = mask_comments_and_literals(text);

    const std::string function = "void AndroidUsbSerialLink::on_java_new_data(";
    const std::string lock_identifier = "g_registry_mutex";

    const auto function_pos = masked.find(function);
    CHECK(function_pos != std::string::npos,
          "on_java_new_data is defined in the android link");
    if (function_pos == std::string::npos) return;
    const auto body_open = masked.find('{', function_pos);
    CHECK(body_open != std::string::npos && body_open > function_pos,
          "the function body has an opening brace");
    if (body_open == std::string::npos) return;
    const auto body_close = matching_brace(masked, body_open);
    CHECK(body_close != std::string::npos,
          "the function body braces are balanced");
    if (body_close == std::string::npos) return;

    // 找到注册表锁：优先 RAII 声明，其次显式 lock()/unlock()。
    const auto lock_pos = masked.find(lock_identifier, body_open);
    CHECK(lock_pos != std::string::npos && lock_pos < body_close,
          "the function takes the registry lock");
    if (lock_pos == std::string::npos || lock_pos >= body_close) return;

    // 语句范围：从上一个分隔符到下一个 ';'。
    const auto statement_start = masked.find_last_of(";{}", lock_pos);
    const auto statement_end = masked.find(';', lock_pos);
    CHECK(statement_end != std::string::npos && statement_end < body_close,
          "the registry lock statement is terminated inside the function");
    if (statement_end == std::string::npos || statement_end >= body_close) return;
    const std::string lock_statement = masked.substr(
        statement_start == std::string::npos ? body_open : statement_start,
        statement_end - (statement_start == std::string::npos ? body_open : statement_start));

    const bool explicit_lock = lock_statement.find(".lock()") != std::string::npos;
    size_t scope_begin = std::string::npos;
    size_t scope_end_inclusive = std::string::npos;
    if (explicit_lock) {
        const auto unlock_pos = masked.find(lock_identifier + ".unlock()", statement_end);
        CHECK(unlock_pos != std::string::npos && unlock_pos < body_close,
              "an explicit unlock() matches the explicit lock()");
        if (unlock_pos == std::string::npos || unlock_pos >= body_close) return;
        scope_begin = statement_end;
        scope_end_inclusive = unlock_pos;
    } else {
        const auto block_open = enclosing_open_brace(masked, lock_pos);
        CHECK(block_open != std::string::npos && block_open < lock_pos,
              "the RAII lock lives inside a brace scope");
        if (block_open == std::string::npos || block_open >= lock_pos) return;
        const auto block_close = matching_brace(masked, block_open);
        CHECK(block_close != std::string::npos && block_close < body_close,
              "the RAII lock scope closes before the function ends");
        if (block_close == std::string::npos || block_close >= body_close) return;
        scope_begin = block_open + 1;
        scope_end_inclusive = block_close;
    }

    const std::string locked_scope =
        masked.substr(scope_begin, scope_end_inclusive - scope_begin);
    CHECK(locked_scope.find(lock_identifier) != std::string::npos ||
              explicit_lock,
          "the guarded scope really contains the registry lock");
    CHECK(find_callback_invocation(masked, scope_begin, scope_end_inclusive) ==
              std::string::npos,
          "no callback invocation inside the g_registry_mutex scope");
    CHECK(locked_scope.find("receive_cb_") != std::string::npos,
          "the callback is copied under the lock (lookup + copy only)");

    const auto call_pos = find_callback_invocation(masked, body_open, body_close);
    CHECK(call_pos != std::string::npos,
          "the receive callback is invoked in the same function");
    if (call_pos == std::string::npos) return;
    CHECK(call_pos > scope_end_inclusive,
          "the receive callback runs after the registry lock is released");
}

int main() {
    fprintf(stderr, "=== Transport Link Tests ===\n");

    test_tcp_link();
    test_tcp_connect_timeout_is_bounded();
    test_udp_link();
    test_udp_link_close_waits_for_read_thread();
    test_serial_link();
    test_windows_serial_configuration();
    test_board_info_fallback_table_is_loaded_completely();
    test_tcp_reconnect();
    test_connection_manager_adapter_lease_survives_reset();
    test_android_new_data_calls_callback_outside_registry_lock();

    fprintf(stderr, "\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail;
}
