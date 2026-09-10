#!/usr/bin/env bash
set -euo pipefail

ROOT=/workspace
BUILD_DIR="${MGCORE_BUILD_DIR:-$ROOT/build_docker}"
export MGCORE_LIBRARY_PATH="$BUILD_DIR/libmgcore.so"
VENV_DIR="${MGCS_VENV_DIR:-/opt/mgcs-venv}"
MAVLINK_OUTPUT_DIR="${MAVLINK_OUTPUT_DIR:-/opt/mgcs-mavlink/mavlink/v2.0}"

ensure_mavlink_headers() {
  MGCS_VENV_DIR="$VENV_DIR" \
    MAVLINK_OUTPUT_DIR="$MAVLINK_OUTPUT_DIR" \
    "$ROOT/tools/generate-mavlink.sh"
}

backend_configure() {
  ensure_mavlink_headers
  cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
    -DMGCORE_BUILD_TESTS=ON \
    -DMGCORE_BUILD_SHARED=ON \
    -DMAVLINK_INCLUDE_DIR="$MAVLINK_OUTPUT_DIR" \
    -DCMAKE_BUILD_TYPE=Release
}

case "${1:-shell}" in
  shell)
    ensure_mavlink_headers
    exec bash
    ;;
  configure)
    backend_configure
    ;;
  build)
    backend_configure
    cmake --build "$BUILD_DIR"
    ;;
  test)
    backend_configure
    cmake --build "$BUILD_DIR"
    ctest --test-dir "$BUILD_DIR" --output-on-failure \
      -L 'offline|simulation' -LE 'hardware|manual'
    ;;
  flutter-analyze)
    cd "$ROOT/app"
    flutter --no-version-check pub get
    flutter --no-version-check analyze
    ;;
  flutter-test)
    cd "$ROOT/app"
    flutter --no-version-check pub get
    flutter --no-version-check test
    ;;
  check)
    backend_configure
    cmake --build "$BUILD_DIR"
    ctest --test-dir "$BUILD_DIR" --output-on-failure \
      -L 'offline|simulation' -LE 'hardware|manual'
    cd "$ROOT/app"
    flutter --no-version-check pub get
    flutter --no-version-check analyze
    flutter --no-version-check test
    ;;
  *)
    exec "$@"
    ;;
esac
