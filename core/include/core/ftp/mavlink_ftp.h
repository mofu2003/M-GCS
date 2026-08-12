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

#include "common/mavlink.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>

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
    /// 发送函数类型：打包并发送 MAVLink 消息
    using SendFn = std::function<bool(const uint8_t* data, size_t size)>;

    explicit MavlinkFtpClient(SendFn send_fn);
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

    /// 处理收到的 FILE_TRANSFER_PROTOCOL 消息
    /// 在 MavlinkAdapter 的 process() 中调用
    void handle_ftp_message(const mavlink_file_transfer_protocol_t& ftp);

    /// 检查是否正在进行 FTP 操作
    bool is_busy() const { return state_ != FtpState::Idle; }

    /// 取消当前操作
    void cancel();

    /// 设置超时时间（毫秒）
    void set_timeout_ms(int ms) { timeout_ms_ = ms; }

private:
    SendFn send_fn_;
    FtpState state_ = FtpState::Idle;
    int timeout_ms_ = 3000;
    int max_retries_ = 3;
    uint16_t seq_number_ = 0;
    uint8_t session_ = 0;
    std::atomic<bool> cancelled_{false};

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
    std::string download_path_;

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

    // 最大单次读取大小
    static constexpr uint8_t kMaxReadSize = 239;
};

} // namespace mg::core::ftp

#endif /* MGCORE_MAVLINK_FTP_H */
