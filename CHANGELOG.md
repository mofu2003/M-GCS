# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

<!-- 下一轮开发的变更记录从这里开始；发布时整体迁入带版本号的段落。 -->

### Added

- FFI export-debt gate (`tools/check_ffi_contract.py`, `tools/ffi_unbound_baseline.json`): the
  contract check now also freezes every public export that has no Dart binding, no classification
  and no test reference. The baseline may only shrink — a new unclassified export, or a symbol
  that is now bound/tested/classified but still listed, fails the gate. The gate now supports two
  classification tables, `INTERNAL_SYMBOL_NOTES` (internal capability) and the new
  `SUPERSEDED_SYMBOL_NOTES` (superseded legacy path, decommission candidate); `--write-baseline`
  writes atomically. This pass classified 48 exports (17 internal + 31 superseded), dropping the
  debt from 58 to 20 of 314 exports (KD-45).
- `docs/features/feature-inventory.md`: feature inventory split into implemented, partial,
  unimplemented and not-yet-wired-to-frontend, with a cluster-by-cluster explanation of the 178
  unbound FFI exports and their disposition.

### Changed

- Architecture-health maintenance pass: registered KD-45 (FFI export debt), KD-46
  (`flutter_shell/lib/map/amap_map_view.dart` reviewed under the change-coupling rule and kept
  as-is) and KD-47 (`JsonMap` typedef placement accepted), tightened the A7 verification note,
  and documented the maintenance-time review signals in `docs/development.md`. `__pycache__/` is
  now git-ignored.

## [0.2.0] - 2026-10-11

### Added

- Semantic parameter roles (safety.md S6, KD-42 closed): the front end no longer hard-codes
  firmware parameter names. The plugin now ships a role table
  (`core/src/config/ParameterDescriptors.json` `roles` section; `ardupilot` base plus
  `copter`/`plane`/`vtol`/`rover`/`sub`/`tracker` profiles inheriting through `extends`) that maps
  firmware-independent roles such as `pid.rate.roll.p`, `rc.map.yaw`, `rc.channel_option`,
  `flight_mode.slot`, `arming.skip_check`, `compass.cal_fit`, `takeoff.vtol_alt_m`,
  `loiter.radius` and `tecs.time_const` to the actual names for the connected build. It is exposed
  as `mg_config_get_param_roles` (`bridge/ffi/ffi_api_configuration.h`), bound to Dart as
  `ParameterFfiBackend.paramRoles()` and resolved in
  `ConnectionController.roleParam(role, {index})`; the PID, calibration, compass, flight-mode, RC
  calibration and arming-check pages read parameters through roles, and a missing role degrades to
  a disabled control instead of a guessed name. `tools/check_firmware_literals.py` keeps its
  non-empty baseline mechanism as the S6 gate, and `tools/firmware_literals_baseline.json` is
  now `{}` — the 34 ArduPilot literals inherited from `origin/main` are gone.
- Asynchronous MAVLink FTP: `mg_fs_list_directory` / `mg_fs_download_file` / `mg_fs_upload_file`
  now start a background transfer and return immediately instead of running the whole transfer on
  the FFI calling thread. `mg_fs_transfer_status` polls `task`/`state`/`active`/`completed`/
  `cancelled`/`received_bytes`/`total_bytes`/`error_code`/`message` (plus an `items` array for a
  directory listing) and stays available after a disconnect, and `mg_fs_cancel` cancels the active
  transfer (`MG_ERROR_NOT_SUPPORTED` when idle). `MavlinkFtpClient` gained
  `start_list_directory`/`start_download_file`/`start_upload_file`/`transfer_status`/
  `transfer_active`/`wait_for_completion`/`cancel`/`shutdown`, matching the `mg_onboard_log_*`
  async pattern.
- Onboard file browser page (`flutter_shell/lib/logging/file_browser_page.dart`,
  `file_controller.dart`, `file_models.dart`, `file_index.dart`, `file_backend.dart`,
  `file_ffi_backend.dart`; opened from the settings menu → configuration workspace `files`): browse
  the vehicle's MAVLink FTP filesystem with a path breadcrumb and a common-path shortcut row
  (`@SYS`, `/APM/LOGS`, `/`), list a directory, descend into subdirectories, download a file with a
  progress bar, cancel the active transfer, or upload a local file, and see the saved path or a
  definite failure/disconnect message. Results are bound to the session that started them so a
  vehicle switch discards stale listings and transfers.
- Local file index and cache (`FileIndexStore`, `files/file_index.json`, `schema_version: 1`):
  caches the last directory listings and completed downloads so the page shows the last known state
  before the first reply and reuses an already-downloaded copy when the remote size still matches. A
  corrupt index falls back to an empty one and a write failure only sets `writeFailed`, so a broken
  cache never blocks browsing. The Flutter suite is at 247 passing / 4 skipped.
- Shared `MgInlineNotice` / `MgDismissButton` widgets (`flutter_shell/lib/widgets/mg_inline_notice.dart`)
  replace the notice/dismiss presentation that the onboard-log and flight-log pages each duplicated.
