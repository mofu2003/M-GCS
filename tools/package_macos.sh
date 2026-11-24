#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "用法：tools/package_macos.sh <app> <version> <arm64|x64> <output.dmg>" >&2
  exit 2
fi

app_path="$1"
version="$2"
architecture="$3"
output_path="$4"

if [[ ! -d "$app_path" ]]; then
  echo "[macos-package] 应用不存在：$app_path" >&2
  exit 1
fi
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "[macos-package] 版本号必须形如 1.3.0。" >&2
  exit 2
fi
if [[ "$architecture" != "arm64" && "$architecture" != "x64" ]]; then
  echo "[macos-package] 架构必须是 arm64 或 x64。" >&2
  exit 2
fi

sign_identity="${MGC_SIGN_IDENTITY:-}"
if [[ -n "$sign_identity" && "$sign_identity" != "-" ]]; then
  echo "[macos-package] 使用 Developer ID 重新签名应用。" >&2
  codesign \
    --force \
    --deep \
    --timestamp \
    --options runtime \
    --preserve-metadata=entitlements \
    --sign "$sign_identity" \
    "$app_path"
  codesign --verify --deep --strict "$app_path"
elif ! codesign --verify --deep --strict "$app_path" >/dev/null 2>&1; then
  echo "[macos-package] 构建产物签名无效，改用 ad-hoc 签名后打包。" >&2
  codesign --force --deep --sign - "$app_path"
  codesign --verify --deep --strict "$app_path"
fi

stage_dir="$(mktemp -d "${TMPDIR:-/tmp}/mgcs-dmg.XXXXXX")"
trap 'rm -rf "$stage_dir"' EXIT

ditto "$app_path" "$stage_dir/M-GCS.app"
ln -s /Applications "$stage_dir/Applications"

mkdir -p "$(dirname "$output_path")"
hdiutil create \
  -volname "M-GCS $version ($architecture)" \
  -srcfolder "$stage_dir" \
  -ov \
  -format UDZO \
  "$output_path"

echo "[macos-package] 已生成：$output_path"
