#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_dir="$root_dir/app"
expected_flutter="$(cat "$root_dir/.flutter-version")"
expected_xcode="${MGCS_EXPECTED_XCODE_VERSION:-27.0}"

if ! command -v flutter >/dev/null 2>&1; then
  echo "[mgcs] flutter is not installed or not on PATH" >&2
  exit 1
fi

actual_flutter="$(flutter --version 2>/dev/null | awk '/^Flutter / {print $2; exit}')"
actual_xcode="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}')"
developer_dir="$(xcode-select -p)"

printf '[mgcs] Flutter: %s (expected %s)\n' "$actual_flutter" "$expected_flutter"
printf '[mgcs] Xcode: %s (expected %s)\n' "$actual_xcode" "$expected_xcode"
printf '[mgcs] Developer directory: %s\n' "$developer_dir"

[[ "$actual_flutter" == "$expected_flutter" ]] || {
  echo "[mgcs] Flutter version mismatch; use the pinned SDK from .flutter-version" >&2
  exit 1
}
[[ "$actual_xcode" == "$expected_xcode" ]] || {
  echo "[mgcs] Xcode version mismatch; install/select the pinned Xcode version" >&2
  exit 1
}
[[ "$developer_dir" == */Xcode.app/Contents/Developer ]] || {
  echo "[mgcs] active developer directory is not the full Xcode installation" >&2
  exit 1
}

grep -q 'MACOSX_DEPLOYMENT_TARGET = 12.0;' "$app_dir/macos/Runner.xcodeproj/project.pbxproj" || {
  echo "[mgcs] project must target macOS 12.0 or newer" >&2
  exit 1
}
echo '[mgcs] macOS toolchain check passed.'
