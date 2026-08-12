/**
 * mavlink_ftp.cc - MAVLink FTP client implementation
 *
 * Implements the stop-and-wait OpenFileRO/ReadFile flow used by Mission
 * Planner for small ArduPilot system files such as @SYS/uarts.txt.
 */
#include "core/ftp/mavlink_ftp.h"

#include "core/log/logging.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>

namespace mg::core::ftp {
namespace {

constexpr size_t kFtpHeaderSize = 12;

uint16_t read_u16_le(const uint8_t* value) {
    return static_cast<uint16_t>(value[0]) |
           (static_cast<uint16_t>(value[1]) << 8);
}

uint32_t read_u32_le(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8) |
           (static_cast<uint32_t>(value[2]) << 16) |
           (static_cast<uint32_t>(value[3]) << 24);
}

void write_u16_le(uint8_t* output, uint16_t value) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
}

void write_u32_le(uint8_t* output, uint32_t value) {
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8);
    output[2] = static_cast<uint8_t>(value >> 16);
    output[3] = static_cast<uint8_t>(value >> 24);
}

} // namespace

MavlinkFtpClient::MavlinkFtpClient(SendFn send_fn)
    : send_fn_(std::move(send_fn)) {}

MavlinkFtpClient::~MavlinkFtpClient() {
    cancel();
}

bool MavlinkFtpClient::send_ftp_request(uint8_t target_sys,
                                        uint8_t target_comp,
                                        uint8_t opcode,
                                        uint8_t session,
                                        uint32_t offset,
                                        uint8_t size,
                                        uint16_t sequence,
                                        const uint8_t* data) {
    if (!send_fn_ || size > kMaxReadSize) return false;

    mavlink_file_transfer_protocol_t ftp{};
    ftp.target_system = target_sys;
    ftp.target_component = target_comp;
    ftp.target_network = 0;

    uint8_t* payload = ftp.payload;
    write_u16_le(payload, sequence);
    payload[2] = session;
    payload[3] = opcode;
    payload[4] = size;
    payload[5] = 0;
    payload[6] = 0;
    payload[7] = 0;
    write_u32_le(payload + 8, offset);
    if (data != nullptr && size != 0) {
        std::memcpy(payload + kFtpHeaderSize, data, size);
    }

    mavlink_message_t msg{};
    mavlink_msg_file_transfer_protocol_encode(255, 0, &msg, &ftp);
    std::array<uint8_t, MAVLINK_MAX_PACKET_LEN> buffer{};
    const uint16_t length = mavlink_msg_to_send_buffer(buffer.data(), &msg);
    return send_fn_(buffer.data(), length);
}

bool MavlinkFtpClient::request_response(
        uint8_t target_sys,
        uint8_t target_comp,
        uint8_t opcode,
        uint8_t session,
        uint32_t offset,
        uint8_t size,
        const uint8_t* data,
        mavlink_file_transfer_protocol_t* response,
        int retries,
        int timeout_ms) {
    if (response == nullptr) return false;

    const uint16_t request_sequence = seq_number_;
    const uint16_t response_sequence =
        static_cast<uint16_t>(request_sequence + 1);
    const int retry_count = retries < 0 ? max_retries_ : retries;
    const int response_timeout = timeout_ms < 0 ? timeout_ms_ : timeout_ms;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        response_pending_ = true;
        response_received_ = false;
        expected_response_seq_ = response_sequence;
        expected_request_opcode_ = opcode;
    }

    bool received = false;
    for (int attempt = 0;
         attempt <= retry_count && !cancelled_.load();
         ++attempt) {
        // Retries intentionally reuse the exact sequence number. ArduPilot
        // caches duplicate FTP requests, and both MP and QGC follow this rule.
        if (!send_ftp_request(target_sys, target_comp, opcode, session, offset,
                              size, request_sequence, data)) {
            continue;
        }
        if (wait_response(response_timeout, response)) {
            received = true;
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        response_pending_ = false;
        response_received_ = false;
    }

    // Reserve both the outgoing and expected response sequence numbers even
    // when the response is lost. This prevents a late ACK from a failed
    // transaction being accepted by the next transaction.
    seq_number_ = static_cast<uint16_t>(response_sequence + 1);
    return received;
}

bool MavlinkFtpClient::wait_response(
        int timeout_ms,
        mavlink_file_transfer_protocol_t* response) {
    std::unique_lock<std::mutex> lock(mtx_);
    const bool ready = cv_.wait_for(
        lock, std::chrono::milliseconds(timeout_ms),
        [this]() { return response_received_ || cancelled_.load(); });
    if (!ready || cancelled_.load() || !response_received_) return false;

    *response = last_response_;
    response_received_ = false;
    return true;
}

