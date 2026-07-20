/**
 * mavlink_ftp.cc - MAVLink FTP 客户端实现
 *
 * 通过 FILE_TRANSFER_PROTOCOL (msg 110) 消息实现文件传输。
 * 参照 QGC FTPManager 实现。
 */
#include "core/ftp/mavlink_ftp.h"
#include "core/log/logging.h"
#include <cstring>
#include <fstream>

namespace mg::core::ftp {

MavlinkFtpClient::MavlinkFtpClient(SendFn send_fn)
    : send_fn_(std::move(send_fn)) {}

MavlinkFtpClient::~MavlinkFtpClient() {
    cancel();
}

bool MavlinkFtpClient::send_ftp_request(uint8_t target_sys, uint8_t target_comp,
                                         uint8_t opcode, uint8_t session,
                                         uint32_t offset, uint8_t size,
                                         const uint8_t* data) {
    mavlink_file_transfer_protocol_t ftp{};
    ftp.target_system = target_sys;
    ftp.target_component = target_comp;
    ftp.target_network = 0;

    // 构建 FTP 协议载荷
    uint8_t* payload = ftp.payload;
    payload[0] = seq_number_ & 0xFF;       // seq_number low
    payload[1] = (seq_number_ >> 8) & 0xFF; // seq_number high
    payload[2] = session;
    payload[3] = opcode;
    payload[4] = size;                      // size
    payload[5] = 0;                         // req_opcode (0 for request)
    payload[6] = 0;                         // burst_complete
    payload[7] = 0;                         // padding
    // offset at bytes 8-11 (little-endian)
    payload[8] = offset & 0xFF;
    payload[9] = (offset >> 8) & 0xFF;
    payload[10] = (offset >> 16) & 0xFF;
    payload[11] = (offset >> 24) & 0xFF;

    if (data && size > 0) {
        memcpy(payload + 12, data, size);
    }

    seq_number_++;

    // 打包并发送
    mavlink_message_t msg;
    mavlink_msg_file_transfer_protocol_encode(255, 0, &msg, &ftp);
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    uint16_t len = mavlink_msg_to_send_buffer(buf, &msg);
    return send_fn_(buf, len);
}

bool MavlinkFtpClient::wait_response(int timeout_ms) {
    std::unique_lock<std::mutex> lk(mtx_);
    response_received_ = false;
    return cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                        [this]() { return response_received_ || cancelled_.load(); });
}

void MavlinkFtpClient::handle_ftp_message(const mavlink_file_transfer_protocol_t& ftp) {
    std::lock_guard<std::mutex> lk(mtx_);
    last_response_ = ftp;
    response_received_ = true;
    cv_.notify_all();
}

void MavlinkFtpClient::cancel() {
    cancelled_ = true;
    cv_.notify_all();
    state_ = FtpState::Idle;
}

std::vector<DirEntry> MavlinkFtpClient::list_directory(uint8_t target_sys, uint8_t target_comp,
                                                        const std::string& path,
                                                        FtpResult* result) {
    if (is_busy()) {
        if (result) { result->success = false; result->error = "FTP busy"; }
        return {};
    }

    state_ = FtpState::Listing;
    cancelled_ = false;
    dir_entries_.clear();

    // 发送 LIST_DIRECTORY 请求
    uint8_t path_data[239] = {};
    size_t path_len = path.size();
    if (path_len > 238) path_len = 238;
    memcpy(path_data, path.c_str(), path_len);
    path_data[path_len] = 0;  // null terminator

    bool ok = send_ftp_request(target_sys, target_comp,
                               kOpListDirectory, 0, 0,
                               static_cast<uint8_t>(path_len + 1),
                               path_data);
    if (!ok) {
        state_ = FtpState::Idle;
        if (result) { result->success = false; result->error = "send failed"; }
        return {};
    }

    // 循环接收 LIST_DIRECTORY 响应
    while (!cancelled_.load()) {
        if (!wait_response(timeout_ms_)) {
            // 超时，重试一次
            send_ftp_request(target_sys, target_comp,
                            kOpListDirectory, 0, 0,
                            static_cast<uint8_t>(path_len + 1),
                            path_data);
            if (!wait_response(timeout_ms_)) {
                state_ = FtpState::Idle;
                if (result) { result->success = false; result->error = "timeout"; }
                return dir_entries_;
            }
        }

        const uint8_t* payload = last_response_.payload;
        uint8_t opcode = payload[3];
        uint8_t req_opcode = payload[5];
        uint8_t error_code = payload[6];  // burst_complete used as error

        if (req_opcode != kOpListDirectory) break;

        if (opcode == kOpNone && error_code == kResultEOF) {
            // 目录列表结束
            break;
        }

        if (opcode == kOpNone && error_code == kResultSuccess) {
            // 解析目录条目
            uint8_t size = payload[4];
            if (size > 0) {
                const char* entry_str = reinterpret_cast<const char*>(payload + 12);
                DirEntry entry;
                // 第一个字符表示类型：D=目录, F=文件
                if (entry_str[0] == 'D') {
                    entry.is_directory = true;
                    entry.name = entry_str + 1;
                } else if (entry_str[0] == 'F') {
                    entry.is_directory = false;
                    entry.name = entry_str + 1;
                    // 文件大小可能在后面的字段
                    if (size > entry.name.size() + 2) {
                        entry.size = *reinterpret_cast<const uint32_t*>(
                            payload + 12 + 1 + entry.name.size() + 1);
                    }
                }
                if (!entry.name.empty()) {
                    dir_entries_.push_back(entry);
                }
            }
        }
    }

    state_ = FtpState::Idle;
    if (result) { result->success = true; }
    return dir_entries_;
}

