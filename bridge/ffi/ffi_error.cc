#define MGCORE_EXPORTS
/**
 * ffi_error.cc - Error code implementation
 *
 * 错误码实现：线程本地错误状态和错误描述转换。
 *
 * Thread-local error state and error string conversion.
 */
#include "ffi_internal.h"

static thread_local mg_error_t tls_last_error = MG_ERROR_NONE;

MGCORE_API mg_error_t mg_get_last_error(void) {
    return tls_last_error;
}

MGCORE_API void mg_clear_last_error(void) {
    tls_last_error = MG_ERROR_NONE;
}

MGCORE_API void mg_set_last_error(mg_error_t error) {
    tls_last_error = error;
}

MGCORE_API const char* mg_error_string(mg_error_t error) {
    switch (error) {
        case MG_ERROR_NONE: return "No error";
        case MG_ERROR_NULL_CORE: return "Core handle is NULL";
        case MG_ERROR_NOT_CONNECTED: return "Not connected to vehicle";
        case MG_ERROR_SEND_FAILED: return "Failed to send MAVLink message";
        case MG_ERROR_INVALID_PARAM: return "Invalid parameter";
        case MG_ERROR_INVALID_STATE: return "Invalid state for this operation";
        case MG_ERROR_TIMEOUT: return "Operation timed out";
        case MG_ERROR_BUSY: return "Resource busy";
        case MG_ERROR_NOT_SUPPORTED: return "Operation not supported";
        case MG_ERROR_INTERNAL: return "Internal error";
        case MG_ERROR_BUFFER_TOO_SMALL: return "Buffer too small";
        case MG_ERROR_INVALID_MESSAGE: return "Invalid MAVLink message";
        case MG_ERROR_CHECKSUM_FAIL: return "Message checksum failed";
        case MG_ERROR_PROTOCOL: return "Protocol error";
        case MG_ERROR_NOT_FOUND: return "Resource not found";
        case MG_ERROR_CALIBRATION: return "Calibration error";
        default: return "Unknown error";
    }
}