void MavlinkFtpClient::handle_ftp_message(
        const mavlink_file_transfer_protocol_t& ftp) {
    const uint8_t* payload = ftp.payload;
    const uint16_t sequence = read_u16_le(payload);
    const uint8_t opcode = payload[3];
    const uint8_t size = payload[4];
    const uint8_t request_opcode = payload[5];

    std::lock_guard<std::mutex> lock(mtx_);
    if (!response_pending_ || response_received_) return;
    if (opcode != kRspAck && opcode != kRspNak) return;
    if (size > kMaxReadSize) return;
    if (sequence != expected_response_seq_ ||
        request_opcode != expected_request_opcode_) {
        return;
    }

    last_response_ = ftp;
    response_received_ = true;
    cv_.notify_all();
}

void MavlinkFtpClient::cancel() {
    cancelled_ = true;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        response_pending_ = false;
    }
    cv_.notify_all();
    state_ = FtpState::Idle;
}

std::vector<DirEntry> MavlinkFtpClient::list_directory(
        uint8_t target_sys,
        uint8_t target_comp,
        const std::string& path,
        FtpResult* result) {
    if (result != nullptr) *result = {};
    if (is_busy()) {
        if (result != nullptr) result->error = "FTP busy";
        return {};
    }
    if (path.size() > kMaxReadSize) {
        if (result != nullptr) result->error = "path too long";
        return {};
    }

    state_ = FtpState::Listing;
    cancelled_ = false;
    dir_entries_.clear();

    auto fail = [&](const std::string& error, uint8_t error_code = 0) {
        state_ = FtpState::Idle;
        if (result != nullptr) {
            result->success = false;
            result->error = error;
            result->error_code = error_code;
        }
        return dir_entries_;
    };

    uint32_t entry_offset = 0;
    while (!cancelled_.load()) {
        mavlink_file_transfer_protocol_t response{};
        const auto* path_data = reinterpret_cast<const uint8_t*>(path.data());
        if (!request_response(
                target_sys, target_comp, kOpListDirectory, 0, entry_offset,
                static_cast<uint8_t>(path.size()), path_data, &response)) {
            return fail(cancelled_.load() ? "cancelled" : "list timeout");
        }

        const uint8_t* payload = response.payload;
        if (payload[3] == kRspNak) {
            const uint8_t error_code =
                payload[4] >= 1 ? payload[kFtpHeaderSize] : kResultFail;
            if (error_code == kResultEOF) {
                state_ = FtpState::Idle;
                if (result != nullptr) result->success = true;
                return dir_entries_;
            }
            return fail("list rejected", error_code);
        }

        const uint8_t response_size = payload[4];
        if (response_size == 0) return fail("empty directory response");

        const uint8_t* cursor = payload + kFtpHeaderSize;
        const uint8_t* const limit = cursor + response_size;
        uint32_t returned_entries = 0;
        while (cursor < limit) {
            const uint8_t* const terminator =
                std::find(cursor, limit, static_cast<uint8_t>(0));
            const std::string encoded(
                reinterpret_cast<const char*>(cursor),
                static_cast<size_t>(terminator - cursor));
            cursor = terminator == limit ? limit : terminator + 1;
            if (encoded.empty()) continue;

            ++returned_entries;
            if (encoded[0] == 'S') continue;

            DirEntry entry;
            entry.is_directory = encoded[0] == 'D';
            if (!entry.is_directory && encoded[0] != 'F') continue;

            const size_t tab = encoded.find('\t', 1);
            entry.name = encoded.substr(
                1, tab == std::string::npos ? std::string::npos : tab - 1);
            if (!entry.is_directory && tab != std::string::npos) {
                try {
                    const unsigned long value = std::stoul(encoded.substr(tab + 1));
                    entry.size = static_cast<uint32_t>(std::min<unsigned long>(
                        value, std::numeric_limits<uint32_t>::max()));
                } catch (...) {
                    entry.size = 0;
                }
            }
            if (!entry.name.empty()) dir_entries_.push_back(std::move(entry));
        }

        if (returned_entries == 0) {
            return fail("invalid directory response");
        }
        entry_offset += returned_entries;
    }

    return fail("cancelled");
}

