$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDir = if ($env:MGCORE_BUILD_DIR) { $env:MGCORE_BUILD_DIR } else { Join-Path $Root 'build_desktop' }
$Flutter = if ($env:FLUTTER_BIN) { $env:FLUTTER_BIN } else { 'flutter' }

& (Join-Path $PSScriptRoot 'generate-mavlink.ps1')
cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release

Push-Location (Join-Path $Root 'app')
try {
    & $Flutter --no-version-check pub get
    & $Flutter --no-version-check build windows --release
} finally {
    Pop-Location
}

Write-Host "Built application under $Root\app\build\windows\x64\runner\Release"
