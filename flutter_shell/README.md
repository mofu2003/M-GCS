# M-GCS Flutter Shell

Current version: `0.0.7`.

The shell contains the map and a cross-platform configuration workspace.
The configuration sidebar, toolbar, search, and light/dark backgrounds are
Flutter-rendered; macOS-specific code is limited to optional window chrome.
Connection, telemetry, sensor waveforms, and calibration remain available in
the workspace, subject to each platform's native backend support.

The map reuses the raster-tile catalog. It starts with
Gaode standard tiles and supports Gaode satellite, satellite-with-road-labels,
OpenStreetMap HOT, and OpenHikingMap. No AMap JavaScript key is required for
this existing tile-based integration.

Run with:

```sh
flutter run -d macos
```

The initial window opens at desktop workspace size (up to 1440 × 900) and has a
minimum size suitable for the map and status bar.

The map's **连接飞控** control opens a connection popover with
**串口 / UDP / TCP** tabs. Serial mode enumerates
ports and selects a baud rate; UDP mode binds a local address and port; TCP
mode connects to a remote address and port. All three open a backend session,
wait for heartbeat, and disconnect by session ID. The macOS build embeds
`mgcore.dylib` from the repository backend build.
