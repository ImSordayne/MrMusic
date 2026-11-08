#!/usr/bin/env bash
# Builds the Android arm64 APK on Linux (used by GitHub Actions).
#
# Expects:
#   QT_ANDROID_PATH  - Qt for Android kit (…/android_arm64_v8a)
#   QT_HOST_PATH     - host Qt (…/gcc_64)
#   ANDROID_SDK_ROOT / ANDROID_NDK_ROOT
#   android/mrmusic.keystore (self signed, contains alias "mrmusic")
#
# Mirrors scripts/build-android.ps1 and produces dist/MrMusic-android-arm64.apk
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build-android}"
DIST_DIR="${DIST_DIR:-dist}"
KEYSTORE="${KEYSTORE:-android/mrmusic.keystore}"
KEYSTORE_ALIAS="${KEYSTORE_ALIAS:-mrmusic}"
KEYSTORE_PASS="${KEYSTORE_PASS:-mrmusic}"

: "${QT_ANDROID_PATH:?QT_ANDROID_PATH is required}"
: "${QT_HOST_PATH:?QT_HOST_PATH is required}"

echo "== configure (Android arm64) =="
cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$QT_ANDROID_PATH/lib/cmake/Qt6/qt.toolchain.cmake" \
    -DQT_HOST_PATH="$QT_HOST_PATH" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-29 \
    -DQT_ANDROID_BUILD_ALL_ABIS=OFF \
    -DQT_ANDROID_PACKAGE_SOURCE_DIR="$ROOT/android"

echo "== build =="
cmake --build "$BUILD_DIR"

echo "== apk =="
cmake --build "$BUILD_DIR" --target apk

UNSIGNED="$(find "$BUILD_DIR/android-build/build/outputs/apk" -name '*.apk' | head -n1)"
if [ -z "$UNSIGNED" ]; then
    echo "no APK produced" >&2
    exit 1
fi
echo "unsigned APK: $UNSIGNED"

BUILD_TOOLS="$(ls -d "${ANDROID_SDK_ROOT}"/build-tools/* | sort -V | tail -n1)"
mkdir -p "$DIST_DIR"
ALIGNED="$DIST_DIR/MrMusic-aligned.apk"
SIGNED="$DIST_DIR/MrMusic-android-arm64.apk"

"$BUILD_TOOLS/zipalign" -f -p 4 "$UNSIGNED" "$ALIGNED"
"$BUILD_TOOLS/apksigner" sign \
    --ks "$KEYSTORE" --ks-key-alias "$KEYSTORE_ALIAS" \
    --ks-pass "pass:${KEYSTORE_PASS}" --key-pass "pass:${KEYSTORE_PASS}" \
    --out "$SIGNED" "$ALIGNED"
"$BUILD_TOOLS/apksigner" verify --print-certs "$SIGNED"

echo "signed APK: $SIGNED"
