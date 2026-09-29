# Changelog

## 0.0.10 - 2026-09-29

### Added

- Map now draws the flight controller's Home point and the aircraft's current
  position, fed by the existing `mg_vehicle_get_global_position` and
  `mg_vehicle_get_home_position` caches at the regular telemetry rate. The
  aircraft marker rotates with heading; neither marker is drawn while the fix is
  invalid.
- Map view preferences are persisted (tile source, zoom, centre) under the app
  support directory, so the map reopens where it was left. New dependency:
  `path_provider` (BSD-3-Clause).
- Per-compass magnetometer calibration progress. `mg_calibration_get_status` now
  carries `compass_progress: [{compass_id, completion_pct}]` alongside the
  aggregate `progress`, and the calibration card draws one bar per detected
  compass.
- `docs/FONT_ALTERNATIVES_RESEARCH.md` and ADR 0007 record how the UI font stack
  was chosen, with per-glyph similarity measurements.

### Changed

- UI font is now a single bundled family, `MiSans`, on every platform, so macOS
  and Windows render identically. The previous stack silently resolved to
  Helvetica Neue / PingFang SC / Menlo on macOS because `SF Pro Text` is not a
  resolvable family name.
- Only titles are bold. Numeric readouts, status labels, badges, sensor models,
  firmware names and sidebar entries all use `w400`; the sidebar selection is
  expressed by its accent background instead of font weight.
- The sidebar is collapsible from a chevron next to the (now removed) search box
  and collapses to icons only.
- Waveform polling runs at 10 Hz instead of 1 Hz, matching the requested stream
  rate; `MgAnimatedNumber` tweens in 100 ms so the readout keeps up.
- Waveform window is 40 s.
- Motor output PWM bars are 18 px tall and use a smaller numeric size; the same
  widget is reused for RC calibration.
- Flight mode cards highlight the slot selected by the live `FLTMODE_CH` PWM, and
  RC5-16 cards highlight only when the channel sits in its high band.
- All-parameters page is a divider-separated table with Chinese display names and
  descriptions where the descriptor database provides them, plus unit labels.
- Empty pages keep their widget skeleton with a status strip instead of showing a
  blank page and a few lines of text.
- Arming checks use `ARMING_SKIPCHK` on current firmware with `ARMING_CHECK` as a
  fallback, rendered as a divided grid.
- Compass orientation and calibration tolerance are bound to real parameters
  (`COMPASS_ORIENT`, `COMPASS_CAL_FIT`) instead of the previous dead controls.
- Magnetometer field strength is refreshed for every compass instance, not only
  the selected one.

### Fixed

- Map coordinates: the flight controller reports WGS-84 while AMap tiles use
  GCJ-02, so markers were offset by 100-700 m. Conversion now follows Mission
  Planner's `MercatorProjectionGCJ`, with a three-step inverse that converges to
  centimetres.
- Calibration failures no longer discard the flight controller's own message.
  Both the STATUSTEXT and COMMAND_ACK paths previously replaced it with generic
  guidance, which is why every failure looked like "move away from magnetic
  interference".
- Accelerometer quality thresholds were an order of magnitude too strict for
  ArduPilot's `INS_ACCOFFS_*` range (+/-3.5 m/s2), so successful calibrations
  were reported as failed.
- Compass quality now trusts ArduPilot's own fitness score when available instead
  of taking the worse of fitness and an offset-magnitude heuristic.
- `DO_START_MAG_CAL` now sends `param2=1` (retry on failure) like Mission
  Planner, instead of disabling the flight controller's own retry.
- Motor output table width follows the panel; it previously stayed at 1040 px, so
  widening the window left blank space and narrowing it overpainted the border.
- `copy_points` with `max_points=1` returns zero points; the "latest sample"
  lookup now uses a 1 ms window instead, with a regression test pinning both
  behaviours.

### Verification

- Backend offline/simulation gate: 33 passed.
- Flutter static analysis: passed.
- Flutter widget tests: 12 passed.
- UI token check: passed.
- macOS debug build: passed.

## 0.0.9 - 2026-09-29

### Added

- Added the mission planning engine with its Flutter-side document controller,
  validator, estimator, and upload/verify bridge.
- Added the arming checks page: 19 checks driven by a built-in bit table, reading
  and writing `ARMING_SKIPCHK` on newer firmware with `ARMING_CHECK` as a
  fallback for older ones.
- Added RC transmitter calibration end to end: a new `CalibrationType::Radio`
  strategy sending `MAV_CMD_PREFLIGHT_CALIBRATION` param4=1, `RCn_MIN/TRIM/MAX`
  read-back verification, and a Flutter page with live channel travel and
  `RCn_REVERSED` toggles.
- Added a staged PID tuning page with basic/extended levels for multirotor,
  fixed-wing, and VTOL. Multirotor and VTOL share one layout and differ only in
  the `ATC_`/`PSC_` versus `Q_A_`/`Q_P_` prefix; fixed-wing uses
  `RLL_RATE_`/`PTCH_RATE_`/`YAW_RATE_` plus `RLL2SRV`/`PTCH2SRV`/`YAW2SRV`
  and TECS.

