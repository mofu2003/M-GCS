param(
    [switch]$Release,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$FlutterDir = Join-Path $Root "flutter_shell"
$BuildMode = if ($Release) { "release" } else { "debug" }
$ConfigName = if ($Release) { "Release" } else { "Debug" }

function Require-Command {
    param([string]$Name)

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $Name"
    }
}

Require-Command "flutter"
Require-Command "dart"

$RequiredHeader = Join-Path $Root "third_party\mavlink\include\mavlink\v2.0\ardupilotmega\mavlink.h"
if (-not (Test-Path $RequiredHeader)) { throw "MAVLink headers missing. Run generate-mavlink.ps1 explicitly before preflight." }

Push-Location $FlutterDir
try {
    if (-not (Test-Path (Join-Path $FlutterDir "pubspec.lock"))) { throw "pubspec.lock missing; run flutter pub get explicitly before preflight." }

    Write-Host "[windows-preflight] UI token check"
    dart ../tools/check_ui_tokens.dart

    Write-Host "[windows-preflight] flutter analyze"
    flutter analyze lib test

    if (-not $SkipTests) {
        Write-Host "[windows-preflight] flutter test"
        flutter test
    }

    Write-Host "[windows-preflight] flutter build windows --$BuildMode"
    flutter build windows "--$BuildMode"

    $BundleDir = Join-Path $FlutterDir "build\windows\x64\runner\$ConfigName"
    dart ../tools/check_windows_bundle.dart $BundleDir

    Write-Host "[windows-preflight] Windows bundle: $BundleDir"
    Write-Host "[windows-preflight] All checks passed."
}
finally {
    Pop-Location
}
