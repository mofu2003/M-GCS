# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

<!-- 下一轮开发的变更记录从这里开始；发布时整体迁入带版本号的段落。 -->

---

## [1.0.0] - 2026-10-13

**M-GCS 首个稳定版本，专注 ArduPilot 固件，提升核心稳定性。**

### Removed

- **PX4 firmware support**: All PX4-specific code is gone — firmware plugin, mode tables, parameter remapping, parameter metadata, sensor-ID decoding, and the seven `mg_px4_*` exports. The firmware adapter now recognises ArduPilot only.
- **Legacy features**: Survey planning, terrain elevation module, standalone geofence manager, and dead FFI exports. **Public exports drop from 280 to 154** (127 bound by Flutter).
- **FFI event callback system**: Removed `mg_set_event_callback`, `mg_event_callback_t`, and `EventDispatcher` — frontend polls instead (KD-27).
- **ArduPilot parameter prose**: Removed 11,655 verbatim GPLv3 strings from `ParameterDescriptors.json`, replaced with self-authored Chinese descriptions (95% coverage).
- **Stale documents**: Deleted `docs/plans/mgcs-development-plan-2026-10-10.md` (44 KB history), phase reports, and PX4 parity document.

### Added

- **Mission logging & file management**: Onboard DataFlash download with integrity check, `.tlog` recording & offline playback, MAVLink FTP file browser.
- **Async FTP**: File transfer moved to background thread with state polling (no UI blocking).
- **New documentation**: `docs/ROADMAP.md` replaces the 44 KB historical plan; `docs/CODEBASE-INVENTORY.md` lists all 154 public exports.
- **Test enhancements**: 34 core test suites + 258 Flutter tests; zero `TODO`/`FIXME` annotations.
  exposes them, and the transfer bar reports "（N 条提示：…）".
- **Mission file import in the UI**: `mg_mission_doc_import_format` was the one remaining
  half-wired exchange path — export had a Flutter sheet, import had none, so a `.waypoints`
  or `.plan` file could only be read by the native tests. The 航线规划 transfer bar now has a
  「导入文件」button next to 「导出文件」, backed by `MissionDocumentBackend.importMissionInterchange`
  and `MissionDocumentController.importInterchange`. It reads the chosen file and replaces the
  current document; uploading to the flight controller stays a separate, confirmed step, and a
  missing file reports an inline error without touching the document. Bound exports 126 → 127.

### Changed

- **`ffi-contract.md` F2 rewritten**: the old text said "独立能力不得因未被 Flutter 调用而删除",
  which contradicted this release's whole approach. F2 now states the four ways an export can
  have a consumer, requires deleting any export with none, names "we might need it later" as an
  invalid excuse, and keeps the ABI/registry/CHANGELOG obligations for deletions.

### Fixed

- **Calibration result text matching (KD-54)**: the five calibration strategies each carried
  their own STATUSTEXT interpretation and disagreed with one another. `radio_calibration.cc`
  matched bare substrings (`"success"`, `"complete"`), so the flight controller's
  `"RC calibration unsuccessful"` was read as **success**; `compass_calibration.cc` tested for
  success before failure and had a narrower failure vocabulary, so
  `"Compass calibration unsuccessful"` matched neither branch and the calibration hung in
  progress forever. The two predicates now live once in
  `core/src/calibration/strategies/statustext_match.h` (whole-phrase completion, failure words
  checked first), the three verbatim copies in gyro/baro/accel are gone, and
  `test_calibration_strategy_result_semantics` pins all five types.
- **Telemetry callbacks held a raw `ConnectionManager*` (KD-57)**: under a shared link,
  `remove_shared_route()` drops the route's manager from `connections_` while the adapter can
  still be held by a lease and keep dispatching — the callback then dereferenced freed memory
  after its `Vehicle` lock succeeded. `wire_telemetry_callbacks_for()` now takes the
  `shared_ptr` and captures a `weak_ptr`; each callback locks it after the `Vehicle` and returns
  early when the route is gone.
- **`state_string(...) == "disconnected"` compared pointers, not text**: a test assertion in
  `test_ffi_comprehensive.cc` relied on string-literal merging to pass. Now compares
  `std::string` contents — the kind of always-true check T2 exists to prevent.
- **Ten inert `#define CJSON_NESTING_LIMIT`**: cJSON is a separate translation unit, so these
  definitions never took effect; two of them also emitted a macro-redefinition warning.
  All removed.

### Removed (round 5 of the dead-code sweep)

- **`FlightLogWriter`** (`core/settings/flight_log_writer.{h,cc}`): the whole class, 11 methods;
  its only reference was an `mg_core_t` member that nothing ever called.
  `FlightLoggingService` is the one the FFI actually uses.
