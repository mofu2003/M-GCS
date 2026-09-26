# M-GCS Flutter Shell

The current macOS shell contains only the top flight-status bar and the map.
The top bar reads GPS, heading, battery, and heartbeat data from the active
serial connection session. Missing or stale telemetry is shown as unavailable,
not filled with demonstration values.

The map reuses the raster-tile catalog from the `main` branch. It starts with
Gaode standard tiles and supports Gaode satellite, satellite-with-road-labels,
OpenStreetMap HOT, and OpenHikingMap. No AMap JavaScript key is required for
this existing tile-based integration.

Run with:

```sh
flutter run -d macos
```

The initial window opens at desktop workspace size (up to 1440 × 900) and has a
minimum size suitable for the map and status bar.

The flight-status bar's right-side **连接飞控** button opens a connection
popover with browser-style **串口 / UDP / TCP** tabs. Serial mode enumerates
ports and selects a baud rate; UDP mode binds a local address and port; TCP
mode connects to a remote address and port. All three open a backend session,
wait for heartbeat, and disconnect by session ID. The macOS build embeds
`mgcore.dylib` from the repository backend build.
