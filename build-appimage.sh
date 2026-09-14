#!/usr/bin/env bash
set -euo pipefail

# Builds a self-contained AppImage using linuxdeploy + linuxdeploy-plugin-qt.
# Based on: /home/david/dev/QLogueLibrarian/build-appimage.sh
#
# Usage: ./build-appimage.sh [Debug|Release]
#
# Optional env:
#   BUILD_DIR              CMake build + AppImage output dir (default: appImage)
#   TOOLS_DIR              where linuxdeploy tools are cached (default: $BUILD_DIR/tools)
#   QRTWHISPER_EXTRA_CMAKE_ARGS  extra flags for the configure step
#
# Notes:
#   - The default models directory is "./models" relative to the executable
#     (or next to the AppImage file), and that location is persisted via QSettings.
#     The user can change it from the Model Management window. If the stored
#     directory does not exist, the app falls back to the default and saves it.
#   - libggml-cuda links the host CUDA driver (libcuda.so.1). linuxdeploy may
#     bundle it; if the AppImage misbehaves on another machine, exclude the
#     driver libs from the bundle (the GPU driver must come from the host).

BUILD_DIR="${BUILD_DIR:-appImage}"
CONFIG="${1:-Release}"
TOOLS_DIR="${TOOLS_DIR:-$BUILD_DIR/tools}"
APP_ID="QRTWhisper"
APPDIR="$BUILD_DIR/AppDir"
DESKTOP_FILE="$BUILD_DIR/${APP_ID}.desktop"

# 1) Fetch linuxdeploy + Qt plugin (AppImage packages)
mkdir -p "$TOOLS_DIR"
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
QT_PLUGIN="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
if [ ! -f "$LINUXDEPLOY" ]; then
    echo "Downloading linuxdeploy..."
    curl -fL -o "$LINUXDEPLOY" \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fi
if [ ! -f "$QT_PLUGIN" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    curl -fL -o "$QT_PLUGIN" \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fi
chmod +x "$LINUXDEPLOY" "$QT_PLUGIN"

# AppImage tools need FUSE; extract-and-run avoids requiring it
export APPIMAGE_EXTRACT_AND_RUN=1

# 2) Configure (reuse existing build dir if already configured) + build
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" ${QRTWHISPER_EXTRA_CMAKE_ARGS:-}
fi
cmake --build "$BUILD_DIR" -j"$(nproc)"

# 3) Assemble AppDir manually (no install prefix, no system paths)
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/doc/${APP_ID}"
cp "$BUILD_DIR/${APP_ID}" "$APPDIR/usr/bin/"
cp COPYING "$APPDIR/usr/share/doc/${APP_ID}/"

# 4) Desktop entry + icon (both required by linuxdeploy)
cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=QRTWhisper
GenericName=Real-time subtitles
Comment=A real time Qt interface for Whisper.cpp (accessibility tool)
Exec=${APP_ID}
Icon=${APP_ID}
Terminal=false
Categories=Utility;AudioVideo;Audio;Accessibility;
EOF

ICON_DIR="$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$ICON_DIR"
if command -v convert >/dev/null 2>&1; then
    convert resources/icon.png -resize 256x256 "$ICON_DIR/${APP_ID}.png"
else
    echo "WARNING: ImageMagick 'convert' not found; using icon.png as-is." >&2
    cp resources/icon.png "$ICON_DIR/${APP_ID}.png"
fi
mkdir -p "$APPDIR/usr/share/applications"
cp "$DESKTOP_FILE" "$APPDIR/usr/share/applications/${APP_ID}.desktop"

# 5) Bundle Qt (and dependencies) and build the AppImage
export LDAI_OUTPUT_DIR="$BUILD_DIR"
"$LINUXDEPLOY" --appdir "$APPDIR" --plugin qt --output appimage

# Ensure the result lands inside $BUILD_DIR (plugin may honor relative LDAI_OUTPUT_DIR differently)
mv -f ./${APP_ID}-*.AppImage "$BUILD_DIR/" 2>/dev/null || true

echo "AppImage created in: $BUILD_DIR/${APP_ID}-x86_64.AppImage"
