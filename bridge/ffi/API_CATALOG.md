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
| Map position | `mg_vehicle_get_global_position`（WGS84 经纬度/海拔/航向）、`mg_vehicle_get_home_position`（Home 点）。地图用它画 Home 与飞机标记 |
| Sensor waveform | `mg_waveform_poll`, `mg_waveform_clear`, `mg_waveform_get_instances` |
| Long operations | `mg_operations_get_snapshot`, `mg_operations_cancel` |
| Parameters | `mg_params_request_all`, `mg_params_get_progress`, `mg_params_get_all`, `mg_params_get_by_name`, `mg_params_set`, `mg_params_prepare_change_set`, `mg_params_commit_change_set` |
| Parameter write (UI) | `mg_config_write_param`, `mg_config_write_params`。带描述库校验的前端写入入口；数值越界或不合法会返回 `success:false`。相比 `mg_params_set` 的差别是不要求参数表已完整下载，缺失的类型回退到 REAL32 |
| Parameter metadata/files | `mg_config_read_param`, `mg_config_get_groups`, `mg_config_get_group_descriptor`, `mg_param_file_export`, `mg_param_file_import` |
| Calibration | `mg_calibration_start`, `mg_calibration_cancel`, `mg_calibration_get_status`, `mg_calibration_accel_confirm`, `mg_calibration_start_airspeed`. `mg_calibration_start` 的 `type` 取 0=gyro、1=accel、2=compass、3=baro、4=radio（遥控器行程校准，MAV_CMD_PREFLIGHT_CALIBRATION param4=1）。`mg_calibration_get_status` 的返回里 `progress` 是所有罗盘的最大值，`compass_progress` 是分罗盘数组 `[{compass_id, completion_pct}]`，供界面按识别到的罗盘数量各画一条进度条 |
| Flight commands | `mg_command_*` |
| Logging | `mg_log_request_list`, `mg_log_get_entries`, `mg_log_record_*` |
| Mission planning document | `mg_mission_doc_create`, `mg_mission_doc_apply`, `mg_mission_doc_validate`, `mg_mission_doc_compile`, `mg_mission_doc_upload`, `mg_mission_doc_verify` |
| Mission command metadata | `mg_mission_get_command_catalog` |

Existing first-party bindings deliberately cover connection, dashboard,
parameters, servo/RC telemetry, serial mapping, calibration, and the
real-time waveform buffer. Mission,
flight command, geofence, payload, logging, SITL, and firmware APIs are
specialized public surfaces that are not yet wired into every client workflow.

`mg_waveform_poll` is the one entry point that writes into caller-owned memory
instead of returning JSON: flight-controller waveforms are large and would
otherwise be re-parsed on every UI frame. The caller allocates
`out_max_points * 10` floats and receives the number of points written, with
`window_ms` selecting how much history to return (`<= 0` means the whole
buffer) and `instance` selecting which IMU's buffer to read (0 is IMU1).
Point layout is
`[relative_ms, accel_x, accel_y, accel_z, gyro_x, gyro_y, gyro_z, mag_x, mag_y, mag_z]`;
all nine axes of one point come from the same IMU sample, so the client never
has to time-align them. When the selected
window holds more frames than `target_points`, values are decimated with a
per-bucket min/max envelope so spikes survive, and the buckets are aligned to
an absolute time grid so an existing point never moves sideways when new data
arrives. Accelerometer is m/s², gyro rates are degrees per second, and
magnetometer is gauss.
`mg_waveform_get_instances` reports which instances the link is actually
carrying, so a client can build one selector per detected IMU and compass:
`{"imu":[{"index","active","age_ms","name"}],"mag":[…]}`. Detection follows
received data rather than firmware parameters — `SCALED_IMU`, `SCALED_IMU2`
and `SCALED_IMU3` are the three per-instance messages, and an instance counts
as a magnetometer source once it reports a non-zero field vector. Index 0 is
always listed so the client has a default before any data arrives.
`mg_waveform_clear` drops the buffered history (also done automatically when a
session connects or disconnects).

## Mission Planning Document

The mission planning document is the preferred offline editing model. It is
separate from the compiled MAVLink `MissionPlan`: editing, validation, undo,
redo, save, and load never write to the vehicle.

Lifecycle:

```text
mg_mission_doc_create/get
    -> mg_mission_doc_apply (local revision-checked edits)
    -> mg_mission_doc_validate
    -> mg_mission_doc_compile
    -> mg_mission_doc_upload (explicit write)
    -> mg_mission_doc_verify (compare vehicle readback)
```

