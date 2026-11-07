#ifndef MGCORE_FFI_API_CORE_H
#define MGCORE_FFI_API_CORE_H

#ifdef __cplusplus
extern "C" {
#elif !defined(__cplusplus)
#include <stdbool.h>
#endif

#define MGCORE_VERSION_MAJOR 1
#define MGCORE_VERSION_MINOR 1
#define MGCORE_VERSION_PATCH 0

#ifdef _WIN32
  #ifdef MGCORE_EXPORTS
    #define MGCORE_API __declspec(dllexport)
  #else
    #define MGCORE_API __declspec(dllimport)
  #endif
#else
  #define MGCORE_API __attribute__((visibility("default")))
#endif

typedef struct mg_core_t mg_core_t;

typedef enum {
    MG_ERROR_NONE              =  0,
    MG_ERROR_NULL_CORE         = -1,
    MG_ERROR_NOT_CONNECTED     = -2,
    MG_ERROR_SEND_FAILED       = -3,
    MG_ERROR_INVALID_PARAM     = -4,
    MG_ERROR_INVALID_STATE     = -5,
    MG_ERROR_TIMEOUT           = -6,
    MG_ERROR_BUSY              = -7,
    MG_ERROR_NOT_SUPPORTED     = -8,
    MG_ERROR_INTERNAL          = -9,
    MG_ERROR_BUFFER_TOO_SMALL  = -10,
    MG_ERROR_INVALID_MESSAGE   = -11,
    MG_ERROR_CHECKSUM_FAIL     = -12,
    MG_ERROR_PROTOCOL          = -13,
    MG_ERROR_NOT_FOUND         = -14,
    MG_ERROR_CALIBRATION       = -15,
    MG_ERROR_REJECTED          = -16,
} mg_error_t;

MGCORE_API const char* mg_error_string(mg_error_t error);
MGCORE_API mg_error_t mg_get_last_error(void);
MGCORE_API void mg_clear_last_error(void);
MGCORE_API void mg_set_last_error(mg_error_t error);

MGCORE_API mg_core_t* mg_create(void);
MGCORE_API void mg_destroy(mg_core_t* core);



MGCORE_API void mg_free_string(char* str);
MGCORE_API char* mg_system_get_diagnostics(mg_core_t* core);
MGCORE_API char* mg_operations_get_snapshot(mg_core_t* core);
MGCORE_API char* mg_operations_cancel(mg_core_t* core, const char* operation_id);
MGCORE_API char* mg_operations_cancel_for_vehicle(
    mg_core_t* core, int vehicle_id, const char* operation_id);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_CORE_H */
