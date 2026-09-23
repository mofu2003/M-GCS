#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
python_bin="${PYTHON_BIN:-python3}"
venv_dir="${MGCS_VENV_DIR:-$root_dir/.venv}"
output_dir="${MAVLINK_OUTPUT_DIR:-$root_dir/third_party/mavlink/include/mavlink/v2.0}"
required_header="$output_dir/ardupilotmega/mavlink.h"

if [[ -f "$required_header" ]]; then
  exit 0
fi

echo "[mgcs] Generating MAVLink C headers with pymavlink 2.4.49"
venv_python="$venv_dir/bin/python"
mavgen="$venv_dir/bin/mavgen.py"
if [[ -x "$venv_dir/Scripts/python.exe" ]]; then
  venv_python="$venv_dir/Scripts/python.exe"
  mavgen="$venv_dir/Scripts/mavgen.py"
fi

if [[ ! -f "$mavgen" ]]; then
  "$python_bin" -m venv "$venv_dir"
  if [[ -x "$venv_dir/Scripts/python.exe" ]]; then
    venv_python="$venv_dir/Scripts/python.exe"
    mavgen="$venv_dir/Scripts/mavgen.py"
  fi
  "$venv_python" -m pip install --disable-pip-version-check \
    "pymavlink==2.4.49"
fi

mkdir -p "$output_dir"
"$venv_python" "$mavgen" \
  --no-validate \
  --lang=C \
  --wire-protocol=2.0 \
  --output="$output_dir" \
  "$root_dir/third_party/mavlink/message_definitions/v1.0/ardupilotmega.xml"
