# Builds MrMusic for Android arm64 and produces a self signed APK (TARGET.md §5).
#
#   pwsh scripts/package-android.ps1
#
param(
    [string]$QtDir = "D:\Qt\6.11.1\android_arm64_v8a",
    [string]$HostQtDir = "D:\Qt\6.11.1\msvc2022_64",
    [string]$SdkRoot = "D:\UsersFiles\Sordayne\Android\Sdk",
    [string]$NdkRoot = "D:\UsersFiles\Sordayne\Android\Sdk\ndk\27.2.12479018",
    [string]$BuildDir = "build-android"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $toolchain = Join-Path $QtDir "lib\cmake\Qt6\qt.toolchain.cmake"
    cmake -S . -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
        -DQT_HOST_PATH="$HostQtDir" `
        -DANDROID_SDK_ROOT="$SdkRoot" `
        -DANDROID_NDK_ROOT="$NdkRoot" `
        -DANDROID_ABI=arm64-v8a `
        -DANDROID_PLATFORM=android-29 `
        -DQT_ANDROID_BUILD_ALL_ABIS=OFF `
        -DQT_ANDROID_PACKAGE_SOURCE_DIR="$root/android"
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }

    cmake --build $BuildDir --target apk
    if ($LASTEXITCODE -ne 0) { throw "apk build failed" }

    Get-ChildItem -Recurse "$BuildDir\android-build\build\outputs\apk" -Filter *.apk |
        ForEach-Object { Write-Host "APK: $($_.FullName)" -ForegroundColor Green }
}
finally {
    Pop-Location
}