`mg_mission_doc_apply` accepts one operation per call. Supported operations
are `add`, `update`, `remove`, `move`, `clear`, and `replace`. Except for
`replace`, mutation operations use `entity` or `entity_id` and may include
`expected_revision`; a stale revision is rejected.

The document JSON schema is versioned by `schema_version`. The source
document contains:

- `metadata`: id, name, description, firmware profile, vehicle type.
- `home`: optional home latitude, longitude, and altitude.
- `defaults`: frame, altitude, and speed used when an entity omits them.
- `revision`: monotonically increasing local revision.
- `entities`: ordered mission entities with a stable `id`, command, frame,
  coordinates, altitude, four command parameters, and enabled state.

`mg_mission_doc_export` returns the document JSON for `.mgcad.json` storage.
`mg_mission_doc_import` loads that JSON without uploading to a vehicle.
`mg_mission_doc_save/load` provide the same operation through a filesystem
path.

`mg_mission_doc_compile` returns `valid`, the compiled `MissionPlan`, a
source-entity-id list, and structured diagnostics. `mg_mission_doc_upload`
requires the expected document revision, compiles locally, rejects validation
errors, and then starts the existing mission transfer. ArduPilot home-item
injection remains in the existing transport adapter.

`mg_mission_doc_verify` compares the compiled document with the current
vehicle mission after a completed full download. Calling verify after upload
but before download returns `invalid_state`. It returns `matches` plus structured
differences for item count, command, frame, position, altitude, parameters,
`autocontinue`, and `mission_type`.

Upload is start-only, for both `mg_mission_doc_upload` and the compatibility
entry `mg_mission_upload`: a successful return means the transfer was started,
not that the flight controller finished receiving it. `mg_mission_doc_upload`
reports `state: "uploading"` for that reason. Poll `mg_mission_get_progress`
until it reports completion, then call `mg_mission_doc_verify` to compare the
readback. Treating the upload return value as completion is a bug.

`mg_mission_get_command_catalog` exposes command names, categories, whether a
position or altitude is required, and per-parameter MAVLink index, labels,
units, types, defaults, ranges, required flags, and enum options. Parameter
`index` maps directly to `params[index - 1]`. Frontends should render command
forms from this catalog rather than hard-coding MAV_CMD parameter semantics.

`survey_grid` and `corridor` entities are parameterized: the document stores the
intent (`survey.width_m`, `survey.length_m`, `survey.angle_deg`,
`survey.cross_grid`, `survey.end_lat` / `survey.end_lon`,
`survey.corridor_width_m`, camera and overlap fields) and `mg_mission_doc_compile`
expands them into waypoints through the existing `SurveyCalculator`. Every
expanded item maps back to the same source entity in `source_entity_ids`, so a
client renders one row per survey block rather than one per generated waypoint.
See `docs/MISSION_PLANNING.md` for the full example.

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
| `mg_mission_add_item`, `mg_mission_update_item`, `mg_mission_remove_item`, `mg_mission_move_item` | `mg_mission_doc_apply`; legacy APIs mutate and upload immediately instead of editing an offline document. |
| `mg_mission_validate`, `mg_mission_get_stats` | `mg_mission_doc_validate`, `mg_mission_doc_estimate`; new APIs validate the source document and return structured diagnostics. |
| `mg_mission_export_json`, `mg_mission_import_json`, `mg_mission_export_csv`, `mg_mission_import_csv` | `mg_mission_doc_export`, `mg_mission_doc_import`, `mg_mission_doc_save`, `mg_mission_doc_load`; the new format preserves revision and parameterized intent. |
| `mg_log_list` | `mg_log_get_entries`. Both now read the same log-entry cache; `mg_log_list` wraps it in `{success, logs, total}`. |
| `mg_rc_calibrate_save` | No replacement is needed: ArduPilot and PX4 persist `RCn_MIN/TRIM/MAX` when RC calibration finishes. The entry remains for ABI compatibility and now refuses while an RC calibration is still running instead of reporting a false success. |

### Deprecation policy

Compatibility entries stay exported so existing binaries keep linking; they are
frozen rather than removed. Concretely:

- No new behavior is added to a compatibility name. Bug fixes that make a
  compatibility entry stop reporting false success are allowed, and are called
  out in the `CHANGELOG.md` entry of the release that ships them.
- New consumers must use the replacement listed above.
- Removal happens only at an ABI-major release with a migration note. Until
  then, `MGCORE_API` keeps the plain C signature; no compiler `deprecated`
  attribute is applied, because that would emit warnings in consumers that are
  already pinned to the old name.

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
