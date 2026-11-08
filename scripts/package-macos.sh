#!/usr/bin/env bash
# Builds MrMusic on macOS and packages it with macdeployqt (TARGET.md §5).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"

PREFIX_ARGS=()
if [ -n "${QT_ROOT_DIR:-}" ]; then
    PREFIX_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_ROOT_DIR")
fi

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "${PREFIX_ARGS[@]}"
cmake --build "$BUILD_DIR"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

APP="$BUILD_DIR/MrMusic.app"
if [ ! -d "$APP" ]; then
    echo "MrMusic.app not found in $BUILD_DIR" >&2
    exit 1
fi

if [ -z "${QT_ROOT_DIR:-}" ]; then
    echo "QT_ROOT_DIR is not set (needed for macdeployqt)" >&2
    exit 1
fi

"$QT_ROOT_DIR/bin/macdeployqt" "$APP" -qmldir="$ROOT/qml" -dmg -always-overwrite
mv "$BUILD_DIR"/MrMusic*.dmg "$DIST_DIR/" 2>/dev/null || cp -R "$APP" "$DIST_DIR/"
echo "package ready: $DIST_DIR"
