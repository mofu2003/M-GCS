#ifndef MGCORE_FFI_API_CORE_H
#define MGCORE_FFI_API_CORE_H

#ifdef __cplusplus
extern "C" {
#elif !defined(__cplusplus)
#include <stdbool.h>
#endif

#define MGCORE_VERSION_MAJOR 0
#define MGCORE_VERSION_MINOR 2
#define MGCORE_VERSION_PATCH 2

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

typedef enum {
    MG_EVENT_HEARTBEAT       = 0,
    MG_EVENT_SYS_STATUS      = 1,
    MG_EVENT_GPS             = 2,
    MG_EVENT_ATTITUDE        = 3,
    MG_EVENT_CONNECTION      = 4,
    MG_EVENT_PARAMETERS      = 5,
    MG_EVENT_MISSION         = 6,
    MG_EVENT_LOG             = 7,
    MG_EVENT_TELEMETRY       = 8,
    MG_EVENT_FIRMWARE        = 9,
    MG_EVENT_SENSOR_HEALTH   = 10,
    MG_EVENT_IMU             = 11,
    MG_EVENT_PRESSURE        = 12,
    MG_EVENT_GLOBAL_POSITION = 13,
    MG_EVENT_HOME_POSITION   = 14,
    MG_EVENT_RC_CHANNELS     = 15,
    MG_EVENT_SERVO_OUTPUT    = 16,
    MG_EVENT_STATUSTEXT      = 17,
    MG_EVENT_BATTERY         = 18,
    MG_EVENT_HIGHRES_IMU     = 19,
    MG_EVENT_EXTENDED_SYS_STATE = 20,
    MG_EVENT_INITIAL_CONNECT = 21,
    MG_EVENT_VIBRATION = 22,
    MG_EVENT_ESC_STATUS = 23,
    MG_EVENT_WIND_ESTIMATE = 24,
    MG_EVENT_PREARM_CHECK = 25,
    MG_EVENT_WAYPOINT_REACHED = 26,
    MG_EVENT_DISTANCE_SENSOR = 27,
    MG_EVENT_BATTERY_FAILSAFE = 28,
    MG_EVENT_ADSB = 29,
} mg_event_type_t;

/* The json argument is allocated by mgcore and must be released by the callback
 * with mg_free_string() when the callback is done with it. */
typedef void (*mg_event_callback_t)(mg_event_type_t type, char* json, void* user_data);

MGCORE_API void mg_set_event_callback(mg_core_t* core,
                                       mg_event_callback_t cb,
                                       void* user_data);

MGCORE_API void mg_free_string(char* str);
MGCORE_API char* mg_get_version(void);
MGCORE_API char* mg_system_get_diagnostics(mg_core_t* core);
MGCORE_API char* mg_operations_get_snapshot(mg_core_t* core);
MGCORE_API char* mg_operations_cancel(mg_core_t* core, const char* operation_id);
MGCORE_API char* mg_operations_get_snapshot_for_vehicle(
    mg_core_t* core, int vehicle_id);
MGCORE_API char* mg_operations_cancel_for_vehicle(
    mg_core_t* core, int vehicle_id, const char* operation_id);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_CORE_H */
