# Builds MrMusic for Windows x64 and produces a self contained folder in dist/
# using `windeployqt` (TARGET.md §5).
#
#   pwsh scripts/package-windows.ps1 [-QtDir D:\Qt\6.11.1\msvc2022_64]
#
param(
    [string]$QtDir = "D:\Qt\6.11.1\msvc2022_64",
    [string]$BuildDir = "build",
    [string]$DistDir = "dist"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    Write-Host "== configure ==" -ForegroundColor Cyan
    cmake -S . -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtDir"
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

    Write-Host "== build ==" -ForegroundColor Cyan
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    Write-Host "== install ==" -ForegroundColor Cyan
    # only clear the Windows output, other platform artifacts may live in dist/
    if (Test-Path "$DistDir\bin") { Remove-Item -Recurse -Force "$DistDir\bin" }
    cmake --install $BuildDir --prefix "$DistDir"

    Write-Host "== windeployqt ==" -ForegroundColor Cyan
    $windeploy = Join-Path $QtDir "bin\windeployqt.exe"
    & $windeploy --release --qmldir "$root\qml" "$DistDir\bin\MrMusic.exe"
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

    # data directory next to the executable (TARGET.md §5.2)
    New-Item -ItemType Directory -Force -Path "$DistDir\bin\data" | Out-Null
    Write-Host "package ready: $DistDir\bin" -ForegroundColor Green
}
finally {
    Pop-Location
}