### Changed

- Reworked the motor output page: `macos_ui` progress bars with centered PWM
  readouts, `MacosSwitch` for servo reverse, and optimistic parameter writes that
  render immediately and roll back when the flight controller rejects the value.
- Raised the regular telemetry poll rate from 2 Hz to 10 Hz and removed the
  separate servo output limiter, so the PWM bars track live output.
- Dropped the "waiting for parameter confirmation" banner; write failures are
  still surfaced.
- Removed unused frontend declarations (`MgTypography.group`, `hasParameters`,
  `isCalibrationActive`, `CalibrationStatus.isSuccess/isFailed`, ...) and merged
  the duplicated parameter lookup and error-formatting helpers.
- Renamed `widgets/mg_range_bar.dart` to `mg_motor_output_bar.dart` so the file
  matches its widget.
- Tightened the arming checks layout into a divided grid with uniform row heights.

### Fixed

- Corrected the position and velocity PID parameter names to the ArduPilot 4.8
  `PSC_NE_*` / `PSC_D_*` spelling; the previous `PSC_POSXY_P`-style names do not
  exist on current firmware, so that block read nothing.
- Hardened the Windows serial runtime and bridge packaging.

### Verification

- Backend offline/simulation gate: 33 passed.
- Flutter static analysis: passed.
- Flutter widget tests: 7 passed.
- UI token check: passed.
- macOS debug build: passed.

## 0.0.8 - 2026-09-28

### Changed

- Connected the Flutter FFI loader to Windows `mgcore.dll`, Linux `libmgcore.so`,
  and Android `libmgcore.so` loading paths.
- Added Windows and Linux Flutter build steps that build and stage MgCore and its
  runtime JSON resources beside the desktop executable.
- Added the Android Flutter project and USB Host capability declaration. Android
  still requires a native USB-serial driver bridge before real USB flight-controller
  connections can be validated.
- Expanded the magnetometer calibration surface with per-instance discovery,
  compass mounting state, orientation controls, enable toggles, and field-strength
  summaries.
- Added a Flutter-rendered configuration workspace shell with responsive sidebar,
  search, navigation, toolbar, and explicit light/dark backgrounds; the page
  keeps its existing visual language without depending on macOS-only window
  plugins for these elements.
- Added shared typography and metrics tokens, animated numeric readouts, and a
  PWM range bar with minimum/midpoint/maximum markers for the motor output page.

### Verification

- Cross-platform workspace widget tests: 3 passed.
- Flutter static analysis: passed.
- macOS debug build: passed.

## 0.0.6 - 2026-09-27

### Added

- Added the minimal macOS Flutter shell for connection setup, telemetry, sensor waveforms and calibration workflows.
- Added IMU and magnetometer instance selectors with per-instance waveform buffers and sensor model labels.
- Added accelerometer calibration controls for six-face and level calibration, including step status, orientation placeholder, confirmation and cancellation.

### Changed

- Removed the `1s` mean readout from waveform value cards so the cards show only the latest sample.
- Aligned waveform instance selectors and chart content, and tightened the spacing between the waveform area and calibration cards.
- Added macOS bundle-resource lookup for `SensorDeviceInfo.json`, enabling sensor model decoding from packaged apps.
- Synchronized MgCore and Flutter shell version metadata to `0.0.6` (build `6`).

### Verification

- Flutter static analysis: passed.
- macOS debug build: passed.
- Native sensor and calibration tests: 3/3 passed.

## Backend branch - 2026-09-23

`New-main` was created from `v0.0.5` as the backend and interface branch. Its current shell and backend changes are released together in `main` as `v0.0.6`.

### Removed on this branch

- The Flutter desktop application and all Dart sources.
- Flutter desktop build scripts and toolchain checks.
- Frontend architecture, component and review documents.
- Frontend-specific release notes and the frontend session contract ADR.

### Changed on this branch

- CI now builds and tests the backend on Ubuntu, Windows and macOS.
- The Docker image and compose file only provision the backend toolchain.
- `README.md` and the maintained docs describe the backend contract instead of the Flutter client.
- `mg_vehicle_open()` now creates a session without changing active focus; `mg_vehicle_select()` is the explicit focus operation, while legacy single-connection helpers retain their compatibility behavior.

## 0.0.5 - 2026-09-23

### Changed

- Preserved the complete Flutter desktop frontend while restructuring the client into `application`, `infrastructure`, and `presentation` layers.
- Moved MgCore FFI and JSON adapters into `app/lib/infrastructure/mgcore/` and Flutter controllers/UI into `app/lib/presentation/`.
- Added dependency-direction guards for application, infrastructure, and presentation code.
- Fixed parameter fallback recovery, calibration callback lock boundaries, truncated magnetometer progress parsing, and hidden backstage polling.
- Synchronized CMake, FFI, and Flutter version metadata to `0.0.5` (build `5`).

### Verification

