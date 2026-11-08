#!/usr/bin/env bash
# Builds MrMusic on Linux and packages it as an AppImage (TARGET.md §5).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
DIST_DIR="${DIST_DIR:-dist}"

cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DQT_DEPLOY_USE_PATCHELF=ON
cmake --build "$BUILD_DIR"

rm -rf AppDir "$DIST_DIR"
mkdir -p "$DIST_DIR"
DESTDIR="$ROOT/AppDir" cmake --install "$BUILD_DIR"

# desktop entry + icon required by AppImage
install -D -m 644 packaging/mrmusic.desktop AppDir/usr/share/applications/mrmusic.desktop
install -D -m 644 res/logo-mark.svg AppDir/usr/share/icons/hicolor/scalable/apps/mrmusic.svg

if ! command -v linuxdeploy >/dev/null 2>&1; then
    echo "linuxdeploy not found, downloading"
    curl -fsSLo linuxdeploy https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    curl -fsSLo linuxdeploy-plugin-qt https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x linuxdeploy linuxdeploy-plugin-qt
    export PATH="$PWD:$PATH"
fi

# Works inside CI containers where FUSE is unavailable.
export APPIMAGE_EXTRACT_AND_RUN=1
export QML_SOURCES_PATHS="$ROOT/qml"
export EXTRA_QT_MODULES="${EXTRA_QT_MODULES:-multimedia;svg;networkinformation}"

linuxdeploy --appdir AppDir \
    --desktop-file AppDir/usr/share/applications/mrmusic.desktop \
    --icon-file res/logo-mark.svg \
    --plugin qt \
    --output appimage

mv ./*.AppImage "$DIST_DIR/" 2>/dev/null || true
echo "package ready: $DIST_DIR"