- Flight log page (`flutter_shell/lib/logging/tlog_*.dart`, `flight_log_page.dart`, opened from
  the settings menu → configuration workspace `flightLog`): toggle link-layer `.tlog` recording
  (refused while disconnected, byte count polled every second), list local `.tlog` files in the
  default log directory, open a file for step-wise offline replay with progress and a debounced
  seek slider, filter by message name or numeric msgid, keep the newest 5000 messages, and surface
  a truncated-tail warning.
- Offline `.tlog` replay FFI: `mg_tlog_replay_open/step/seek/close` (`bridge/ffi/ffi_api_logging.h`,
  `ffi_log_commands.cc`) expose the previously-internal `FlightLogReplayer`. `open` scans the file
  and reports message count/time range/`truncated`; `step` reads up to 200 messages (cap 2000) and
  reports `messages/count/progress/last_timestamp_us/eof/truncated`; `seek` jumps by file ratio and
  returns `NOT_FOUND` out of range. Message names come from `mavlink_message_names.{h,cc}`
  (`mavlink_get_message_info`), unknown ids fall back to `UNKNOWN`.
- Single `.tlog` disk-format contract (`core/include/core/log/tlog_format.h`): no file header, each
  record is `[8-byte big-endian Unix microseconds][complete MAVLink frame]`, matching QGroundControl
  and Mission Planner; the writer and the replayer now share it.
- End-to-end SITL mission scenario with a reproducable evidence record
  (`tools/sitl-mission-scenario.sh`, `tests/manual/test_sitl_mission_scenario.cc`,
  `docs/sitl-scenario-evidence.md`, `docs/sitl-scenario-evidence-plane.md`): connect, identify
  firmware/vehicle, build and validate a document, upload, wait for `MISSION_ACCEPTED`,
  read-back verify, disconnect, reconnect, rebuild the document from the vehicle and verify
  again. Run against `arducopter --model quad` and `arduplane --model plane`, 20 assertions each.
- Mission command parameters now carry a per-vehicle applicability mask
  (`MissionCommandParameter::vehicles`, `mission_command_parameter_applies`): a field the flight
  controller does not store for the connected build is reported as a read-back normalization
  instead of a difference.
- Mission file format contract for the native document, `.waypoints` and QGC `.plan`
  (`docs/mission-format-contract.md`): units, altitude frames, schema versioning and the
  round-trip rules for data M-GCS does not model natively.
- Onboard DataFlash log page (`flutter_shell/lib/logging/`, opened from the settings menu →
  configuration workspace `logs`): refresh the log list, download one entry at a time with a
  progress bar, cancel, and see the saved path or a definite failure/disconnect message. The list
  is collected by `expected`/`complete` (from `LOG_ENTRY.num_logs`, with an entry-count fallback),
  sorted newest first, and download results are bound to the session that started them so a
  vehicle switch discards stale state. The `mg_onboard_log_*` exports are now Dart-bound and
  `docs/capabilities.md` moves `ffi_api_logging.h` from 待接入 to 部分接入.
- `mg_onboard_log_get_entries` now returns `expected` (`LOG_ENTRY.num_logs`) and `complete`, and
  `handle_log_entry` de-duplicates by log id.
- Onboard log erase: `mg_onboard_log_erase` sends `LOG_ERASE` and is refused while a download is
  active; the log page gains a confirmed 「擦除全部」 action that clears the local list.
- Log download integrity reporting: `mg_onboard_log_download_status` now carries `size_declared`
  (whether `LOG_ENTRY` provided the size), and the page marks a completed download that could not
  be checked against a declared size instead of implying a verified file.

- PX4 parameter metadata (ADR-0007): `core/src/config/PX4ParameterMetaData.json` (1836
  parameters, verbatim QGroundControl snapshot `bf0096d97`, sourced from PX4-Autopilot BSD-3),
  loaded as its own `px4` profile by `ParamDescriptors` so the parameter page shows the
  snapshot's groups, names, units and enum labels instead of bare names. A `px4` profile never
  falls back to an ArduPilot default descriptor (so e.g. `BATT_MONITOR` does not inherit the
  ArduPilot display name), only a parameter with both `min` and `max` gets a soft range, and
  uncovered parameters keep the honest `found=false` degradation. `ffi_param_profile()` now
  returns `px4` for PX4 while the mission-capability gate that reused it was split into
  `ffi_ardupilot_vehicle_class()`; the data file is added to the CMake, Linux/Windows/macOS/Android
  bundle manifests and the APK asset assertion. A license/source note lives in
  `core/src/config/PX4ParameterMetaData.LICENSE.txt`.