- Flutter tests: 119 passed.
- Native CTest offline/simulation gate: 29/29 passed.
- macOS Debug build: succeeded.

### Notes

- This release keeps the existing Flutter frontend intact. The backend-only client branch is created from this tag and intentionally removes the Flutter application there.

## 0.0.4 - 2026-09-23

### Added

- Capability-based application contracts for mission planning, geofence/Rally, flight logs, MAVLink FTP files, fleet management, and flight control.
- Mission editor support for local waypoint creation, map-coordinate waypoint insertion, validation, JSON/CSV export, and mission transfer progress.
- Flight Logs page with onboard storage browsing, file selection, download progress, cancellation, and upload progress.
- Fleet controller and frontstage vehicle panel for vehicle discovery, active-vehicle selection, batch arm/disarm, mode changes, RTL, landing, per-vehicle results, and active-vehicle restoration.
- Geofence and Rally workflows with validation plus JSON/KML import and export.
- Chinese/English localization catalogs, generated localization bindings, and localization contract tests.
- MacGlass shell contracts and shared frontstage models so the visual shell can be replaced without rewriting application logic.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.4` (build `4`).
- Improved connection/session handling, parameter-session isolation, telemetry polling, and reconnect/disconnect behavior.
- Extended MAVLink FTP and logging FFI paths for onboard file listing, upload, download, cancellation, and progress reporting.
- Raised the macOS deployment target to 12.0 to match the current Flutter/Xcode toolchain baseline.
- Expanded the regression suite to cover fleet control, mission editing/validation, localization, coordinate conversion, parameter synchronization, vehicle files, and session capabilities.

### Verification

- Flutter static analysis: passed with no issues.
- Flutter tests: 114 passed.
- macOS debug build: `mgcs.app` built successfully.
- Native CTest offline/simulation gate: 29/29 passed.

### Known limitations

- Native multi-vehicle batch commands still route through the active-vehicle selection as a compatibility bridge; a future explicit `vehicle_id` command target would remove this temporary coupling.
- Camera, gimbal, and video FFI primitives are present but are not yet exposed as complete Flutter workflows.
- Mission command coverage, log analysis/replay, offline map packages, firmware flashing, mobile packaging, and real-hardware write-operation acceptance remain future work.

## 0.0.3 - 2026-09-10

### Added

- End-to-end TCP and UDP connection flow from the Flutter application through the MgCore FFI backend, including endpoint validation and focused network tests.
- Airframe configuration page for Copter and Plane/QuadPlane, driven by the flight controller's existing parameter metadata.
- Native platform font fallback for Windows, macOS, and Linux.
- Regression coverage for the unified connection drawer layout and network connection callbacks.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.3`.
- Unified the Serial, TCP, UDP, and Bluetooth connection pages around the same `302 x 48` field geometry, label hierarchy, spacing, and glass control styling.
- Made the connection watchdog wake immediately when a disconnect or reset stops it.
- Kept vehicle shutdown outside the multi-vehicle manager mutex to avoid blocking transport teardown.
- Copied runtime metadata into every multi-config output directory so Windows Release tests resolve `SensorDeviceInfo.json` and related data files.
- Improved the Windows desktop build workflow and local toolchain usage.

### Notes

- Bluetooth remains a UI-only connection entry until a native Bluetooth transport and Dart application contract are added.
- Release verification passed 29 backend offline/simulation tests and 84 Flutter tests; Flutter static analysis reported no issues.
- A Windows Release package was built locally with the project-local toolchain and contains `mgcs.exe` and `mgcore.dll`.

## 0.0.2 - 2026-08-12

### Added

- Docker Compose development environment for Linux-based MgCore builds and Flutter checks.
- Shared MAVLink header generation script pinned to `pymavlink 2.4.49`.
- PowerShell and shell wrappers for the Docker development workflow.
- Flutter analyze/test job and reproducible MAVLink generation in GitHub Actions.
- Updated Chinese architecture and optimization reports.

### Changed

- Bumped MgCore and Flutter application versions to `0.0.2`.
- Isolated generated MAVLink headers in a Docker named volume.
- Made the UDP transport test use an available local port.
- Updated widget tests to assert the current calibration and safety UI contract.
- Fixed MSVC standard-library portability and kept multi-config DLL output at `build/mgcore.dll` for Flutter Windows packaging.
- Built the Linux MgCore shared library before Flutter live FFI tests in CI.
- Added a native PowerShell MAVLink generator and a Windows Flutter Release packaging CI gate.
- Packaged MgCore parameter, translation, sensor, and USB board metadata beside the Windows executable.

### Notes

- Docker validates shared C++ and Flutter code but does not produce native macOS or Windows desktop bundles.
- Hardware and manual tests remain opt-in and are not part of the default container or CI gate.
- Release verification passed 29 backend offline/simulation tests and 79 Flutter tests; Flutter static analysis reported no issues.
- The Docker Compose definition was validated, but the local image build could not reach Docker Hub authentication and should be repeated on an unrestricted network.