- **Unused functions with no caller anywhere (28)**: `MissionItemFactory::create_waypoint` /
  `create_rtl` / `create_speed_change` / `create_camera_trigger`;
  `FlightModeManager::is_armed` / `decode_mode_name` / `set_takeoff_altitude` /
  `takeoff_altitude` / `set_loiter_radius` / `loiter_radius`;
  `MavlinkAdapter::on_distance_sensor` / `on_video_stream_info` / `param_refresh_status` /
  `camera_information` / `gimbal_manager_information` / `rally_get_plan` / `fence_get_plan` /
  `video_request_stream_info` / `cmd_roi_stop`; `FlightLogReplayer::replay_all`;
  `MultiVehicleManager::init`; `FlightLoggingService::set_write_callback`;
  `SerialPortDiscovery::refresh` / `best_candidate` / `is_port_available` / `filter_ports`.
  With `filter_ports` gone the `PortFilter` enum went too, and `enumerate_ports()` lost its
  cache — every caller builds a short-lived object, so the cache never hit and its only
  invalidator was the dead `refresh()`.
- **Dead data members**: `VehicleInfo`'s five sensor-name strings, `link_config.h`'s
  `high_latency`, `MavlinkFtpClient::download_path_`, `mg_core_t`'s `core_mtx` / `log_events` /
  `kMaxLogEvents`, `flight_time_s_` (written but never read anywhere),
  `StatustextInfoCallback`, `JsonBuilder::append_object`, `PlatformMutex::owns_lock`.
- **`MultiVehicleManager`'s vehicle-removed / active-vehicle-changed callbacks**: the setters
  had no caller, so the callbacks were permanently empty — the four dispatch sites that copied
  and invoked them are gone too.
- **`mission_item_factory.{h,cc}` → `geo_math.{h,cc}`**: with all four builders deleted the
  struct was a "factory" with no factories, holding only `haversine_distance`. Renamed to match
  what it is, and that function is now a free function.
- **Dangling comments and stale text**: `ffi_api_flight.h` carried four comment blocks for
  exports that no longer exist (mode mapping, flight termination, landing gear, a duplicate
  mode-status note); `ffi_commands.cc`'s header still listed Survey, Terrain and SITL;
  `ffi_config_extra.cc` still documented `mg_config_write_params`.
- **`tools/audit_status.sh`**: superseded by `check_ffi_contract.py --write-catalog`, which is
  also the preflight gate. `ffi-contract.md` now points at the real generator. Three zombie
  entries for the deleted `param_file_io.cc` were dropped from `firmware_literals_baseline.json`.
- **Test-local dead code**: three unused helpers in `test_mission_document.cc`, `ok_data` in the
  SITL scenario tool, `call_json_int` in two hardware tools, three unused locals in
  `test_param_descriptors.cc`, `kFtpPayload` in the mock flight controller.

**Deliberately kept**: unused members of domain enumerations (`event_codes.h` codes,
`mavlink_ftp.h` protocol opcodes/results, `calibration_types.h` calibration commands,
`ModeResult`, `CoordFrame`, `NavCommand`). An enum value having no current call site does not
make it junk; deleting members of a taxonomy trades documented domain facts for bare numbers.
`event_codes.h` is append-only by contract in any case — the header states the numbers appear in
event JSON, so removing a middle entry would renumber everything after it.


- **Architectural risks eliminated** (Phase 1 of quality hardening):
  - **KD-49 (ABBA deadlock risk)**: `deliver_frame()` now copies `send_cb` under lock and invokes it 
    outside all locks, eliminating potential ABBA deadlock between `send_mtx` and external locks. 
    ADR-0007 defines lock ordering (`mtx → send_mtx`) and callback execution rules.
    (`core/src/mavlink/mavlink_adapter_internal.h`, `docs/adr/0007-send-mutex-and-ownership-model.md`)
  - **KD-52 (use-after-free risk)**: `wire_callbacks()` now captures `MavlinkAdapter` via `weak_ptr` 
    instead of raw pointer, eliminating use-after-free when callbacks outlive the adapter. ADR-0007
    defines the ownership model (`ConnectionManager` as sole owner).
    (`core/src/transport/connection_manager.cc`, `docs/adr/0007-send-mutex-and-ownership-model.md`)
  - **Concurrency regression tests**: Added `test_mavlink_send_callback_reentrancy.cc` to verify 
    callback reentrancy and concurrent operations do not deadlock. (3/3 tests passing)
    (`core/tests/test_mavlink_send_callback_reentrancy.cc`)

### Documentation

- **ADR renumbering**: the send-mutex ADR is now ADR-0007 and the video decode backend ADR is
  ADR-0008. The PX4 parameter metadata and ArduPilot-first ADRs are deleted along with PX4.

## [0.2.2] - 2026-10-12

### Fixed

