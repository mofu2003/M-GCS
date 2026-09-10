$ErrorActionPreference = 'Stop'

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$ToolsDir = Join-Path $Root '.tools'
$BuildDir = if ($env:MGCORE_BUILD_DIR) { $env:MGCORE_BUILD_DIR } else { Join-Path $Root 'build_desktop' }
$Cmake = if ($env:CMAKE_BIN) {
    $env:CMAKE_BIN
} elseif (Test-Path (Join-Path $ToolsDir 'cmake\bin\cmake.exe')) {
    Join-Path $ToolsDir 'cmake\bin\cmake.exe'
} else {
    'cmake'
}
$Flutter = if ($env:FLUTTER_BIN) {
    $env:FLUTTER_BIN
} elseif (Test-Path (Join-Path $ToolsDir 'flutter\bin\flutter.bat')) {
    Join-Path $ToolsDir 'flutter\bin\flutter.bat'
} else {
    'flutter'
}
$PubCache = if ($env:MGCS_PUB_CACHE) { $env:MGCS_PUB_CACHE } else { Join-Path $ToolsDir 'pub-cache' }

$PreviousPath = $env:PATH
$HadPubCache = Test-Path Env:PUB_CACHE
$PreviousPubCache = $env:PUB_CACHE
$HadMgcoreDllPath = Test-Path Env:MGCORE_DLL_PATH
$PreviousMgcoreDllPath = $env:MGCORE_DLL_PATH

try {
    $env:PATH = "$(Split-Path -Parent $Cmake);$env:PATH"
    $env:PUB_CACHE = $PubCache
    $env:MGCORE_DLL_PATH = Join-Path $Root 'build\mgcore.dll'

    & (Join-Path $PSScriptRoot 'generate-mavlink.ps1')
    & $Cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }
    & $Cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }

    if (-not (Test-Path $env:MGCORE_DLL_PATH)) {
        throw "Expected Windows DLL was not produced: $env:MGCORE_DLL_PATH"
    }

    Push-Location (Join-Path $Root 'app')
    try {
        & $Flutter --no-version-check pub get
        if ($LASTEXITCODE -ne 0) {
            throw "Flutter pub get failed with exit code $LASTEXITCODE"
        }
        & $Flutter --no-version-check build windows --release
        if ($LASTEXITCODE -ne 0) {
            throw "Flutter Windows build failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
} finally {
    $env:PATH = $PreviousPath
    if ($HadPubCache) {
        $env:PUB_CACHE = $PreviousPubCache
    } else {
        Remove-Item Env:PUB_CACHE
    }
    if ($HadMgcoreDllPath) {
        $env:MGCORE_DLL_PATH = $PreviousMgcoreDllPath
    } else {
        Remove-Item Env:MGCORE_DLL_PATH
    }
}

Write-Host "Built application under $Root\app\build\windows\x64\runner\Release"
