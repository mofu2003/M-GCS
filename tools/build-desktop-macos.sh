#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MGCORE_BUILD_DIR:-$root_dir/build_desktop}"
flutter_bin="${FLUTTER_BIN:-$(command -v flutter || true)}"

if [[ -z "$flutter_bin" ]]; then
  printf 'flutter was not found. Add it to PATH or set FLUTTER_BIN.\n' >&2
  exit 1
fi

cmake -S "$root_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --config Release

cd "$root_dir/app"
"$flutter_bin" --no-version-check pub get
"$flutter_bin" --no-version-check build macos --release

printf '\nBuilt application: %s\n' \
  "$root_dir/app/build/macos/Build/Products/Release/mgcs.app"
