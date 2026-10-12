/**
 * mavlink_ftp.h - MAVLink FTP 客户端
 *
 * 通过 MAVLink FTP 协议（FILE_TRANSFER_PROTOCOL, msg 110）
 * 与飞控进行文件传输。支持列出目录和下载文件。
 *
 * 协议参考：https://mavlink.io/en/services/ftp.html
 */
#ifndef MGCORE_MAVLINK_FTP_H
#define MGCORE_MAVLINK_FTP_H

#include "ardupilotmega/mavlink.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

namespace mg::core::ftp {

/// FTP 操作结果
struct FtpResult {
    bool success = false;
    std::string error;
    uint8_t error_code = 0;  // MAV_FTP_ERR
};

/// 目录条目
struct DirEntry {
    std::string name;
    bool is_directory = false;
    uint32_t size = 0;
};

/// FTP 会话状态
enum class FtpState : uint8_t {
    Idle,
    Listing,       // 正在列目录
    Downloading,   // 正在下载文件
    Uploading,     // 正在上传文件
};

/// FTP 进度回调
using FtpProgressCallback = std::function<void(uint32_t received, uint32_t total)>;

/// MAVLink FTP 客户端
/// 通过 FILE_TRANSFER_PROTOCOL 消息与飞控通信
class MavlinkFtpClient {
public:
    /// 旧版发送函数类型：接收已经序列化的 MAVLink 字节。
    using SendFn = std::function<bool(const uint8_t* data, size_t size)>;
    /// 新版发送函数类型：由适配器使用自己的 TX sequence 完成最终打包。
    using MessageSendFn = std::function<bool(mavlink_message_t& message)>;

    explicit MavlinkFtpClient(SendFn send_fn);

    /// 让 FTP 复用适配器实例的发送器。未设置时保留旧版 SendFn 行为。
    void set_message_send_fn(MessageSendFn send_fn) {
        message_send_fn_ = std::move(send_fn);
    }
    ~MavlinkFtpClient();

    /// 列出目录内容
    /// 返回目录条目列表，失败返回空
    std::vector<DirEntry> list_directory(uint8_t target_sys, uint8_t target_comp,
                                          const std::string& path,
                                          FtpResult* result = nullptr);

    /// 下载文件
    /// 写入到 local_path，返回是否成功
    bool download_file(uint8_t target_sys, uint8_t target_comp,
                       const std::string& remote_path,
                       const std::string& local_path,
                       FtpProgressCallback progress_cb = nullptr,
                       FtpResult* result = nullptr);

    bool upload_file(uint8_t target_sys, uint8_t target_comp,
                     const std::string& local_path,
                     const std::string& remote_path,
                     FtpProgressCallback progress_cb = nullptr,
                     FtpResult* result = nullptr);

    /// 处理收到的 FILE_TRANSFER_PROTOCOL 消息
    /// 在 MavlinkAdapter 的 process() 中调用
    void handle_ftp_message(const mavlink_file_transfer_protocol_t& ftp);

    /// 检查是否正在进行 FTP 操作
    bool is_busy() const { return state_ != FtpState::Idle; }

    /// 取消当前操作
    void cancel();

    /// 设置超时时间（毫秒）
    void set_timeout_ms(int ms) { timeout_ms_ = ms; }

    // ================================================================
    // 异步传输（FFI 使用）
    //
    // 同步的 list_directory/download_file/upload_file 会阻塞调用线程直到
    // 传输结束，直接放在 FFI 调用线程上等于让 Flutter 侧卡死，也无法在传输
    // 途中读取进度或取消（P1）。异步入口把真正的传输放到一个后台线程上，
    // 调用线程只负责「发起 → 轮询状态 → 取消」，与 mg_onboard_log_* 的
    // 既有模式一致，也和 QGC/MP 用独立 FTP 线程的做法一致。
    // 同一时刻只允许一个传输；再次发起返回 false（协议本身是单会话）。
    // ================================================================

    /// 异步传输类型
    enum class FtpTask : uint8_t {
        None,
        List,
        Download,
        Upload,
    };

    /// 异步传输状态快照（线程安全）
    struct FtpTransferStatus {
        FtpTask task = FtpTask::None;
        bool active = false;      // 后台线程仍在传输
        bool finished = false;    // 已结束（成功/失败/取消都算），结果保留到下次发起
        bool success = false;
        bool cancelled = false;
        uint8_t error_code = 0;   // MAV_FTP_ERR
        std::string error;
        uint8_t target_sys = 0;
        uint8_t target_comp = 0;
        std::string remote_path;
        std::string local_path;
        uint64_t transferred_bytes = 0;
        uint64_t total_bytes = 0; // 0 表示未知（列表，或飞控未声明大小）
        std::vector<DirEntry> entries;  // task == List 时的结果
    };

    /// 发起异步列目录；已有传输在进行时返回 false。
    bool start_list_directory(uint8_t target_sys, uint8_t target_comp,
                              const std::string& path);
    /// 发起异步下载；已有传输在进行时返回 false。
    bool start_download_file(uint8_t target_sys, uint8_t target_comp,
                             const std::string& remote_path,
                             const std::string& local_path);
    /// 发起异步上传；已有传输在进行时返回 false。
    bool start_upload_file(uint8_t target_sys, uint8_t target_comp,
                           const std::string& local_path,
                           const std::string& remote_path);

    /// 读取当前/最近一次传输的状态快照。
    FtpTransferStatus transfer_status() const;

    /// 是否有传输正在后台进行。
    bool transfer_active() const { return worker_active_.load(); }

