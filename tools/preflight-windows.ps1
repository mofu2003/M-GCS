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

# 取文件里第一条匹配的第一个捕获组；等价于 preflight.sh 的 `sed -n ... | head -n 1`。
# 用 [regex]::Match（只返回第一条）而不是 Select-String：CHANGELOG.md 有多个
# `## [x.y.z] - ` 条目，Select-String 返回的是数组，直接 `.Matches.Groups[1].Value`
# 会得到数组，后面的 `-ne` 变成元素级比较而恒为真，Windows CI 会误报版本不一致。
function Get-FirstCaptured {
    param([string]$Path, [string]$Pattern)

    $text = Get-Content -Path $Path -Raw
    $match = [regex]::Match(
        $text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if (-not $match.Success) { return $null }
    return $match.Groups[1].Value
}

Require-Command "flutter"
Require-Command "dart"
Require-Command "python"

$RequiredHeader = Join-Path $Root "third_party\mavlink\include\mavlink\v2.0\ardupilotmega\mavlink.h"
if (-not (Test-Path $RequiredHeader)) { throw "MAVLink headers missing. Run generate-mavlink.ps1 explicitly before preflight." }

# --- 后端门禁检查（与 preflight.sh 对齐） ---

Write-Host "[windows-preflight] 检查补丁格式 (git diff --check)"
git diff --check HEAD
if ($LASTEXITCODE -ne 0) { throw "git diff --check HEAD failed" }
git diff --cached --check
if ($LASTEXITCODE -ne 0) { throw "git diff --cached --check failed" }

Write-Host "[windows-preflight] 检查版本号一致性"
$cmakeVersion = Get-FirstCaptured (Join-Path $Root "CMakeLists.txt") '^project\(mgcore VERSION ([0-9][0-9.]*) LANGUAGES CXX'
$pubspecVersion = Get-FirstCaptured (Join-Path $Root "flutter_shell\pubspec.yaml") '^version: ([0-9][0-9.]*)\+[0-9]*$'
$ffiMajor = Get-FirstCaptured (Join-Path $Root "bridge\ffi\ffi_api_core.h") '^#define MGCORE_VERSION_MAJOR ([0-9]*)$'
$ffiMinor = Get-FirstCaptured (Join-Path $Root "bridge\ffi\ffi_api_core.h") '^#define MGCORE_VERSION_MINOR ([0-9]*)$'
$ffiPatch = Get-FirstCaptured (Join-Path $Root "bridge\ffi\ffi_api_core.h") '^#define MGCORE_VERSION_PATCH ([0-9]*)$'
$ffiVersion = "$ffiMajor.$ffiMinor.$ffiPatch"
if (-not $cmakeVersion -or $cmakeVersion -ne $pubspecVersion -or $cmakeVersion -ne $ffiVersion) {
    throw "版本号不一致：CMake=$cmakeVersion pubspec=$pubspecVersion ffi=$ffiVersion"
}
$changelogVersion = Get-FirstCaptured (Join-Path $Root "CHANGELOG.md") '^## \[([0-9][0-9.]*)\] - '
if ($changelogVersion -ne $cmakeVersion) {
    throw "最新发布条目与版本不一致：CHANGELOG=$changelogVersion CMake=$cmakeVersion"
}
Write-Host "[windows-preflight] 版本一致：$cmakeVersion"

Write-Host "[windows-preflight] 检查 FFI 声明与前端绑定"
python (Join-Path $Root "tools\check_ffi_contract.py")
if ($LASTEXITCODE -ne 0) { throw "check_ffi_contract.py failed" }

Write-Host "[windows-preflight] 检查固件参数字面量 (S6)"
python (Join-Path $Root "tools\check_firmware_literals.py")
if ($LASTEXITCODE -ne 0) { throw "check_firmware_literals.py failed" }

Write-Host "[windows-preflight] 检查假断言写法 (T2)"
python (Join-Path $Root "tools\check_fake_assertions.py")
if ($LASTEXITCODE -ne 0) { throw "check_fake_assertions.py failed" }
python (Join-Path $Root "tools\check_fake_assertions.py") --self-test
if ($LASTEXITCODE -ne 0) { throw "check_fake_assertions.py --self-test failed" }

Write-Host "[windows-preflight] 检查 FFI 公开头的成套变更 (F1)"
python (Join-Path $Root "tools\check_api_change_set.py") --self-test
if ($LASTEXITCODE -ne 0) { throw "check_api_change_set.py --self-test failed" }
python (Join-Path $Root "tools\check_api_change_set.py") --base HEAD
if ($LASTEXITCODE -ne 0) { throw "check_api_change_set.py failed" }

# --- 前端门禁检查 ---

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