bool MavlinkFtpClient::download_file(uint8_t target_sys, uint8_t target_comp,
                                      const std::string& remote_path,
                                      const std::string& local_path,
                                      FtpProgressCallback progress_cb,
                                      FtpResult* result) {
    if (is_busy()) {
        if (result) { result->success = false; result->error = "FTP busy"; }
        return false;
    }

    state_ = FtpState::Downloading;
    cancelled_ = false;
    file_data_.clear();

    // 1. 打开文件 (OpenFileRO)
    uint8_t path_data[239] = {};
    size_t path_len = remote_path.size();
    if (path_len > 238) path_len = 238;
    memcpy(path_data, remote_path.c_str(), path_len);

    bool ok = send_ftp_request(target_sys, target_comp,
                               kOpOpenFileRO, 0, 0,
                               static_cast<uint8_t>(path_len + 1),
                               path_data);
    if (!ok || !wait_response(timeout_ms_)) {
        state_ = FtpState::Idle;
        if (result) { result->success = false; result->error = "open failed"; }
        return false;
    }

    const uint8_t* resp = last_response_.payload;
    if (resp[3] != kOpNone || resp[6] != kResultSuccess) {
        state_ = FtpState::Idle;
        if (result) {
            result->success = false;
            result->error = "open rejected";
            result->error_code = resp[6];
        }
        return false;
    }

    // 获取文件大小（在响应的 offset 字段）
    uint32_t file_size = resp[8] | (resp[9] << 8) | (resp[10] << 16) | (resp[11] << 24);
    session_ = resp[2];

    // 2. 读取文件 (ReadFile) 循环
    uint32_t offset = 0;
    while (offset < file_size && !cancelled_.load()) {
        ok = send_ftp_request(target_sys, target_comp,
                              kOpReadFile, session_, offset, 0);
        if (!ok || !wait_response(timeout_ms_)) {
            // 重试
            if (!ok || !wait_response(timeout_ms_)) {
                // 终止会话
                send_ftp_request(target_sys, target_comp,
                                kOpTerminateSession, session_, 0, 0);
                state_ = FtpState::Idle;
                if (result) { result->success = false; result->error = "read timeout"; }
                return false;
            }
        }

        resp = last_response_.payload;
        uint8_t read_size = resp[4];
        uint8_t read_error = resp[6];

        if (read_error == kResultEOF) {
            // 文件结束
            break;
        }

        if (read_error != kResultSuccess) {
            send_ftp_request(target_sys, target_comp,
                            kOpTerminateSession, session_, 0, 0);
            state_ = FtpState::Idle;
            if (result) { result->success = false; result->error = "read error"; }
            return false;
        }

        if (read_size > 0) {
            file_data_.insert(file_data_.end(),
                              resp + 12, resp + 12 + read_size);
            offset += read_size;
            if (progress_cb) {
                progress_cb(offset, file_size);
            }
        }
    }

    // 3. 终止会话
    send_ftp_request(target_sys, target_comp,
                     kOpTerminateSession, session_, 0, 0);
    wait_response(1000);

    // 4. 写入本地文件
    if (!cancelled_.load() && !file_data_.empty()) {
        FILE* f = fopen(local_path.c_str(), "wb");
        if (f) {
            fwrite(file_data_.data(), 1, file_data_.size(), f);
            fclose(f);
            MGC_LOG_INFO("[FTP] Downloaded %s -> %s (%zu bytes)",
                         remote_path.c_str(), local_path.c_str(), file_data_.size());
        } else {
            state_ = FtpState::Idle;
            if (result) { result->success = false; result->error = "write failed"; }
            return false;
        }
    }

    state_ = FtpState::Idle;
    if (result) { result->success = true; }
    return true;
}

} // namespace mg::core::ftp