    /// 等待传输结束（同步调用方使用，如 @SYS/uarts.txt 这类小文件读取）。
    /// 返回 true 表示在超时前结束；超时返回 false，调用方应 cancel()。
    bool wait_for_completion(int timeout_ms);

    /// 取消并等待后台线程退出；析构与适配器销毁时调用。
    void shutdown();

private:
    SendFn send_fn_;
    MessageSendFn message_send_fn_;
    // 传输状态由工作线程写、被 FFI/UI 线程经 is_busy()/cancel() 读写，用原子
    // 消除数据竞争（TSan 曾在 download_file 的 finish_failure 处报 race）。
    // 与 cancelled_/cancel_requested_/worker_active_ 同一模式：这是单值状态标志，
    // 不属于 mtx_ 保护的请求/响应协议不变量。
    std::atomic<FtpState> state_{FtpState::Idle};
    int timeout_ms_ = 3000;
    int max_retries_ = 3;
    uint16_t seq_number_ = 0;
    uint8_t session_ = 0;
    std::atomic<bool> cancelled_{false};
    // cancel() 同时置位这两个：cancelled_ 供正在跑的循环读取，
    // cancel_requested_ 供「已 start 但传输体还没开始」的窗口使用，
    // 由各传输体开头 exchange(false) 接过来，避免刚点取消就丢请求。
    std::atomic<bool> cancel_requested_{false};

    // 异步传输状态。status_mtx_ 只保护结果字段，绝不用于等待响应，
    // 否则 transfer_status() 会被 wait_response 的超时拖住（正是 P1）。
    mutable std::mutex status_mtx_;
    std::condition_variable status_cv_;
    FtpTransferStatus status_{};
    std::thread worker_;
    std::atomic<bool> worker_active_{false};
    std::atomic<uint32_t> progress_bytes_{0};
    std::atomic<uint32_t> progress_total_{0};

    // 响应同步
    std::mutex mtx_;
    std::condition_variable cv_;
    bool response_pending_ = false;
    bool response_received_ = false;
    uint16_t expected_response_seq_ = 0;
    uint8_t expected_request_opcode_ = 0;
    mavlink_file_transfer_protocol_t last_response_{};

    // 目录列表结果
    std::vector<DirEntry> dir_entries_;

    // 文件下载结果
    std::vector<uint8_t> file_data_;

    // 内部方法
    bool send_ftp_request(uint8_t target_sys, uint8_t target_comp,
                          uint8_t opcode, uint8_t session,
                          uint32_t offset, uint8_t size, uint16_t sequence,
                          const uint8_t* data = nullptr);

    bool request_response(uint8_t target_sys, uint8_t target_comp,
                          uint8_t opcode, uint8_t session,
                          uint32_t offset, uint8_t size,
                          const uint8_t* data,
                          mavlink_file_transfer_protocol_t* response,
                          int retries = -1,
                          int timeout_ms = -1);

    bool wait_response(int timeout_ms,
                       mavlink_file_transfer_protocol_t* response);

    /// 启动一个后台传输；已有传输在进行时返回 false。
    bool start_task(FtpTask task, uint8_t target_sys, uint8_t target_comp,
                    const std::string& remote_path,
                    const std::string& local_path);

    /// 后台线程主体：跑完一次阻塞传输并写入状态。
    void run_task(FtpTask task, uint8_t target_sys, uint8_t target_comp,
                  const std::string& remote_path,
                  const std::string& local_path);

    // FTP 操作码（MAVLink FTP 协议）
    static constexpr uint8_t kOpNone = 0;
    static constexpr uint8_t kOpTerminateSession = 1;
    static constexpr uint8_t kOpResetSessions = 2;
    static constexpr uint8_t kOpListDirectory = 3;
    static constexpr uint8_t kOpOpenFileRO = 4;
    static constexpr uint8_t kOpReadFile = 5;
    static constexpr uint8_t kOpCreateFile = 6;
    static constexpr uint8_t kOpWriteFile = 7;
    static constexpr uint8_t kOpRemoveFile = 8;
    static constexpr uint8_t kOpCreateDirectory = 9;
    static constexpr uint8_t kOpRemoveDirectory = 10;
    static constexpr uint8_t kOpOpenFileWO = 11;
    static constexpr uint8_t kOpTruncateFile = 12;
    static constexpr uint8_t kOpRename = 13;
    static constexpr uint8_t kOpCalcFileCRC32 = 14;
    static constexpr uint8_t kOpBurstReadFile = 15;
    static constexpr uint8_t kRspAck = 128;
    static constexpr uint8_t kRspNak = 129;

    // FTP 结果码
    static constexpr uint8_t kResultSuccess = 0;
    static constexpr uint8_t kResultFail = 1;
    static constexpr uint8_t kResultFailErrno = 2;
    static constexpr uint8_t kResultInvalidDataSize = 3;
    static constexpr uint8_t kResultInvalidSession = 4;
    static constexpr uint8_t kResultNoSessionsAvailable = 5;
    static constexpr uint8_t kResultEOF = 6;
    static constexpr uint8_t kResultUnknownCommand = 7;
    static constexpr uint8_t kResultFileExists = 8;
    static constexpr uint8_t kResultFileProtected = 9;
    static constexpr uint8_t kResultFileNotFound = 10;

public:
    // 最大单次读写大小，同时也是 MAVLink FTP 路径长度的上限
    // （路径与数据共用同一个 payload 缓冲区）。FFI 会用它预校验参数。
    static constexpr uint8_t kMaxReadSize = 239;

private:
};

} // namespace mg::core::ftp

#endif /* MGCORE_MAVLINK_FTP_H */