- PX4 mission command parity (S4): `mg_mission_command_catalog` now hides the parameters that
  QGroundControl removes for PX4 (`PX4-MavCmdInfoCommon.json` `paramRemove`: `NAV_WAYPOINT` params
  2/3, `NAV_LAND` param 1, `VIDEO_START_CAPTURE` param 2) through a profile-aware
  `mission_command_parameter_visible()`. Every catalog command now carries a `vehicles`
  array; `mg_mission_get_command_catalog` never emitted that field before even though the Dart
  model and `filterCommandCatalogForVehicle()` already read it, so the frontend's per-vehicle
  command filtering was silently a no-op on every vehicle. `mission_command_uses_location(command, firmware_profile)` gives `DO_LAND_START`(189) no
  coordinates under PX4 (QGC marks it `specifiesCoordinate:false`, while ArduPilot's
  `AP_Mission::stored_in_location()` includes it), wired through the mission compiler, document
  import, QGC `.plan` import/export and the JSON serializers; the single-argument overload keeps
  the ArduPilot semantics so firmware-agnostic callers are unchanged. Home semantics needed no
  change: M-GCS prepends a Home item only for ArduPilot, matching QGC's
  `sendHomePositionToVehicle() == false` for PX4. `DO_SET_MISSION_CURRENT` version gating is
  deliberately not implemented because M-GCS already sends `MISSION_SET_CURRENT`, which is what
  QGC falls back to.
- The seven `mg_px4_*` configuration exports (actuator mapping, `COM_FLTMODE1..6` slots, safety
  circuit breakers, PID tuning) were classified as **FFI-internal capabilities** (S5): they stay
  available to native tooling and tests but get no Dart binding and no separate UI page, because
  the generic parameter page (`mg_params_*`/`mg_config_*`) with the `px4` descriptor profile from
  ADR-0007 already reads and writes the same flight-controller parameters with groups, ranges and
  enum labels. `docs/capabilities.md`, `bridge/ffi/API_CATALOG.md` and
  `tools/check_ffi_contract.py` (new `INTERNAL_SYMBOL_NOTES` table plus guards that fail when an
  internal-only symbol becomes bound in Dart or when a note references a missing export) record the
  identity so no public symbol is left unexplained.
- Camera & gimbal page (`flutter_shell/lib/payload/` — `payload_models.dart`,
  `payload_backend.dart`, `payload_ffi_backend.dart`, `payload_controller.dart`,
  `camera_gimbal_page.dart`; opened from the settings menu → configuration workspace `payload`,
  direction 4 S2): list the cameras/gimbals discovered from `mg_camera_get_status` /
  `mg_gimbal_get_status`, pick one by MAVLink component id (「自动（第一台）」 → `id<=0`), show
  model/resolution/mode/capture state/storage and gimbal capability range + attitude, and send
  take-photo / start-stop-video / stop-capture / zoom / mode / gimbal point-recenter commands. A
  command's success wording is 「已发送」 (the export has no ACK); disconnect, failure and
  "queried, no device" are worded distinctly. The `ffi_api_payload.h` exports are now Dart-bound
  (17 of 19) and `docs/capabilities.md` moves it from 待接入 to 部分接入. Video stays
  metadata-only and says so, because local decoding/rendering is slice S3.
- Local video decode pipeline (direction 4 S3, `docs/adr/0008-video-decode-backend.md`; ADR-0008 is
  the D3 decision): a new native `core/include/core/video/video_decoder.h` +
  `core/src/video/video_decoder.cc` (`VideoFrameBuffer` = bounded latest-frame slot with a
  sequence number, `VideoDecoder` = one optional source) replaces the dead `ffmpeg_decoder.{h,cc}`
  (no call sites, an empty shell under `MGCORE_HAS_FFMPEG=OFF`). Two payload exports are added:
  `mg_video_renderer_status` (`{success,available,backend,running,uri,has_frame,width,height,pts_us,sequence}`)
  and `mg_video_poll_frame` (`>0` bytes / `0` no frame / `-1` capacity error, reporting the required
  size). `mg_video_start` now means start the local source (no backend → `NOT_SUPPORTED`, empty URI
  → `INVALID_PARAM`, open failure → `SEND_FAILED`) and no longer fires the camera-record command;
  `mg_video_status.playing` is true only while the local decoder is running with a new frame. The
  Flutter side gains `payload/video_preview.dart` (33 ms polling only while running, sequence
  de-duplication, `ui.decodeImageFromPixels` → `RawImage`) and a rewritten video panel that shows
  「本平台未启用本地视频解码（此构建未包含 LGPL FFmpeg 后端）」 when there is no backend instead of
  faking playback. Backend choice is FFmpeg under LGPL only (`--disable-gpl`); frames cross FFI as a
  copy (no platform texture). Frame format is RGBA8888. A local LGPL FFmpeg 7.1.1 build now plays a real TCP/MPEG-TS source
  end-to-end on macOS (arm64). Local recording and frame capture build on this pipeline (S4,
  below).

