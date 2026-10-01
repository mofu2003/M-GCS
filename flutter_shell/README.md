# M-GCS Flutter Shell

Current version: `0.0.50` (source of truth: `pubspec.yaml`).

The shell is the cross-platform presentation layer for M-GCS. It contains the
map, connection workspace, telemetry and waveform views, mission planning,
configuration pages, calibration flows, and parameter editing. It talks to the
native core only through `bridge/ffi/ffi_api.h`; it does not import MAVLink or
reimplement backend protocol state.

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
- `lib/theme/` is the single source for fonts, typography, metrics, and colors.

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
