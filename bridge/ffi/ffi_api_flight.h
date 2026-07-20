#ifndef MGCORE_FFI_API_FLIGHT_H
#define MGCORE_FFI_API_FLIGHT_H

#include "ffi_api_core.h"

#ifdef __cplusplus
extern "C" {
#endif

MGCORE_API char* mg_cmd_arm(mg_core_t* core);
MGCORE_API char* mg_cmd_disarm(mg_core_t* core);
MGCORE_API char* mg_cmd_set_mode(mg_core_t* core, const char* mode);
MGCORE_API char* mg_cmd_rtl(mg_core_t* core);
MGCORE_API char* mg_cmd_takeoff(mg_core_t* core, float altitude);
MGCORE_API char* mg_cmd_land(mg_core_t* core);
MGCORE_API char* mg_cmd_loiter(mg_core_t* core);
MGCORE_API char* mg_cmd_guided(mg_core_t* core, double lat, double lon, float alt);

MGCORE_API char* mg_command_arm(mg_core_t* core, int force);
MGCORE_API char* mg_command_disarm(mg_core_t* core, int force);
MGCORE_API char* mg_command_set_mode(mg_core_t* core, int base_mode, int custom_mode);
MGCORE_API char* mg_command_rtl(mg_core_t* core);
MGCORE_API char* mg_command_takeoff(mg_core_t* core, float altitude_m);
MGCORE_API char* mg_command_land(mg_core_t* core);
MGCORE_API char* mg_command_emergency_stop(mg_core_t* core);
MGCORE_API char* mg_command_guided_point(mg_core_t* core,
                                         double lat, double lon, float alt,
                                         float yaw, float speed);
MGCORE_API char* mg_command_guided_velocity(mg_core_t* core, float vx, float vy, float vz);
MGCORE_API char* mg_command_guided_pause(mg_core_t* core);
MGCORE_API char* mg_command_orbit(mg_core_t* core, const char* json_config);
MGCORE_API char* mg_command_set_roi(mg_core_t* core, double lat, double lon, float alt);
MGCORE_API char* mg_command_stop_roi(mg_core_t* core);
MGCORE_API char* mg_command_landing_gear(mg_core_t* core, bool deploy);

MGCORE_API char* mg_flight_get_available_modes(mg_core_t* core);
MGCORE_API char* mg_flight_get_mode_status(mg_core_t* core);
MGCORE_API char* mg_flight_get_mode_history(mg_core_t* core);
MGCORE_API char* mg_flight_get_takeoff_altitude(mg_core_t* core);
MGCORE_API char* mg_flight_set_takeoff_altitude(mg_core_t* core, float altitude_m);
MGCORE_API char* mg_flight_get_loiter_radius(mg_core_t* core);
MGCORE_API char* mg_flight_set_loiter_radius(mg_core_t* core, float radius_m);

MGCORE_API char* mg_preflight_check(mg_core_t* core);
MGCORE_API char* mg_battery_failsafe_configure(mg_core_t* core,
                                                int low_voltage_action,
                                                float low_voltage_threshold,
                                                float critical_voltage_threshold);
MGCORE_API char* mg_failsafe_get_status(mg_core_t* core);

#ifdef __cplusplus
}
#endif

#endif /* MGCORE_FFI_API_FLIGHT_H */