- Local recording and frame capture on top of the S3 video pipeline (direction 4 S4,
  `docs/features/video-camera-gimbal.md` §3.4): four payload exports —
  `mg_video_start_recording`, `mg_video_stop_recording`, `mg_video_recording_status` and
  `mg_video_capture_frame` — enable, disable, poll and snapshot the running local decoder.
  Recording copies the decoded stream into a container chosen by the file extension (the UI
  writes `.mkv`, i.e. Matroska) with no re-encode, synthesising a frame-rate time base when the
  source carries no timestamps; `mg_video_recording_status` always returns a success envelope
  with `available/recording/path/error/bytes/duration_us` and keeps the final result after a
  stop, an empty path is `INVALID_PARAM`, no backend is `NOT_SUPPORTED`, no running source or no
  frame yet is `INVALID_STATE`, and stopping while idle is an idempotent success.
  `mg_video_capture_frame` writes the latest frame as PNG (RGBA) or JPEG (YUVJ420P). Container
  and handle setup, the trailer write and the close all run outside the decoder lock so the poll
  path is never blocked. The Flutter side gains `payload/video_storage.dart`
  (`VideoStorageSettings` persisted as JSON in the app-support directory with a temp-directory
  fallback, `enableStorageLimit` default off, `maxVideoSize` default 10240 MB and floored at
  100 MB, `listRecordings` / `pruneOldRecordings` deleting oldest-first while never deleting the
  file just written), the `PayloadController.startLocalRecording` / `stopLocalRecording` /
  `captureLocalFrame` / `refreshRecordingStatus` actions (which, like starting the local video,
  do not require a vehicle connection) and a recording section on the camera & gimbal page with
  a directory field, the size-limit toggle, start/stop and a snapshot button; the controls are
  not drawn when the build has no video backend. Recording and capture were verified against a
  real TCP/MPEG-TS `testsrc` stream on macOS (arm64) with the LGPL FFmpeg 7.1.1 build: the
  Matroska output probes as mpeg4 640x480 at ~5.1 s with a real byte count, and the captured
  640x480 PNG content was checked visually. Flutter suite is 247 passing / 4 skipped;
  `tools/check_ffi_contract.py` reports 314 declarations / 136 bindings and `tools/preflight.sh`
  passes end to end. Recording and playback are demonstrated on one platform, so the S3/S4
  "two platforms" exit condition is still open (see R6 for the FFmpeg packaging work).

- Emergency stop entry in the top status bar: forced disarm behind the S3 confirmation component,
  shown only while armed so it stays reachable in flight.
- `docs/capabilities.md`: module-level delivery status (delivered / partial / pending UI /
  internal) for all 305 public FFI exports, generated from the headers and Dart bindings and
  gated by `tools/check_ffi_contract.py`, so the coverage counts cannot silently drift.
- `docs/plans/mgcs-development-plan-2026-10-10.md` and
  `docs/comparisons/ground-station-comparison-2026-10-10.md` are linked from `README.md` and
  `AGENTS.md` so the plan and the QGC/MP comparison are discoverable from the doc index.

### Changed

- Documentation code references are symbol-and-file based instead of line numbers, so they no
  longer break on refactors: `docs/constraints/parity.md`,
  `docs/features/px4-parity.md`, `docs/plans/mgcs-development-plan-2026-10-10.md`,
  `docs/comparisons/ground-station-comparison-2026-10-10.md` and
  `docs/constraints/known-deviations.md` now point at `isGuidedGotoFlightMode`,
  `guidedTakeoff`, `minimumTakeoffAltitudeMeters`, `_uploadToVehicle`/`_clearVehicleMission`,
  `_FenceRallyCard` and `mg_command_submit_for_vehicle`/`mg_command_poll` instead of the
  pre-KD-43/KD-44 line numbers.
- Extracted the mission transfer section out of the planning panel:
  `flutter_shell/lib/mission/mission_transfer_section.dart` now owns the action bar
  (read / clear / export / upload / start), the transfer progress and verification/diagnostic
  display, the adopt-vehicle-command path and the three verification widgets that only it used
  (`_MissionVerificationPanel`, `_HomeSyncNotice`, `_MissionDiagnosticRow`). The seam is three
  constructor arguments (`mission`, `controller`, `onRequestCommandConfirmation`);
  `_MissionPlanningFloatingPanelState` drops from 777 to 377 lines and
  `mission_planning_panel.dart` from 3053 to 2362 (KD-44). One visible difference: the "exported
  to ..." inline notice now lives in the section, so collapsing and re-expanding the panel no
  longer keeps it.
- Extracted the realtime waveform subsystem out of `ConnectionController`:
  `flutter_shell/lib/connection/waveform_controller.dart` now owns the IMU/magnetometer ring
  buffers, the 10 Hz polling timer, instance discovery and the SCALED_IMU/2/3 stream-rate
  negotiation. Per-vehicle switching and disconnects go through the new
  `captureSnapshot`/`applySnapshot`/`reset`/`stopPolling`/`restartPolling`/`onLinkEstablished`
  lifecycle hooks instead of the controller moving waveform fields one by one;
  `connection_controller.dart` drops from 4160 to 3833 lines (KD-43). The public waveform API is
  now reached as `controller.waveform.*` (`changes`, `instances`, `instanceIndex`, `points`,
  `pointCount`, `magLatest`, `setInstance`, `setActive`), and the buffers are allocated eagerly,
  so they are valid even when the native library fails to load.
