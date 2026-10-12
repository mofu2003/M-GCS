/**
 * core_status.h - 核心层错误传播类型
 *
 * Replaces bare `bool` returns in core modules with a Status type that
 * carries an error code and a short human-readable reason.
 *
 * Design goals:
 *   - Zero heap allocation (fixed-size internal buffer)
 *   - Implicit bool conversion (drop-in replacement for `bool` returns)
 *   - No exceptions, no RTTI
 *   - Thread-safe (value type, no shared state)
 *
 * Usage:
 *   Status result = adapter.set_param(...);
 *   if (!result) {
 *       log("set_param failed: %s", result.message());
 *   }
 *
 * QGC precedent:
 *   QGC uses MAV_RESULT codes + MavCmdResultFailureCode_t in VehicleTypes.h
 *   and RequestMessageResultHandlerFailureCode_t for async failures.
 *   These are callback-based; our Status is a synchronous return type.
 *
 * MP precedent:
 *   MP uses MAVLink.MAV_MISSION_RESULT enum returns + try/catch for errors.
 *   We avoid exceptions: no try/catch and no RTTI in core paths.
 */

#ifndef MGCORE_CORE_STATUS_H
#define MGCORE_CORE_STATUS_H

#include <cstdint>
#include <cstring>

namespace mg::core {

/// 核心层错误码枚举，与 ffi_api.h 中的 mg_error_t 对应
/// Mirrors mg_error_t from ffi_api.h but lives in the core layer
/// so core modules don't depend on the FFI header.
enum class ErrorCode : int32_t {
    Ok                 =  0,
    NotConnected       = -2,
    SendFailed         = -3,
    InvalidParam       = -4,
    InvalidState       = -5,
    Timeout            = -6,
    Busy               = -7,
    NotSupported       = -8,
    Internal           = -9,
    NotFound           = -14,
    Calibration        = -15,
    Protocol           = -13,
};

/// 轻量级结果类型，栈上分配，携带错误码和简短原因字符串
/// Carries an error code + a short reason string (stack-allocated).
class Status {
public:
    /// 默认构造：成功状态
    Status() : code_(ErrorCode::Ok) { msg_[0] = '\0'; }

    /// 返回成功状态（无附加消息）
    static Status ok() { return Status(); }

    /// 构造一个失败状态，携带错误码和可选消息
    static Status error(ErrorCode code, const char* message = "") {
        Status s;
        s.code_ = code;
        if (message) {
            size_t len = strlen(message);
            if (len >= kMsgCapacity) len = kMsgCapacity - 1;
            memcpy(s.msg_, message, len);
            s.msg_[len] = '\0';
        }
        return s;
    }

    /// 显式布尔转换：true 表示成功，false 表示失败
    /// 使用 explicit 防止意外的隐式类型转换（如 int x = status）
    explicit operator bool() const { return code_ == ErrorCode::Ok; }

    /// 显式访问器
    ErrorCode code() const { return code_; }
    const char* message() const { return msg_; }

private:
    ErrorCode code_;
    static constexpr size_t kMsgCapacity = 128;
    char msg_[kMsgCapacity];
};

} // namespace mg::core

#endif // MGCORE_CORE_STATUS_H
