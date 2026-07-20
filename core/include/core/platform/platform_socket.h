/**
 * platform_socket.h - 跨平台 socket 类型定义和辅助函数
 *
 * Provides socket types, init/cleanup, and error helpers for TCP/UDP transport.
 * Include this instead of directly including winsock2.h / sys/socket.h.
 */
#pragma once

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET mg_socket_t;
  constexpr mg_socket_t MG_INVALID_SOCKET = INVALID_SOCKET;
  constexpr int MG_SOCKET_ERR = SOCKET_ERROR;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int mg_socket_t;
  constexpr mg_socket_t MG_INVALID_SOCKET = -1;
  constexpr int MG_SOCKET_ERR = -1;
#endif

namespace mg::core::platform {

/// 初始化 socket 子系统（Windows 上调用 WSAStartup，POSIX 上为空操作），成功返回 true
inline bool socket_init() {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

/// 清理 socket 子系统
inline void socket_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/// 获取最近一次 socket 错误码
inline int socket_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

/// 关闭 socket 句柄
inline void socket_close(mg_socket_t sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    ::close(sock);
#endif
}

/// 关闭 socket 的双向通信
inline void socket_shutdown(mg_socket_t sock) {
#ifdef _WIN32
    shutdown(sock, SD_BOTH);
#else
    ::shutdown(sock, SHUT_RDWR);
#endif
}

} // namespace mg::core::platform