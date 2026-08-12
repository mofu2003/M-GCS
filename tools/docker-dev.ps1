param(
    [Parameter(Position = 0)]
    [string]$Command = "shell",

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$CommandArgs
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

docker compose run --rm mgcs-dev $Command @CommandArgs
exit $LASTEXITCODE
