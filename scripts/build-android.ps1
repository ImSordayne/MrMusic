# Configures and builds the Android arm64 target and produces the APK.
#
#   pwsh scripts/build-android.ps1 [-Clean]
#
param(
    [switch]$Clean,
    [string]$QtAndroidDir = "D:\Qt\6.11.1\android_arm64_v8a",
    [string]$QtHostDir = "D:\Qt\6.11.1\msvc2022_64",
    [string]$SdkRoot = "D:\UsersFiles\Sordayne\Android\Sdk",
    [string]$NdkRoot = "D:\UsersFiles\Sordayne\Android\Sdk\ndk\27.2.12479018",
    [string]$JdkRoot = "D:\Java\Java17.0.13",
    [string]$BuildDir = "build-android"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    $env:JAVA_HOME = $JdkRoot
    $env:PATH = "D:\Qt\Tools\CMake_64\bin;D:\Qt\Tools\Ninja;$JdkRoot\bin;$env:PATH"
    $env:ANDROID_SDK_ROOT = $SdkRoot
    $env:ANDROID_NDK_ROOT = $NdkRoot

    $cmake = "D:\Qt\Tools\CMake_64\bin\cmake.exe"
    $ninja = "D:\Qt\Tools\Ninja\ninja.exe"

    if ($Clean -and (Test-Path $BuildDir)) {
        Remove-Item -Recurse -Force $BuildDir
    }

    $toolchain = "$($QtAndroidDir -replace '\\','/')/lib/cmake/Qt6/qt.toolchain.cmake"
    $chainload = "$($NdkRoot -replace '\\','/')/build/cmake/android.toolchain.cmake"
    $hostPath = $QtHostDir -replace '\\','/'
    $ndk = $NdkRoot -replace '\\','/'
    $sdk = $SdkRoot -replace '\\','/'

    Write-Host "== configure (Android arm64) ==" -ForegroundColor Cyan
    & $cmake -S . -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
        -DQT_CHAINLOAD_TOOLCHAIN_FILE="$chainload" `
        -DANDROID_NDK="$ndk" `
        -DCMAKE_MAKE_PROGRAM="$ninja" `
        -DQT_HOST_PATH="$hostPath" `
        -DANDROID_SDK_ROOT="$sdk" `
        -DANDROID_NDK_ROOT="$ndk" `
        -DANDROID_ABI=arm64-v8a `
        -DANDROID_PLATFORM=android-29 `
        -DQT_ANDROID_BUILD_ALL_ABIS=OFF `
        -DQT_ANDROID_PACKAGE_SOURCE_DIR="$root/android"
    if ($LASTEXITCODE -ne 0) { throw "configure failed" }

    Write-Host "== build ==" -ForegroundColor Cyan
    & $cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "build failed" }

    Write-Host "== apk ==" -ForegroundColor Cyan
    & $cmake --build $BuildDir --target apk
    if ($LASTEXITCODE -ne 0) { throw "apk target failed" }

    $unsigned = Get-ChildItem -Recurse (Join-Path $BuildDir "android-build\build\outputs\apk") -Filter *-unsigned.apk -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $unsigned) {
        $unsigned = Get-ChildItem -Recurse (Join-Path $BuildDir "android-build\build\outputs\apk") -Filter *.apk -ErrorAction SilentlyContinue |
            Select-Object -First 1
    }
    Write-Host "unsigned APK: $($unsigned.FullName)" -ForegroundColor Green

    # --- self signing (TARGET.md §5: 自签名 APK) --------------------------
    $buildTools = Get-ChildItem (Join-Path $SdkRoot "build-tools") -Directory |
        Sort-Object { [version]($_.Name -replace '[^0-9\.]', '') } -Descending | Select-Object -First 1
    $zipalign = Join-Path $buildTools.FullName "zipalign.exe"
    $apksigner = Join-Path $buildTools.FullName "apksigner.bat"
    $keytool = Join-Path $JdkRoot "bin\keytool.exe"

    $keystore = Join-Path $root "android\mrmusic.keystore"
    if (-not (Test-Path $keystore)) {
        Write-Host "== generating self signed keystore ==" -ForegroundColor Cyan
        & $keytool -genkeypair -v -keystore $keystore -alias mrmusic `
            -keyalg RSA -keysize 2048 -validity 36500 `
            -storepass mrmusic -keypass mrmusic `
            -dname "CN=MrMusic, OU=MrMusic, O=MrMusic, L=, S=, C=CN"
        if ($LASTEXITCODE -ne 0) { throw "keytool failed" }
    }

    $distDir = Join-Path $root "dist"
    New-Item -ItemType Directory -Force -Path $distDir | Out-Null
    $aligned = Join-Path $distDir "MrMusic-aligned.apk"
    $signed = Join-Path $distDir "MrMusic-android-arm64.apk"

    & $zipalign -f -p 4 $unsigned.FullName $aligned
    if ($LASTEXITCODE -ne 0) { throw "zipalign failed" }

    & $apksigner sign --ks $keystore --ks-key-alias mrmusic `
        --ks-pass pass:mrmusic --key-pass pass:mrmusic `
        --out $signed $aligned
    if ($LASTEXITCODE -ne 0) { throw "apksigner failed" }

    & $apksigner verify --verbose $signed | Select-Object -First 5
    Write-Host "signed APK: $signed" -ForegroundColor Green
}
finally {
    Pop-Location
}
