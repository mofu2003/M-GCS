#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "用法：tools/release.sh <major.minor.patch>" >&2
  exit 2
fi

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$1"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "[release] 版本号必须形如 0.1.0。" >&2
  exit 2
fi

cmake_file="$root_dir/CMakeLists.txt"
pubspec_file="$root_dir/flutter_shell/pubspec.yaml"
readme_file="$root_dir/README.md"
changelog_file="$root_dir/CHANGELOG.md"
ffi_header="$root_dir/bridge/ffi/ffi_api_core.h"

# Validate required documents before touching any version file.
for required_file in "$readme_file" "$changelog_file" "$ffi_header"; do
  if [[ ! -f "$required_file" ]]; then
    echo "[release] 缺少必需文件：$required_file" >&2
    exit 1
  fi
done
if ! grep -q '当前版本：`[0-9][0-9.]*`' "$readme_file" ||
   ! grep -Eq '^## \[[0-9]+\.[0-9]+\.[0-9]+\] - ' "$changelog_file"; then
  echo "[release] README 或 CHANGELOG 缺少可更新的版本标记。" >&2
  exit 1
fi

current_version="$(
  sed -n 's/^project(mgcore VERSION \([0-9][0-9.]*\) LANGUAGES CXX.*/\1/p' \
    "$cmake_file"
)"
current_pubspec_version="$(
  sed -n 's/^version: \([0-9][0-9.]*\)+[0-9][0-9]*$/\1/p' \
    "$pubspec_file"
)"
current_build="$(
  sed -n 's/^version: [0-9][0-9.]*+\([0-9][0-9]*\)$/\1/p' \
    "$pubspec_file"
)"

if [[ -z "$current_version" || -z "$current_pubspec_version" || -z "$current_build" ]]; then
  echo "[release] 无法解析当前版本号或构建号。" >&2
  exit 1
fi
if [[ "$current_version" != "$current_pubspec_version" ]]; then
  echo "[release] CMake 版本 $current_version 与 Flutter 版本 $current_pubspec_version 不一致。" >&2
  exit 1
fi

next_build=$((current_build + 1))
next_pubspec_version="$version+$next_build"
today="$(date +%F)"

VERSION="$version" perl -0pi -e \
  's/project\(mgcore VERSION \K[0-9]+\.[0-9]+\.[0-9]+(?= LANGUAGES CXX)/$ENV{VERSION}/' \
  "$cmake_file"

NEXT_VERSION="$next_pubspec_version" perl -0pi -e \
  's/^version: \K[0-9]+\.[0-9]+\.[0-9]+\+[0-9]+$/$ENV{NEXT_VERSION}/m' \
  "$pubspec_file"

# Keep the FFI version macros (mg_get_version) in sync with the project version.
major="${version%%.*}"
rest="${version#*.}"
minor="${rest%%.*}"
patch="${rest#*.}"
perl -0pi -e "s/#define MGCORE_VERSION_MAJOR \K[0-9]+/$major/" "$ffi_header"
perl -0pi -e "s/#define MGCORE_VERSION_MINOR \K[0-9]+/$minor/" "$ffi_header"
perl -0pi -e "s/#define MGCORE_VERSION_PATCH \K[0-9]+/$patch/" "$ffi_header"

# README / CHANGELOG are required release metadata.
VERSION="$version" perl -0pi -e \
  's/当前版本：`\K[0-9]+\.[0-9]+\.[0-9]+(?=`)/$ENV{VERSION}/' \
  "$readme_file"

if ! grep -Fq "## [$version] - " "$changelog_file"; then
  VERSION="$version" RELEASE_DATE="$today" perl -0pi -e \
    's/^(## \[[0-9]+\.[0-9]+\.[0-9]+\] - )/## [$ENV{VERSION}] - $ENV{RELEASE_DATE}\n\n### Changed\n\n- 待补充。\n\n### Verification\n\n- 待补充。\n\n$1/m' \
    "$changelog_file"
fi

docs_ok=1
if ! grep -Fq "当前版本：\`$version\`" "$readme_file"; then
  docs_ok=0
fi
if ! grep -Fq "## [$version] - " "$changelog_file"; then
  docs_ok=0
fi

if ! grep -q "#define MGCORE_VERSION_MAJOR $major" "$ffi_header" ||
  ! grep -q "#define MGCORE_VERSION_MINOR $minor" "$ffi_header" ||
  ! grep -q "#define MGCORE_VERSION_PATCH $patch" "$ffi_header" ||
  ! grep -q "project(mgcore VERSION $version LANGUAGES CXX" "$cmake_file" ||
  ! grep -q "^version: $next_pubspec_version$" "$pubspec_file" ||
  [[ "$docs_ok" -eq 0 ]]; then
  echo "[release] 版本文件校验失败，请检查版本号。" >&2
  exit 1
fi

git -C "$root_dir" diff --check

echo "[release] 已更新版本号：${current_version} -> ${version}（build ${current_build} -> ${next_build}）"
git -C "$root_dir" diff --stat -- \
  CMakeLists.txt \
  flutter_shell/pubspec.yaml \
  bridge/ffi/ffi_api_core.h
if [[ -f "$changelog_file" ]]; then
  git -C "$root_dir" diff --stat -- CHANGELOG.md
fi
echo "[release] 请运行 tools/preflight.sh 后本地提交和打标签。"