- The synchronous `mg_fs_download_cancel` export is gone: FTP transfers are now async and are
  cancelled with `mg_fs_cancel`, observed with `mg_fs_transfer_status`. `MavlinkAdapter`'s old
  blocking `list_directory`/`download_file`/`upload_file`/`cancel_file_transfer` methods were
  replaced by `file_*_start`/`file_transfer_status`/`file_transfer_cancel`/`file_transfer_wait`.
- Logging FFI exports are now named by object with no legacy aliases: `mg_onboard_log_*`
  (onboard DataFlash list / download / cancel / status), `mg_tlog_*` (link `.tlog` recording),
  `mg_session_log_*` (JSONL/CSV session files) and `mg_event_*` (structured event stream);
  `mg_fs_*` stays as generic MAVLink FTP. The old `mg_log_*` / `mg_log_record_*` /
  `mg_log_file_*` names are removed rather than aliased — header, implementation, Dart bindings,
  API catalog and docs updated in one change.
- Logging byte counters (`size_bytes`, `received_bytes`, `total_bytes`, `saved_bytes`, `bytes`)
  are emitted as 64-bit values instead of `int`, so DataFlash / `.tlog` files larger than 2 GiB
  no longer wrap to negative numbers.

- Different commands may now run concurrently for the same vehicle; only a duplicate of a command
  that is still awaiting its ACK is refused (QGC `MavCommandQueue` rule). This unblocks e.g. RTL
  while a guided reposition is pending (#3).
- `mg_command_terminate_flight` is the only flight-termination entrypoint. It sends
  `DO_FLIGHTTERMINATION` (irreversible), which is not the recoverable emergency stop
  (`COMPONENT_ARM_DISARM` + param2=21196).
- Extracted `NativeCore` (sole owner of the native library and handle) and `SigningController` out
  of `ConnectionController`: 4273 → 3780 lines, with no `dart:ffi` left in the controller.

### Fixed

- PX4 flight modes had two drifting tables: `PX4Plugin` kept its own `kPX4Modes` while
  `firmware_adapter.cc` decoded through `px4_modes.h`, so the same `custom_mode` could be named
  differently in the status bar and in role resolution, and both tables missed the PX4 1.14+
  values (`ALTITUDE_CRUISE=11`, POSCTL `ORBIT/SLOW`, `AUTO_GUIDED_COURSE=19`, `VTOL_TAKEOFF=10`).
  `px4_modes.h` is now the single source of truth, consumed by both; the removed RTGS sub-mode is
  reserved and decoded as `Mode(N)`. It also carries a separate `px4_fltmode_slot_table()` for the
  `COM_FLTMODE1..6` parameter enum, which is a different encoding from `custom_mode`, and
  `mg_flight_get_mode_mapping` uses it for PX4 slot names/options and reads `RC_MAP_FLTMODE` as the
  mode channel parameter.
- PX4 guided commands did not match QGroundControl: `mg_command_takeoff` wrote the requested
  relative altitude straight into `NAV_TAKEOFF` param7 even though PX4 expects AMSL, and
  "pause" sent a zero-velocity `SET_POSITION_TARGET` that expires. PX4 takeoff now adds the current
  `GLOBAL_POSITION_INT` MSL altitude (mm to m) to param7 and is refused locally when no valid
  position exists; PX4 pause sends `DO_REPOSITION` with `MAV_DO_REPOSITION_FLAGS_CHANGE_MODE`, waiting
  for the ACK. ArduPilot keeps the relative-home altitude and the zero-velocity pause.
- `PX4Plugin::guided_mode_name()` returned `Offboard` and `is_guided_mode()` matched it, while the
  Flutter side already treated PX4 guided mode as `auto loiter` (QGC `isGuidedMode` = pause/takeoff/
  land). It is now `Auto Loiter` with the QGC three-mode `is_guided_mode`, so core and UI agree.
- Firmware capabilities ignored the vehicle class and misreported orbit: PX4 returned one set for
  all airframes without `CapOrbit`, yet the map "orbit" entry was gated by `guided_mode` so it was
  usable while the JSON said `orbit=false`. Capabilities now follow QGC per airframe (`CapRoi`/
  `CapChangeHeading` added), and the JSON gains `roi`/`change_heading`; the Flutter orbit/ROI entries
  read those fields, so each button matches the reported capability.
- PX4 ignored its minimum takeoff altitude: the Flutter side only read ArduPilot parameters (always
  0 on PX4) and `PX4Plugin::minimum_takeoff_altitude` had no caller. It now returns the QGC base
  default 3.048 m (10 ft), is exposed as `minimum_takeoff_altitude_m` in `mg_firmware_get_type`, and
  is used by the Flutter takeoff flow, which also refuses takeoff while the position is unknown.
- The PX4 safety wrappers fabricated circuit breakers: `mg_px4_safety_get/set` hard-coded
  `CBRK_AIRSPD_CHK`/`CBRK_GPS_CHK`/`CBRK_IO_SAFETY`, but `CBRK_GPS_CHK` exists in neither the QGC
  `PX4ParameterFactMetaData.json` snapshot nor PX4-Autopilot `e7b1d486e5` (it always read 0 and a
  write silently did nothing, while `CBRK_AIRSPD_CHK` is not in the QGC snapshot). They now report
  only the `CBRK_*` parameters the vehicle actually caches (field name = parameter name); a set
  rejects a non-`CBRK_` key with `INVALID_PARAM`, a breaker the vehicle lacks with `NOT_FOUND`, and
  a failed send with `SEND_FAILED`.
- The PX4 actuator wrapper covered only the first 8 outputs: `mg_px4_actuator_get/set_config`
  read and wrote `PWM_MAIN_FUNC1..8` although PX4 exposes `PWM_MAIN_FUNC1..16` (present in both the
  QGC snapshot and PX4 source). It now covers all 16, reporting a missing channel as 0 (Disabled).
- `mg_px4_flight_mode_get_config` is read-only by design: writing `COM_FLTMODE1..6` must stay
  consistent with `RC_MAP_FLTMODE` or the vehicle loses its usable modes, so no setter was added and
  the header documents that mode-slot writes go through the generic parameter page.
- The unused `default_loiter_radius` firmware-plugin virtual was removed.
- Link-layer `.tlog` recording was never actually wired: `MavlinkLogWriter` had no call site
  outside its unit test, so the recording UI would have produced empty files. `MavlinkAdapter` now
  exposes `set_log_sink()` and records sent/received frames on the delivery path
  (`record_frame()`), attached idempotently in `Vehicle::wire_telemetry_callbacks_for`
  (ADR-0006). Signed frames have their signature stripped and CRC recomputed, and `SETUP_SIGNING`
  is skipped, matching QGroundControl.
- The old `.tlog` writer emitted a magic+length prefixed layout that neither QGroundControl nor
  Mission Planner could read; it now writes the shared big-endian-timestamp + raw-frame format
  above. `FlightLogReplayer` truncation semantics were corrected (an open scan keeps the
  truncated result, a successful seek clears it).
- `mg_tlog_replay_seek` resynchronised by scanning raw bytes from the target offset, so the 8-byte
  timestamp prefix was fed to the MAVLink parser and the landing point drifted with the recorded
  timestamps (reproducibly wrong in 2 of 30 runs on a 3-message file). `FlightLogReplayer::seek`
  now walks to the first complete record whose start offset is at or past the target and buffers it,
  so the first `step` after a seek returns the landing record itself; seeking to the end returns
  `NOT_FOUND`.
- `mg_fs_*` no longer blocks the calling thread for the whole transfer: the blocking FTP loop runs
  on a worker thread whose status is guarded by its own mutex, progress is kept in atomics, and
  cancellation sets both the running flag and a start-window flag so an immediate cancel is not
  lost. `MavlinkAdapter::Impl::~Impl()` cancels and joins the worker before the send members are
  destroyed. Verified by new core tests (async download/list/upload, single-session rejection,
  cancel mid-flight, reuse after cancel) and an FFI end-to-end test (start → poll `completed` →
  file on disk, `FILE_NOT_FOUND` → `failed`/`error_code` 10, silent link → `cancelled`).
- Log download no longer hangs when the flight controller ends the stream (`LOG_DATA` count 0)
  before the declared `LOG_ENTRY.size` bytes have arrived: it now reports
  `log truncated: received X of Y bytes`, writes no partial file, and never reports completion.

- Mission command × vehicle capability table (`docs/mission-capability-table.md`): each catalog
  command now carries a `vehicles` bit mask (Copter/Plane/VTOL/Rover) sourced from the ArduPilot
  `mode_auto` / `commands_logic` / `AP_Mission_Commands` switches, so an unsupported item is
  refused with an explanation instead of being silently ignored by the flight controller. The
  FFI catalog JSON and the Flutter `MissionCommandDescriptor` expose the same field, and the
  mission editor filters the command dropdown by the connected vehicle.

- Feature brief for the logging / DataFlash / replay direction (`docs/features/flight-logging-and-replay.md`),
  the first item of plan phase 3: inventory of the 19 `ffi_api_logging.h` exports, QGroundControl and
  Mission Planner behavior evidence with file:line references, gap table, decision points and the
  S1/S2/S3 slices. It records seven verified problems: `mg_fs_*` block the calling thread for the
  whole transfer, log download reports completion before the file is written (write failures are
  swallowed), three overlapping `mg_log_*` API families, a side-effecting `log_download_status`,
  `int`-truncated byte counters, missing erase/index/integrity checks, and a missing
  `CHECK_CONNECTED` in `mg_onboard_log_get_entries`.
- Onboard log download no longer reports `completed` before the file is on disk: the write runs
  outside the adapter lock and `download_completed` / `saved_bytes` are set only after a
  successful `fopen` / `fwrite` / `fclose`. Open, short-write and flush failures now surface as
  `state="failed"` with `download_error` instead of being swallowed, and
  `mg_onboard_log_download_status` exposes `saving` / `saved_bytes`.
- `mg_onboard_log_download_status` no longer mutates the link: the lost-packet `LOG_REQUEST_DATA`
  re-send moved out of the status query into the download's own timeout handling
  (`process_timeouts()`), which retransmits the first missing 90-byte block after a 500 ms stall.
- `mg_onboard_log_get_entries` now applies `CHECK_MAVLINK` and `CHECK_CONNECTED` like the rest of
  its family.

- Scheduled-mission read-back no longer reports a spurious difference for waypoint fields
  ArduPilot does not store: fixed-wing builds (including VTOL/quadplane) pack the acceptance and
  pass radius into `MISSION_ITEM_INT.p1` and drop `param1`, while Copter/Rover store `param1` and
  drop the radii; `param4` (yaw) is never stored. `mg_mission_doc_verify` now classifies those
  fields as normalizations, so a correctly uploaded ArduCopter mission verifies as a match
  (found by the SITL scenario; `normalization_count=3` for copter, `0` for plane).
- Mission upload runs a vehicle-capability gate immediately before the transfer: after the
  document compiles, `mg_mission_doc_upload` re-validates every entity against the connected
  vehicle's parameter profile and fails with `mission.command_vehicle_unsupported` if the
  planned vehicle and the connected vehicle disagree, instead of handing an incompatible
  plan to the flight controller.
- Mission import validation now checks each entity and complex item against the document's
  declared `vehicle_type` for ArduPilot (`entity.command_vehicle_unsupported` /
  `complex_item.command_vehicle_unsupported`). PX4 or firmware-unknown documents skip the
  check, and unmodelled vehicles (Sub/Tracker) are never rejected on guessed data.
- QGC `.plan` round-trip no longer drops data M-GCS does not model: unknown task-item fields
  (for example `altitudeMode`, `altitude`, `AMSLAltAboveTerrain`), unknown `mission` fields
  (`hoverSpeed`, `globalPlanAltitudeMode`) and unknown top-level sections are preserved
  verbatim and re-emitted on export. `Document.extensions` is now populated on import
  (`qgc_plan.root_fields`, `qgc_plan.mission_fields`) and entities gained
  `MissionEntity::extensions_json`.
- The Flutter mission document models now carry `complex_items`, document `extensions` and
  entity `extensions` through `replace` / `update`, so editing a QGC-imported plan in the UI
  no longer deletes Survey/Corridor complex items or their preserved fields.
- Sanitizer passes no longer overwrite the release shared library: `tools/sanitize.sh` builds with `MGCORE_BUILD_SHARED=OFF`, and preflight re-links `build/libmgcore.*` if it is stale or sanitizer-instrumented before the FFI symbol check, so the Flutter bundle and native symbol check always load the Release library.
- Unix shared-library packaging now keeps the platform `lib` prefix (`libmgcore.so` / `libmgcore.dylib`); Windows continues to ship `mgcore.dll`.
- Stale heartbeat state is treated as unknown across the flight-mode, map, mission and emergency-stop entry points, so old armed/mode data cannot authorize a new command.
- Queued command send failures now invoke their result callbacks with an explicit `MAV_RESULT_FAILED` error.
- Removed unused legacy Flutter entry points for the standalone flight-mode panel, attitude indicator and configuration workspace alias; callers now use the current modules directly.
- Removed unused Arimo configuration, redundant GPS fixture assignments and obsolete test placeholders (KD-28, KD-29, KD-34).
- Restored the generated FFI catalog and added checks for Dart bindings, all native exports and the loaded library version (KD-41); full preflight also runs the native FFI smoke tests.
- S6 frontend firmware-literal gate: `tools/check_firmware_literals.py` scans `flutter_shell/lib` for parameter names owned by the firmware plugins (`core/src/config/ParameterDescriptors.json`, `PX4ParameterMetaData.json`) and fails on new literals outside the recorded baseline (`tools/firmware_literals_baseline.json`). The baseline is only allowed to shrink; the 34 pre-existing literals it once registered are gone and the baseline is now empty (KD-42 closed). Preflight runs the gate with the frontend checks, and a parameter-descriptor change reruns it even when the full Flutter suite is not in scope.
- Anti-over-engineering rule and pending god-file review: `docs/constraints/architecture.md` gains **A7** (a newly extracted module, interface or layer must have a current real consumer - no abstractions reserved for a hypothetical future) and a SHOULD to split large files by change coupling rather than line count. The two largest sources (`flutter_shell/lib/connection/connection_controller.dart`, `flutter_shell/lib/mission/mission_planning_panel.dart`) are registered as KD-43/KD-44 (待评估拆分) so a future split starts from an evaluation rather than intuition.
- Stage 1A started: fleet/session summaries now expose `heartbeat_fresh`; stale vehicles are excluded from batch flight-command targets, while the core session status and v2 telemetry response expose the same freshness signal.
- Release metadata is required and validated before edits; release/preflight now understand bracketed CHANGELOG version headings (KD-38).
- Mission upload: accept `MISSION_ACK` only from the transfer target (sysid/compid) and only while a
  transaction is actually waiting for it, so another vehicle on a shared link cannot end this
  vehicle's transfer (#4). ACK matching for commands now includes the source system for the same reason.
- Mission upload: back up the vehicle plan before the upload and roll it back when the flight
  controller rejects it, the send fails or the transfer times out. `mission_get_plan` no longer
  reports a mission the vehicle never accepted (#4, KD-19).
- Mission transfer: timeouts and send failures were reported as `MAV_MISSION_OPERATION_CANCELLED`,
  which surfaced as "cancelled by the user" in the verification flow. They now report
  `MAV_MISSION_ERROR` and keep the timeout reason visible (KD-14).
- PX4: compass calibration now sends `PREFLIGHT_CALIBRATION` with param2=1 instead of the
  ArduPilot-only `DO_START_MAG_CAL`, which PX4 ignored while the UI showed "calibrating" (#7).
- PX4: calibration cancel and the six-face accelerometer confirmation report "not supported"
  instead of pretending to succeed; calibration also fails after a 30 s stall (KD-17).
- `mg_params_load_from_file` reports partial failure together with the rejected parameter names.
- Flutter: the top status bar shows `—` instead of stale GPS/battery/mode values after telemetry
  goes silent, and map/mission commands without a confirmation callback no longer execute silently.

- ArduCopter, ArduRover and AntennaTracker mode tables were built from list indices instead of
  the upstream enum values, shifting every mode from a certain point onward. Copter 9 (LAND) decoded
  as "Drift", so the UI showed Drift while landing; Rover 9 is CIRCLE while `land()` sent 9. Tables
  rewritten from `ArduCopter/mode.h`, `Rover/mode.h` and `AntennaTracker/mode.h`, with tests pinning
  the safety-relevant entries.
- RTL / land / loiter now resolve through the firmware plugin by semantic role instead of hardcoded
  ArduCopter numbers (KD-18, safety.md S6). Modes a firmware/vehicle does not have (Land on Rover or
  Plane, RTL on Sub) are refused with a warning instead of sending another vehicle's value.
- Stale deviation entries: KD-07 (flight mode switch confirmation) and KD-12 (map menu commands)
  were already fixed in code but still registered as open, and five other documents still pointed at
  deviations that had been closed (KD-14, KD-17, KD-19 in parity.md, KD-07/KD-12 in ui-pages.md).
- parity.md claimed QGC confirms guided map actions after 3000 ms. That value is the help-text hide
  timer in `QGCDelayButton.qml:42`; the actual confirmation delay is `defaultDelay: 500` (`:24`).
  M-GCS's 500 ms hold was already aligned, and the recorded decision was corrected instead of
  changing code.
- A5 (`dart:ffi` only in backend files) is now enforced by
  `flutter_shell/test/architecture_ffi_boundary_test.dart` instead of relying on a manual check of a
  `backend/` directory that does not exist.
- Mission upload, fence and rally upload now run through the shared confirmation component, and
  RC calibration start plus the signing-key "save and apply"/`SETUP_SIGNING` actions confirm first
  as well (KD-08, KD-10, KD-11); the deviation register now lists only unresolved items and keeps a
  one-line index of closed IDs so code comments that cite `KD-xx` stay traceable.
- `ui.md` U3 now states that the platform CJK font fallback chain is allowed inside `MgFonts` only,
  resolving the KD-35 ambiguity, and its verification note matches what `tools/check_ui_tokens.dart`
  actually checks.

### Removed

- Deleted the six `bridge/ffi` compatibility forwarding headers (`json_builder.h`,
  `json_serializers.h`, `param_json_serializers.h`, `sensor_health_json_serializers.h`,
  `telemetry_cache.h`, `telemetry_freshness.h`). They re-exported the telemetry JSON builder,
  serializers, cache, freshness and group types that had already moved to `core/telemetry` under the
  old `mg::bridge::*` names, so every type had two names. Bridge code now includes
  `core/telemetry/...` and refers to the single `mg::core::telemetry::*` name; the KD-20 guard in
  `test_bridge_architecture_guards.cc` no longer lists the deleted headers.

## [0.1.1] - 2026-10-09

### Added
- Multi-agent collaboration framework with roles for Claude, Codex, DeepSeek, and WorkBuddy
- Pragmatic on-demand workflow in `docs/workflow/COLLABORATION.md`
- Comprehensive constraint documentation with three-level priority (MUST/SHOULD/INFO)
- `docs/constraints/`: structured constraint files replacing scattered documentation
- `docs/adr/`: migrated architectural decision records 0001-0004
- `CLAUDE.md`: entry point for Claude Code automatic loading

### Changed
- Restructured documentation hierarchy: removed scattered planning and research docs
- Consolidated upstream reference rules into single source of truth
- Improved constraint clarity with explicit severity levels
- Updated `AGENTS.md` with rule hierarchy and workflow entry point

### Removed
- Legacy `CHANGELOG.md` with 1850 lines of outdated entries
- Scattered README files in `bridge/`, `core/`, `flutter_shell/`
- Old architecture documentation structure under `docs/architecture/`
- Planning documents that were superseded by constraint system
- Research documents that were consolidated into ADRs

## [0.0.80] - 2026-10-08

Previous releases tracked in git history.
