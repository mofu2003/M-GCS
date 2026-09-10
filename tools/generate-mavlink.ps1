param(
    [string]$PythonBin = $(if ($env:PYTHON_BIN) { $env:PYTHON_BIN } else { "python" })
)

$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$VenvDir = if ($env:MGCS_VENV_DIR) { $env:MGCS_VENV_DIR } else { Join-Path $Root ".venv" }
$OutputDir = if ($env:MAVLINK_OUTPUT_DIR) {
    $env:MAVLINK_OUTPUT_DIR
} else {
    Join-Path $Root "third_party\mavlink\include\mavlink\v2.0"
}
$CommonHeader = Join-Path $OutputDir "common\mavlink.h"
$VenvPython = Join-Path $VenvDir "Scripts\python.exe"
$Mavgen = Join-Path $VenvDir "Scripts\mavgen.py"

if (Test-Path $CommonHeader) {
    exit 0
}

Write-Host "[mgcs] Generating MAVLink C headers with pymavlink 2.4.49"
if (-not (Test-Path $Mavgen)) {
    & $PythonBin -m venv $VenvDir
    & $VenvPython -m pip install --disable-pip-version-check "pymavlink==2.4.49"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
& $VenvPython $Mavgen `
    --no-validate `
    --lang=C `
    --wire-protocol=2.0 `
    --output=$OutputDir `
    (Join-Path $Root "third_party\mavlink\message_definitions\v1.0\ardupilotmega.xml")
