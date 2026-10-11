/**
 * serial_link.cc - Win32 串口通信实现
 *
 * Opens a COM port, configures baud rate/8N1, and runs a background
 * read thread that dispatches received bytes to the registered callback.
 *
 * Thread safety:
 *   - The read thread only touches SerialLink::Reader (a shared_ptr the thread
 *     owns), never SerialLink itself, so destroying the link cannot turn a
 *     delayed thread exit into a use-after-free (KD-24).
 *   - close() interrupts a blocked ReadFile with CancelIoEx and waits for the
 *     thread to finish before releasing the COM handle. If it cannot finish in
 *     time the port is abandoned: the handle is intentionally left open instead
 *     of using TerminateThread, which could kill the thread while it holds
 *     locks or handles.
 *   - Reader::cb_mtx protects Reader::receive_cb while close() clears it.
 */

#include "core/transport/serial_link.h"
#include "core/log/logging.h"

#include <algorithm>
#include <memory>

#ifndef _WIN32
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <poll.h>
#include <system_error>
#include <termios.h>
#include <thread>
#include <unistd.h>
#endif

namespace mg::core::transport {

#ifndef _WIN32
namespace {
/// 一次 send() 补齐短写的等待上限。send() 在 adapter 锁内被调用，不能无限等。
constexpr int kWriteTimeoutMs = 200;

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

/// 读线程自持的状态。线程与链路共享 shared_ptr：链路先消失时线程仍然安全，
/// 线程仍在运行时其状态也不会被释放。
struct SerialLink::Reader {
    std::atomic<bool> opened{false};
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> finished{false};
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
    mg::core::platform::Mutex cb_mtx;
    ReceiveCallback receive_cb;

    mg::core::platform::Mutex done_mtx;
    mg::core::platform::ConditionVariable done_cv;

