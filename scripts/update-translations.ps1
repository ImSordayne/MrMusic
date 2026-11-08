# Updates the translation sources and rebuilds the embedded .qm file.
#
#   pwsh scripts/update-translations.ps1
#
param(
    [string]$QtDir = "D:\Qt\6.11.1\msvc2022_64"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $lupdate = Join-Path $QtDir "bin\lupdate.exe"
    $lrelease = Join-Path $QtDir "bin\lrelease.exe"

    & $lupdate qml src -ts translations/mrmusic_en_US.ts -no-obsolete
    & $lrelease translations/mrmusic_en_US.ts -qm translations/mrmusic_en_US.qm

    Write-Host "translations updated" -ForegroundColor Green
}
finally {
    Pop-Location
}