bool MavlinkFtpClient::download_file(
        uint8_t target_sys,
        uint8_t target_comp,
        const std::string& remote_path,
        const std::string& local_path,
        FtpProgressCallback progress_cb,
        FtpResult* result) {
    if (result != nullptr) *result = {};
    if (is_busy()) {
        if (result != nullptr) result->error = "FTP busy";
        return false;
    }
    if (remote_path.empty() || remote_path.size() > kMaxReadSize) {
        if (result != nullptr) result->error = "invalid remote path";
        return false;
    }

    state_ = FtpState::Downloading;
    cancelled_ = false;
    file_data_.clear();

    auto finish_failure = [&](const std::string& error,
                              uint8_t error_code = 0) {
        state_ = FtpState::Idle;
        if (result != nullptr) {
            result->success = false;
            result->error = error;
            result->error_code = error_code;
        }
        return false;
    };

    mavlink_file_transfer_protocol_t response{};
    // Mission Planner clears any stale read session before opening a system
    // file. This also recovers from an interrupted earlier metadata request.
    request_response(target_sys, target_comp, kOpResetSessions, 0, 0, 0,
                     nullptr, &response, 0, 1000);

    const auto* path_data =
        reinterpret_cast<const uint8_t*>(remote_path.data());
    if (!request_response(
            target_sys, target_comp, kOpOpenFileRO, 0, 0,
            static_cast<uint8_t>(remote_path.size()), path_data, &response)) {
        return finish_failure(cancelled_.load() ? "cancelled" : "open timeout");
    }

    const uint8_t* payload = response.payload;
    if (payload[3] == kRspNak) {
        const uint8_t error_code =
            payload[4] >= 1 ? payload[kFtpHeaderSize] : kResultFail;
        return finish_failure("open rejected", error_code);
    }
    if (payload[3] != kRspAck || payload[4] != sizeof(uint32_t)) {
        return finish_failure("invalid open response");
    }

    // OpenFileRO ACK stores the file length in data[0..3], not hdr.offset.
    const uint32_t file_size = read_u32_le(payload + kFtpHeaderSize);
    session_ = payload[2];

    uint32_t offset = 0;
    while (offset < file_size && !cancelled_.load()) {
        const uint8_t request_size = static_cast<uint8_t>(
            std::min<uint32_t>(kMaxReadSize, file_size - offset));
        if (!request_response(target_sys, target_comp, kOpReadFile, session_,
                              offset, request_size, nullptr, &response)) {
            request_response(target_sys, target_comp, kOpTerminateSession,
                             session_, 0, 0, nullptr, &response, 0, 1000);
            return finish_failure(
                cancelled_.load() ? "cancelled" : "read timeout");
        }

        payload = response.payload;
        if (payload[3] == kRspNak) {
            const uint8_t error_code =
                payload[4] >= 1 ? payload[kFtpHeaderSize] : kResultFail;
            // ArduPilot system files such as @SYS/uarts.txt are generated on
            // demand. Their OpenFileRO size can be an upper bound, so the
            // final ReadFile is terminated with EOF before that size is
            // reached. Mission Planner treats this EOF as successful
            // completion and keeps all bytes received so far.
            if (error_code == kResultEOF) break;
            request_response(target_sys, target_comp, kOpTerminateSession,
                             session_, 0, 0, nullptr, &response, 0, 1000);
            return finish_failure("read rejected", error_code);
        }

        const uint8_t read_size = payload[4];
        const uint32_t response_offset = read_u32_le(payload + 8);
        if (payload[3] != kRspAck || payload[2] != session_ ||
            response_offset != offset || read_size == 0 ||
            read_size > request_size ||
            static_cast<uint64_t>(offset) + read_size > file_size) {
            request_response(target_sys, target_comp, kOpTerminateSession,
                             session_, 0, 0, nullptr, &response, 0, 1000);
            return finish_failure("invalid read response");
        }

        file_data_.insert(file_data_.end(),
                          payload + kFtpHeaderSize,
                          payload + kFtpHeaderSize + read_size);
        offset += read_size;
        if (progress_cb) progress_cb(offset, file_size);
    }

    if (cancelled_.load()) {
        return finish_failure("cancelled");
    }

    // Session cleanup is best effort. A completed download remains valid even
    // if the server drops the final TerminateSession ACK.
    request_response(target_sys, target_comp, kOpTerminateSession, session_,
                     0, 0, nullptr, &response, 0, 1000);
    session_ = 0;

    std::ofstream output(local_path, std::ios::binary | std::ios::trunc);
    if (!output) return finish_failure("write failed");
    if (!file_data_.empty()) {
        output.write(reinterpret_cast<const char*>(file_data_.data()),
                     static_cast<std::streamsize>(file_data_.size()));
    }
    output.close();
    if (!output) return finish_failure("write failed");

    MGC_LOG_INFO("[FTP] Downloaded %s -> %s (%zu bytes)",
                 remote_path.c_str(), local_path.c_str(), file_data_.size());
    state_ = FtpState::Idle;
    if (result != nullptr) result->success = true;
    return true;
}

} // namespace mg::core::ftp
