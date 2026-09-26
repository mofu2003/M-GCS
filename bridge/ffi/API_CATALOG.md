# MgCore FFI API Catalog

`ffi_api.h` is the complete public C ABI. Every `char*` result, including an
event callback payload, is owned by the caller and must be released with
`mg_free_string()`.

## Stability Rules

- **Preferred**: available to new frontend and external consumers. Keep the
  JSON envelope and function signature stable within the current major ABI.
- **Compatibility**: exported for existing tools. Do not add new consumers or
  new behavior to these names. A replacement is listed where one exists.
- **Specialized**: public, but intended for a feature-specific client rather
  than a general-purpose client. Its lack of a first-party binding does not
  make it removable.

No compatibility symbol may be removed until a documented ABI-major release
and a migration note have been published.

## Preferred Entry Points

| Area | Preferred API |
| --- | --- |
| Lifecycle | `mg_create`, `mg_destroy`, `mg_free_string` |
| Connection | `mg_connection_open`, `mg_connection_get_session_state`, `mg_connection_close`, `mg_vehicle_select`, `mg_connection_get_diagnostics` |
| Session telemetry | `mg_vehicle_get_session_status` |
| Dashboard | `mg_telemetry_get_dashboard_snapshot`, `mg_system_get_diagnostics` |
| Long operations | `mg_operations_get_snapshot`, `mg_operations_cancel` |
| Parameters | `mg_params_request_all`, `mg_params_get_progress`, `mg_params_get_all`, `mg_params_get_by_name`, `mg_params_set`, `mg_params_prepare_change_set`, `mg_params_commit_change_set` |
| Parameter metadata/files | `mg_config_get_groups`, `mg_config_get_group_descriptor`, `mg_param_file_export`, `mg_param_file_import` |
| Calibration | `mg_calibration_start`, `mg_calibration_cancel`, `mg_calibration_get_status`, `mg_calibration_accel_confirm`, `mg_calibration_start_airspeed` |
| Flight commands | `mg_command_*` |
| Logging | `mg_log_request_list`, `mg_log_get_entries`, `mg_log_record_*` |

Existing first-party bindings deliberately cover connection, dashboard,
parameters, servo/RC telemetry, serial mapping, and calibration. Mission,
flight command, geofence, payload, logging, SITL, and firmware APIs are
specialized public surfaces that are not yet wired into every client workflow.

## Compatibility Entries

| Compatibility API | Replacement / note |
| --- | --- |
| `mg_param_list`, `mg_param_get`, `mg_param_set` | `mg_params_*`; `mg_param_list` historically starts a list request rather than returning a list. |
| `mg_cmd_*` | `mg_command_*`; the preferred family keeps raw MAVLink command inputs explicit. |
| `mg_cal_start`, `mg_cal_cancel`, `mg_cal_status`, `mg_cal_history`, `mg_cal_retry_info` | `mg_calibration_*`. |
| `mg_logging_get_entries`, `mg_logging_get_stats` | `mg_log_get_entries`, `mg_log_record_status`. |
| `mg_firmware_request_version`, `mg_firmware_get_version` | `mg_vehicle_request_firmware_version`, `mg_vehicle_get_firmware_version`. |
| `mg_fence_get`, `mg_fence_set` | `mg_geofence_get_config`, `mg_geofence_set_config`. |
| `mg_mission_request` | `mg_mission_download`. |

`mg_connection_open` accepts one explicit URI (`serial://`, `tcp://`, or
`udp://`) and returns a session/vehicle ID without changing active/UI focus.
Use `mg_connection_get_session_state` and `mg_connection_close` with that ID to
inspect and close exactly that session. These operations do not depend on
selection.

`mg_vehicle_get_session_status` reads GPS fix/satellite count, heading, battery
voltage/remaining percentage, and link heartbeat for one session without
changing active/UI focus. Telemetry fields are nullable when the backend cache
has no fresh sample; clients should display unavailable values instead of
reusing stale readings.

`mg_vehicle_open` and `mg_vehicle_close` are compatibility aliases;
`mg_vehicle_open` additionally accepts a bare serial device path.
`mg_connection_connect`, `mg_connection_disconnect`, and TCP/UDP convenience
helpers remain legacy single-active-session wrappers. The connect wrappers
select the session they create so existing active-vehicle commands continue to
work. New code should use the session-oriented `mg_connection_*` lifecycle.

## Adding or Retiring APIs

1. First reuse an existing preferred API or extend an aggregate snapshot.
2. Add the declaration to the matching `ffi_api_*.h`, implementation tests,
   and a catalog entry.
3. Preserve JSON envelope and ownership rules.
4. Mark replacements as compatibility before removal; do not delete symbols
   based only on current first-party usage.