    void mark_finished() {
        finished.store(true);
        mg::core::platform::LockGuard lk(done_mtx);
        done_cv.notify_all();
    }
};

SerialLink::SerialLink(const LinkDescriptor& descriptor)
    : descriptor_(descriptor) {
    reader_ = std::make_shared<Reader>();
}

SerialLink::~SerialLink() {
    close();
}

// 打开串口，配置波特率和 8N1，启动后台读取线程
bool SerialLink::open() {
    if (reader_->opened.load()) return true;

#ifdef _WIN32
    std::string port_path = "\\\\.\\" + descriptor_.endpoint;
    if (descriptor_.endpoint.rfind("\\\\.\\", 0) == 0) {
        port_path = descriptor_.endpoint;
    }

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

    SetupComm(handle_, 4096, 4096);
    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

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
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;
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

    reader_->stop_requested.store(false);
    reader_->opened.store(true);
    reader_->handle = handle_;

    // 启动后台读取线程
    if (reader_->receive_cb) {
        auto* arg = new std::shared_ptr<Reader>(reader_);
        read_thread_ = CreateThread(nullptr, 0, read_thread_proc, arg, 0, nullptr);
        if (!read_thread_) {
            delete arg;
            MGC_LOG_ERROR("[SerialLink] CreateThread failed for %s", descriptor_.endpoint.c_str());
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            reader_->opened.store(false);
            return false;
        }
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

    reader_->stop_requested.store(false);
    reader_->opened.store(true);
    reader_->fd = fd_;

    if (reader_->receive_cb) {
        read_thread_ = std::thread(read_thread_proc, reader_);
    }

    MGC_LOG_INFO("[SerialLink] Opened %s @ %u baud", descriptor_.endpoint.c_str(), descriptor_.baud_rate);
    return true;
#endif
}

// 等待读取线程退出，使用 CancelIoEx 中断阻塞的读操作
void SerialLink::join_read_thread() {
    if (abandoned_) return;  // 句柄归读线程所有，不能再动它
#ifdef _WIN32
    if (!read_thread_) return;

    // 中断串口上所有阻塞的 I/O 操作
    // CancelIoEx cancels all pending I/O for this handle on this thread.
    // If the read thread is not blocked in I/O, this is a no-op.
    if (handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(handle_, nullptr);
    }
    CancelSynchronousIo(read_thread_);

    // 等待读取线程退出（5 秒超时）。旧版 USB 串口驱动可能忽略 CancelIoEx。
    // 这里不强杀线程（KD-24）：TerminateThread 会在任意点中断，可能留下未释放
    // 的句柄或锁。超时就放弃等待，把句柄所有权留给仍在运行的读线程——不关闭
    // 串口句柄，代价是一次性的句柄泄漏，换取不会 use-after-free。
    const auto reader = reader_;
    bool finished = false;
    {
        mg::core::platform::UniqueLock lk(reader->done_mtx);
        finished = reader->done_cv.wait_for(
            lk, std::chrono::seconds(5), [&] { return reader->finished.load(); });
    }
    if (!finished) {
        MGC_LOG_ERROR(
            "[SerialLink] Read thread did not stop for %s; abandoning the port "
            "(handle intentionally left open)",
            descriptor_.endpoint.c_str());
        abandoned_ = true;
        return;
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
    if (!reader_->opened.load() && read_thread_ == nullptr && handle_ == INVALID_HANDLE_VALUE) return;
#else
    if (!reader_->opened.load() && !read_thread_.joinable() && fd_ < 0) return;
#endif

    // 通知读取线程停止
    reader_->stop_requested.store(true);
    set_receive_callback(nullptr);

    // 中断阻塞的 I/O 并等待线程退出
    join_read_thread();

#ifdef _WIN32
    if (!abandoned_ && handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif

    reader_->opened.store(false);
    MGC_LOG_INFO("[SerialLink] Closed %s", descriptor_.endpoint.c_str());
}

bool SerialLink::is_open() const {
    return reader_->opened.load();
}

// 向串口写入数据
bool SerialLink::send(const uint8_t* data, size_t size) {
    if (!reader_->opened.load()) return false;

#ifdef _WIN32
    size_t written_total = 0;
    while (written_total < size) {
        DWORD bytes_written = 0;
        const DWORD request = static_cast<DWORD>(
            std::min<size_t>(size - written_total, MAXDWORD));
        if (!WriteFile(
                handle_,
                data + written_total,
                request,
                &bytes_written,
                nullptr) ||
            bytes_written == 0) {
            return false;
        }
        written_total += bytes_written;
    }
    return true;
#else
    // POSIX 串口以 O_NONBLOCK 打开：短写与 EAGAIN 都必须补齐，否则一帧
    // MAVLink 会被静默丢掉（以前只写一次，飞控侧看到的是坏帧）。
    size_t written_total = 0;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(kWriteTimeoutMs);
    while (written_total < size) {
        const ssize_t written = ::write(fd_, data + written_total, size - written_total);
        if (written > 0) {
            written_total += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && errno == EINTR) continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) {
                MGC_LOG_WARN("[SerialLink] Write timed out for %s after %zu/%zu bytes",
                             descriptor_.endpoint.c_str(), written_total, size);
                return false;
            }
            pollfd pfd{};
            pfd.fd = fd_;
            pfd.events = POLLOUT;
            if (::poll(&pfd, 1, static_cast<int>(remaining)) <= 0) {
                MGC_LOG_WARN("[SerialLink] Write timed out for %s after %zu/%zu bytes",
                             descriptor_.endpoint.c_str(), written_total, size);
                return false;
            }
            continue;
        }
        MGC_LOG_WARN("[SerialLink] Write failed for %s after %zu/%zu bytes: %d",
                     descriptor_.endpoint.c_str(), written_total, size, errno);
        return false;
    }
    return true;
#endif
}

// 设置接收回调，若链路已打开且无读取线程则启动
void SerialLink::set_receive_callback(ReceiveCallback cb) {
    const bool should_start = static_cast<bool>(cb);
    {
        mg::core::platform::LockGuard lk(reader_->cb_mtx);
        reader_->receive_cb = std::move(cb);
    }

    // 若已打开且尚未启动读取线程，则现在启动。没有读取线程的串口链路不能工作，
    // 启动失败必须让链路变成“已关闭”而不是继续对外显示已打开（KD-05）。
#ifdef _WIN32
    if (reader_->opened.load() && should_start && !read_thread_) {
        auto* arg = new std::shared_ptr<Reader>(reader_);
        read_thread_ = CreateThread(nullptr, 0, read_thread_proc, arg, 0, nullptr);
        if (!read_thread_) {
            delete arg;
            fail_read_thread_start();
        }
    }
#else
    if (reader_->opened.load() && should_start && !read_thread_.joinable()) {
        try {
            read_thread_ = std::thread(read_thread_proc, reader_);
        } catch (const std::system_error& error) {
            // std::thread 构造失败会抛异常；这里不能让它穿出 C ABI 边界。
            MGC_LOG_ERROR("[SerialLink] Failed to start read thread for %s: %s",
                          descriptor_.endpoint.c_str(), error.what());
            fail_read_thread_start();
        }
    }
#endif
}

/// 读取线程启动失败：关闭端口并标记为未打开，避免上层以为链路可用。
void SerialLink::fail_read_thread_start() {
    MGC_LOG_ERROR("[SerialLink] Read thread unavailable for %s; closing the port",
                  descriptor_.endpoint.c_str());
    reader_->opened.store(false);
    reader_->stop_requested.store(true);
    reader_->mark_finished();  // 没有线程会再置位，避免 join 白等 5 秒
#ifdef _WIN32
    if (handle_ != INVALID_HANDLE_VALUE) {
        CancelIoEx(handle_, nullptr);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    read_thread_ = nullptr;
#else
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

// 后台读取线程入口函数，循环读取串口数据并分发给回调
#ifdef _WIN32
DWORD WINAPI SerialLink::read_thread_proc(LPVOID lpParam) {
    std::unique_ptr<std::shared_ptr<Reader>> holder(
        static_cast<std::shared_ptr<Reader>*>(lpParam));
    const std::shared_ptr<Reader> reader = *holder;
    uint8_t buffer[4096];

    while (!reader->stop_requested.load()) {
        DWORD bytes_read = 0;
        BOOL result = ReadFile(reader->handle, buffer, sizeof(buffer), &bytes_read, nullptr);

        if (result && bytes_read > 0) {
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(reader->cb_mtx);  // 加锁保护回调
                cb_copy = reader->receive_cb;
            }
            if (cb_copy) cb_copy(buffer, bytes_read);
        } else if (!result) {
            // 读取出错或被 CancelIoEx 中断，检查是否应退出
            DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED || reader->stop_requested.load()) {
                break;
            }
            // 真实错误，也退出
            reader->opened.store(false);
            break;
        }
        // result==true 但 bytes_read==0 表示无数据可用（非阻塞模式）
        // 必须 sleep 以避免 CPU 空转，同时让 reader_->stop_requested 有机会被检查
        if (result && bytes_read == 0) {
            Sleep(10);
        }
    }

    if (!reader->stop_requested.load()) {
        reader->opened.store(false);
    }
    reader->mark_finished();
    return 0;
}
#else
void SerialLink::read_thread_proc(std::shared_ptr<Reader> reader) {
    uint8_t buffer[4096];

    while (!reader->stop_requested.load()) {
        ssize_t bytes_read = ::read(reader->fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            ReceiveCallback cb_copy;
            {
                mg::core::platform::LockGuard lk(reader->cb_mtx);
                cb_copy = reader->receive_cb;
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
            if (!reader->stop_requested.load()) {
                MGC_LOG_ERROR("[SerialLink] Read error: %d", errno);
            }
            break;
        }
    }

    if (!reader->stop_requested.load()) {
        reader->opened.store(false);
    }
    reader->mark_finished();
}
#endif

} // namespace mg::core::transport
