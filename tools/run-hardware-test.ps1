param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("dump", "scan", "calibration", "mission", "modes", "px4ops")]
    [string]$Test,

    [string]$Port,

    [int]$Baud = 115200,

    [switch]$Quick,

    [switch]$KeepLog,

    [string]$BuildDir = "build",

    [string[]]$ExtraArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))

if (-not (Test-Path -LiteralPath $resolvedBuildDir)) {
    throw "Build directory not found: $resolvedBuildDir"
}

$testExecutables = @{
    dump        = "test_real_fc_dump.exe"
    scan        = "test_scan_connect.exe"
    calibration = "test_real_fc_calibration.exe"
    mission     = "test_real_fc_cal_mission.exe"
    modes       = "test_real_fc_modes.exe"
    px4ops      = "test_real_fc_px4_ops.exe"
}

$artifactPatterns = @(
    "real_fc_dump_*.txt",
    "hardware_*.log",
    "*_identity.txt"
)

function Remove-StaleHardwareArtifacts {
    param(
        [string]$Directory
    )

    $staleFiles = foreach ($pattern in $artifactPatterns) {
        Get-ChildItem -LiteralPath $Directory -File -Filter $pattern -ErrorAction SilentlyContinue
    }

    $staleFiles = $staleFiles | Sort-Object FullName -Unique
    if (-not $staleFiles) {
        Write-Host "[hardware-test] No stale hardware log artifacts found."
        return
    }

    foreach ($file in $staleFiles) {
        Remove-Item -LiteralPath $file.FullName -Force
        Write-Host "[hardware-test] Removed stale artifact: $($file.Name)"
    }
}

function Resolve-Port {
    param(
        [string]$RequestedPort
    )

    if ($RequestedPort) {
        return $RequestedPort
    }

    $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    if (-not $ports -or $ports.Count -eq 0) {
        throw "No serial ports detected. Pass -Port explicitly after connecting the FC."
    }

    if ($ports.Count -gt 1) {
        throw "Multiple serial ports detected ($($ports -join ', ')). Pass -Port explicitly."
    }

    return $ports[0]
}

function Invoke-HardwareTest {
    param(
        [string]$ExecutablePath,
        [string[]]$Arguments,
        [switch]$PersistLog
    )

    if (-not $PersistLog) {
        & $ExecutablePath @Arguments
        return $LASTEXITCODE
    }

    $logDir = Join-Path $resolvedBuildDir "hardware-logs"
    if (-not (Test-Path -LiteralPath $logDir)) {
        New-Item -ItemType Directory -Path $logDir | Out-Null
    }

    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $logName = "hardware-$Test-$timestamp.log"
    $logPath = Join-Path $logDir $logName

    Write-Host "[hardware-test] Keeping log: $logPath"

    & $ExecutablePath @Arguments 2>&1 | Tee-Object -FilePath $logPath
    return $LASTEXITCODE
}

$exeName = $testExecutables[$Test]
$exePath = Join-Path $resolvedBuildDir $exeName
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Executable not found: $exePath"
}

Remove-StaleHardwareArtifacts -Directory $resolvedBuildDir

$resolvedPort = Resolve-Port -RequestedPort $Port
$arguments = @($resolvedPort)

if ($Test -eq "dump" -and $Quick) {
    $arguments += "--quick"
}
if ($ExtraArgs.Count -gt 0) {
    $arguments += $ExtraArgs
}

Write-Host "[hardware-test] Test: $Test"
Write-Host "[hardware-test] Port: $resolvedPort"
Write-Host "[hardware-test] Baud: $Baud"
if (-not $KeepLog) {
    Write-Host "[hardware-test] Logging: disabled (default)"
}

$exitCode = Invoke-HardwareTest -ExecutablePath $exePath -Arguments $arguments -PersistLog:$KeepLog
exit $exitCode
