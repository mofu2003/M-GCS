#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
command_name="${1:-shell}"
shift || true

cd "$root_dir"
exec docker compose run --rm mgcore-dev "$command_name" "$@"
