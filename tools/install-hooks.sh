#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
hooks_dir="$root_dir/.githooks"
hook_file="$hooks_dir/pre-commit"

if [[ ! -f "$hook_file" ]]; then
  echo "[hooks] 缺少 $hook_file" >&2
  exit 1
fi

chmod +x "$hook_file"
git -C "$root_dir" config core.hooksPath .githooks

echo "[hooks] 已启用仓库 hooks：$(git -C "$root_dir" config --get core.hooksPath)"
echo "[hooks] pre-commit 会执行 tools/preflight.sh。"