- **Concurrent test file collision**: test_new_modules.cc now generates per-process unique names
  for tlog temporary files (`mgcs-<pid>-test_replay.tlog`), eliminating file overwrites when
  running tests in parallel (ctest -j4). Similarly, calibration_service.cc's test-build history
  path (`M-GCS-test-<pid>/`) now avoids collision with concurrent CalibrationService instances.
  (`core/tests/test_new_modules.cc`, `core/src/calibration/calibration_service.cc`,
  `core/tests/test_sim_calibration.cc`)
- **Null pointer dereference on disconnected vehicle** (from DeepSeek's t14 work, verified this
  release): `Vehicle::conn()` now returns a fallback `detached_conn_` manager when all routes are
  disconnected, instead of dereferencing a null pointer. The fallback reports the disconnected
  baseline (state Disconnected, no link, no adapter), so readers stay safe without lifetime checks.
  Crash scenario was `mg_connection_disconnect()` followed by `mg_connection_get_state()` on a core
  with no active vehicle, which triggered EXC_BAD_ACCESS @ ConnectionManager::state_ (offset 0x74).
  (`core/include/core/vehicle/vehicle.h`, `core/tests/test_ffi_comprehensive.cc`)

### Verified

- Concurrent stability: ctest -j4 × 3 runs = 39/39 ✓
- Full backend gate: 39/39 tests + FFI contract + scripts ✓
- Crash regression test: vehicle disconnect path now safe ✓

## [0.2.1] - 2026-10-11

### Added

- FFI export-debt gate (`tools/check_ffi_contract.py`, `tools/ffi_unbound_baseline.json`): the
  contract check now also freezes every public export that has no Dart binding, no classification
  and no test reference. The baseline may only shrink — a new unclassified export, or a symbol
  that is now bound/tested/classified but still listed, fails the gate. The gate now supports two
  classification tables, `INTERNAL_SYMBOL_NOTES` (internal capability) and the new
  `SUPERSEDED_SYMBOL_NOTES` (superseded legacy path, decommission candidate); `--write-baseline`
  writes atomically. This pass classified 48 exports (17 internal + 31 superseded), dropping the
  debt from 58 to 20 exports (KD-45).
- `docs/features/feature-inventory.md`: feature inventory split into implemented, partial,
  unimplemented and not-yet-wired-to-frontend, with a cluster-by-cluster explanation of the 178
  unbound FFI exports and their disposition.

### Changed

- Architecture-health maintenance pass: registered KD-45 (FFI export debt), KD-46
  (`flutter_shell/lib/map/amap_map_view.dart` reviewed under the change-coupling rule and kept
  as-is) and KD-47 (`JsonMap` typedef placement accepted), tightened the A7 verification note,
  and documented the maintenance-time review signals in `docs/development.md`. `__pycache__/` is
  now git-ignored.

### Removed

- Dead-code cleanup: deleted historical FFI exports that had no caller and no future use,
  shrinking the public ABI from 314 to 280 declarations (KD-45, `docs/features/feature-inventory.md`).
  Removed: the old `MissionManager` read-modify-transfer path (`mg_mission_add_item`/`update_item`/
  `remove_item`/`move_item`/`validate`/`get_stats`/`export_json`/`import_json`/`export_csv`/
  `import_csv`, plus `mg_mission_start` and `mg_mission_get_templates`), the old Geofence/Rally
  direct-edit exports (`mg_geofence_add_point`/`remove_point`/`get_points`/`validate`/`set_config`/
  `import_json`/`export_json`/`export_kml`, `mg_fence_clear`, `mg_rally_clear`), the old connection
  entries (`mg_connection_connect_tcp`, `mg_send_heartbeat`, `mg_stream_request`), the old serial
  config exports (`mg_serial_get_config`/`set_baud`/`set_protocol`), the old RC-specific calibration
  exports (`mg_rc_calibrate_start`/`status`/`save`), and `mg_params_get_changed` with
  `mg_params_save_to_file`/`load_from_file`. All 34 names are frozen in `RETIRED_SYMBOLS` so they
  cannot silently return; `tools/ffi_unbound_baseline.json` shrank from 20 to 19 and
  `tools/check_ffi_contract.py` `SUPERSEDED_SYMBOL_NOTES` is now empty. Files deleted:
  `bridge/ffi/ffi_mission_management.cc`, `bridge/ffi/ffi_param_file.cc`,
  `core/mission/mission_manager.{h,cc}`, and the `mg_rc_calibrate_save` test; the still-used
  mission-item builders were extracted to `core/mission/mission_item_factory.{h,cc}`.

---

## [0.2.2] - 2026-10-12

机载日志下载、飞行记录回放、MAVLink FTP 文件浏览等功能完成。详见 Git 历史。

## [0.2.1] - 2026-10-11

FFI 导出债务门禁、特征库存清单等完成。详见 Git 历史。

## [0.2.0] 及之前

早期开发版本。详见 Git 历史与已关闭的 ADR 记录。
