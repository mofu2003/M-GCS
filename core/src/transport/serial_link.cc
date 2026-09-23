/**
 * serial_link.cc - Win32 串口通信实现
 *
 * Opens a COM port, configures baud rate/8N1, and runs a background
 * read thread that dispatches received bytes to the registered callback.
 *
 * Thread safety:
 *   - opened_ and stop_requested_ are std::atomic<bool>.
 *   - close() calls join_read_thread() which uses CancelIoEx to interrupt
 *     any blocked ReadFile, then waits for the thread to exit before
 *     releasing the COM handle. This eliminates the destructor race where
 *     a blocked read thread could access freed memory.
 *   - cb_mtx_ protects receive_cb_ while close() clears it.
 */

#include "core/transport/serial_link.h"
#include "core/log/logging.h"

#ifndef _WIN32
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#endif

namespace mg::core::transport {

#ifndef _WIN32
namespace {
speed_t to_termios_baud(uint32_t baud_rate) {
    switch (baud_rate) {
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return B115200;
    }
}
} // namespace
#endif

SerialLink::SerialLink(const LinkDescriptor& descriptor)
    : descriptor_(descriptor) {}

SerialLink::~SerialLink() {
    close();
}

// 打开串口，配置波特率和 8N1，启动后台读取线程
bool SerialLink::open() {
    if (opened_.load()) return true;

#ifdef _WIN32
    std::string port_path = "\\\\.\\" + descriptor_.endpoint;

    handle_ = CreateFileA(
        port_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (handle_ == INVALID_HANDLE_VALUE) {
        MGC_LOG_ERROR("[SerialLink] Failed to open %s", descriptor_.endpoint.c_str());
        return false;
    }

    // 配置串口参数：波特率、8 数据位、1 停止位、无校验
    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(handle_, &dcb)) {
        MGC_LOG_ERROR("[SerialLink] GetCommState failed");
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = descriptor_.baud_rate;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;

    if (!SetCommState(handle_, &dcb)) {
        MGC_LOG_ERROR("[SerialLink] SetCommState failed");
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    // 设置非阻塞读取模式：ReadFile 立即返回可用数据
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 0;

    if (!SetCommTimeouts(handle_, &timeouts)) {
        MGC_LOG_ERROR("[SerialLink] SetCommTimeouts failed");
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    stop_requested_.store(false);
    opened_.store(true);

    // 启动后台读取线程
    if (receive_cb_) {
        read_thread_ = CreateThread(nullptr, 0, read_thread_proc, this, 0, nullptr);
    }

    MGC_LOG_INFO("[SerialLink] Opened %s @ %u baud", descriptor_.endpoint.c_str(), descriptor_.baud_rate);
    return true;
#else
    fd_ = ::open(descriptor_.endpoint.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        MGC_LOG_ERROR("[SerialLink] Failed to open %s", descriptor_.endpoint.c_str());
        return false;
    }

    termios tty = {};
    if (tcgetattr(fd_, &tty) != 0) {
        MGC_LOG_ERROR("[SerialLink] tcgetattr failed");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    cfmakeraw(&tty);
    const speed_t baud = to_termios_baud(descriptor_.baud_rate);
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);
    tty.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    tty.c_cflag &= static_cast<tcflag_t>(~PARENB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSTOPB);
    tty.c_cflag &= static_cast<tcflag_t>(~CSIZE);
    tty.c_cflag |= CS8;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
        MGC_LOG_ERROR("[SerialLink] tcsetattr failed");
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    stop_requested_.store(false);
    opened_.store(true);

    if (receive_cb_) {
        read_thread_ = std::thread(read_thread_proc, this);
    }

    MGC_LOG_INFO("[SerialLink] Opened %s @ %u baud", descriptor_.endpoint.c_str(), descriptor_.baud_rate);
    return true;
#endif
}

// 等待读取线程退出，使用 CancelIoEx 中断阻塞的读操作
void SerialLink::join_read_thread() {
#ifdef _WIN32
    if (!read_thread_) return;

    // 中断串口上所有阻塞的 I/O 操作
    // CancelIoEx cancels all pending I/O for this handle on this thread.
    // If the read thread is not blocked in I/O, this is a no-op.
    if (handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(handle_, nullptr);
    }

    // 等待读取线程退出（5 秒超时）
    if (WaitForSingleObject(read_thread_, 5000) == WAIT_TIMEOUT) {
        MGC_LOG_ERROR("[SerialLink] Read thread did not stop for %s", descriptor_.endpoint.c_str());
    }
    CloseHandle(read_thread_);
    read_thread_ = nullptr;
#else
    if (read_thread_.joinable()) {
        read_thread_.join();
    }
#endif
}

// 关闭串口，停止读取线程并释放句柄
void SerialLink::close() {
#ifdef _WIN32
    if (!opened_.load() && read_thread_ == nullptr && handle_ == INVALID_HANDLE_VALUE) return;
#else
    if (!opened_.load() && !read_thread_.joinable() && fd_ < 0) return;
#endif

    // 通知读取线程停止
    stop_requested_.store(true);
    set_receive_callback(nullptr);

    // 中断阻塞的 I/O 并等待线程退出
    join_read_thread();

#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif

    opened_.store(false);
    MGC_LOG_INFO("[SerialLink] Closed %s", descriptor_.endpoint.c_str());
}

bool SerialLink::is_open() const {
    return opened_.load();
}

// 向串口写入数据
bool SerialLink::send(const uint8_t* data, size_t size) {
    if (!opened_.load()) return false;

#ifdef _WIN32
    DWORD bytes_written = 0;
    BOOL result = WriteFile(handle_, data, static_cast<DWORD>(size), &bytes_written, nullptr);

    return result && bytes_written == size;
#else
    ssize_t bytes_written = ::write(fd_, data, size);
    return bytes_written == static_cast<ssize_t>(size);
#endif
}

// 设置接收回调，若链路已打开且无读取线程则启动
void SerialLink::set_receive_callback(ReceiveCallback cb) {
    const bool should_start = static_cast<bool>(cb);
    {
        mg::core::platform::LockGuard lk(cb_mtx_);
        receive_cb_ = std::move(cb);
    }

    // 若已打开且尚未启动读取线程，则现在启动
#ifdef _WIN32
    if (opened_.load() && should_start && !read_thread_) {
        read_thread_ = CreateThread(nullptr, 0, read_thread_proc, this, 0, nullptr);
    }
#else
    if (opened_.load() && should_start && !read_thread_.joinable()) {
        read_thread_ = std::thread(read_thread_proc, this);
    }
#endif
}

// 后台读取线程入口函数，循环读取串口数据并分发给回调
#ifdef _WIN32
DWORD WINAPI SerialLink::read_thread_proc(LPVOID lpParam) {
    auto* self = static_cast<SerialLink*>(lpParam);
    uint8_t buffer[4096];

    while (!self->stop_requested_.load()) {
        DWORD bytes_read = 0;
        BOOL result = ReadFile(self->handle_, buffer, sizeof(buffer), &bytes_read, nullptr);

        if (result && bytes_read > 0) {
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(self->cb_mtx_);  // 加锁保护回调
                cb_copy = self->receive_cb_;
            }
            if (cb_copy) cb_copy(buffer, bytes_read);
        } else if (!result) {
            // 读取出错或被 CancelIoEx 中断，检查是否应退出
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED || self->stop_requested_.load()) {
                break;
            }
            // 真实错误，也退出
            self->opened_.store(false);
            break;
        }
        // result==true 但 bytes_read==0 表示无数据可用（非阻塞模式）
        // 必须 sleep 以避免 CPU 空转，同时让 stop_requested_ 有机会被检查
        if (result && bytes_read == 0) {
            Sleep(10);
        }
    }

    if (!self->stop_requested_.load()) {
        self->opened_.store(false);
    }

    return 0;
}
#else
void SerialLink::read_thread_proc(SerialLink* self) {
    uint8_t buffer[4096];

    while (!self->stop_requested_.load()) {
        ssize_t bytes_read = ::read(self->fd_, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(self->cb_mtx_);
                cb_copy = self->receive_cb_;
            }
            if (cb_copy) cb_copy(buffer, static_cast<size_t>(bytes_read));
        } else if (bytes_read == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            // EAGAIN/EINTR are transient; other errors usually mean that a
            // USB-serial device was removed or the descriptor became invalid.
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            if (!self->stop_requested_.load()) {
                MGC_LOG_ERROR("[SerialLink] Read error: %d", errno);
            }
            break;
        }
    }

    if (!self->stop_requested_.load()) {
        self->opened_.store(false);
    }
}
#endif

} // namespace mg::core::transport
