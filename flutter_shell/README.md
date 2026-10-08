# M-GCS Flutter Shell

Current version: `0.0.79` (source of truth: `pubspec.yaml`).

The shell is the cross-platform presentation layer for M-GCS. It contains the
map, connection workspace, telemetry and waveform views, mission planning,
configuration pages, calibration flows, and parameter editing. It talks to the
native core only through `bridge/ffi/ffi_api.h`; it does not import MAVLink or
reimplement backend protocol state.

Recent shell behavior:

- Dangerous commands use one floating 0.5-second hold-to-confirm control;
  moving to a location, RTL, mission start, and arm/disarm wait for confirmation.
- Flight messages are translated to Chinese for known ArduPilot status templates
  while unknown text remains verbatim.
- The lower-right composite MacGlass instrument combines attitude and heading;
  the map and attitude/position views poll at 20 Hz.
- QGC mission files and vehicle readback preserve non-location command semantics,
  so `MAV_FRAME_MISSION` items do not become fake waypoints.

## Architecture

- `lib/connection/connection_controller.dart` owns the shared native library
  lifetime, connection state, polling, state convergence, and UI notifications.
- Domain ABI bindings are split into `MissionDocumentFfiBackend`,
  `TelemetryFfiBackend`, `ParameterFfiBackend`, `CalibrationFfiBackend`, and
  `FlightCommandFfiBackend`.
- `lib/mission/` contains the mission document model, controller, planning
  panel, map interaction, and file interchange entry points.
- `lib/map/` contains raster tiles, aircraft/Home markers, WGS-84/GCJ-02
  conversion, and map interaction.
- `lib/configuration/` contains the configuration workspace router and nine
  feature pages. Calibration is split into views and cards; PID into its
  parameter model, page, and gain widgets. Shared tab surfaces, parameter
  editors, status notices, and summary rows live in `configuration/widgets/`.
  Each file is an independent Dart library; page-local state remains private.
  `lib/configuration_workspace.dart` preserves the original import as a
  compatibility export. Battery and airspeed calibration remain placeholders;
  splitting the source does not add those features.
- `lib/theme/` is the single source for fonts, typography, metrics, and colors.
  Configuration spacing is defined once by `MgMetrics.config*`.

## UI Rules

All platforms use the bundled `MiSans` family defined by `MgFonts`. Page code
must use `MgTypography`, `MgColors`, and `MgMetrics` instead of raw font sizes,
colors, or platform font names. Numbers use tabular figures; only headings may
use a heavier weight.

The map reuses the raster-tile catalog and currently supports Gaode standard,
Gaode satellite, satellite-with-road-labels, OpenStreetMap HOT, and
OpenHikingMap. No AMap JavaScript key is required for this tile integration.

The map connection control supports serial, UDP, and TCP endpoints. Every mode
opens a backend session, waits for a heartbeat, and disconnects by session ID.

## Run And Verify

```sh
flutter pub get
flutter run -d macos
flutter analyze lib test
flutter test
```

The macOS build embeds `mgcore.dylib` from the repository backend build. Native
core changes require an app restart; Dart hot reload does not replace the
already loaded shared library.
