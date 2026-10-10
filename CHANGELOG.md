# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
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
### Changed
- Different commands may now run concurrently for the same vehicle; only a duplicate of a command
  that is still awaiting its ACK is refused (QGC `MavCommandQueue` rule). This unblocks e.g. RTL
  while a guided reposition is pending (#3).
- `mg_command_emergency_stop` renamed to `mg_command_terminate_flight` with the old name kept as a
  forwarding alias. It sends `DO_FLIGHTTERMINATION` (irreversible), which is not the recoverable
  emergency stop (`COMPONENT_ARM_DISARM` + param2=21196).
- Extracted `NativeCore` (sole owner of the native library and handle) and `SigningController` out
  of `ConnectionController`: 4273 → 3780 lines, with no `dart:ffi` left in the controller.

### Added
- Emergency stop entry in the top status bar: forced disarm behind the S3 confirmation component,
  shown only while armed so it stays reachable in flight.

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
