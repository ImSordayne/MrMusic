# Rasterises UiDesign/Logo.svg into every icon the app ships.
#
#   pwsh scripts/render-icons.ps1
#
# Requires the Qt environment (the app itself renders the SVG through
# QSvgRenderer, so no extra tooling is needed). Run it from a shell that has
# been set up with qtenv2.bat + vcvars64.bat, e.g.:
#
#   cmd /c 'call "D:\Qt\6.11.1\msvc2022_64\bin\qtenv2.bat" && call "...\vcvars64.bat" && pwsh scripts/render-icons.ps1'

param(
    [string]$Exe = "build\MrMusic.exe",
    [string]$Svg = "UiDesign\Logo.svg"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if (-not (Test-Path $Exe)) { throw "$Exe not found - build the project first" }
    if (-not (Test-Path $Svg)) { throw "$Svg not found" }

    function Render([string]$out, [int]$px, [string]$background = "") {
        $renderArgs = @("--render-svg", $Svg, $out, "$px")
        if ($background) { $renderArgs += $background }
        # MrMusic is a GUI subsystem binary: Start-Process -Wait actually blocks.
        $process = Start-Process -FilePath $Exe -ArgumentList $renderArgs -Wait -PassThru -NoNewWindow
        if ($process.ExitCode -ne 0) { throw "render failed: $out (exit $($process.ExitCode))" }
        if (-not (Test-Path $out)) { throw "render produced no file: $out" }
        Write-Host ("{0,-42} {1}" -f $out, $px)
    }

    # transparent master (docs / light backgrounds)
    Render "UiDesign/Logo.png" 1024

    # app icons: rounded plate so the black artwork stays visible in a taskbar
    Render "res/app.png" 256 "#FFFFFF:round"
    Render "res/logo-512.png" 512 "#FFFFFF:round"

    # Android launcher icons
    $mipmaps = @{ mdpi = 48; hdpi = 72; xhdpi = 96; xxhdpi = 144; xxxhdpi = 192 }
    foreach ($name in $mipmaps.Keys) {
        $dir = "android\res\mipmap-$name"
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        Render "$dir/ic_launcher.png" $mipmaps[$name] "#FFFFFF:round"
    }

    Write-Host "icons rendered from $Svg" -ForegroundColor Green
}
finally {
    Pop-Location
}
