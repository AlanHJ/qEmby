#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates curl git build-essential cmake ninja-build pkg-config python3 file \
  patchelf binutils desktop-file-utils libmpv-dev libgl1-mesa-dev \
  qt6-base-dev qt6-base-private-dev libqt6opengl6-dev qt6-tools-dev qt6-tools-dev-tools \
  qt6-l10n-tools libqt6websockets6-dev libqt6svg6-dev qt6-wayland xkb-data \
  libxkbcommon-dev libxkbcommon-x11-dev libxcb-cursor-dev libx11-xcb-dev \
  libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
  libxcb-render-util0-dev libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev \
  libxcb-xkb-dev libxcb-util-dev ffmpeg
git config --global --add safe.directory "$GITHUB_WORKSPACE"
python3 scripts/release/verify_source.py
mkdir -p "$RUNNER_TEMP/appimage-tools"
export APPIMAGETOOL="$RUNNER_TEMP/appimage-tools/appimagetool.AppImage"
export APPIMAGE_RUNTIME_FILE="$RUNNER_TEMP/appimage-tools/runtime-x86_64"
curl --fail --location --retry 3 \
  https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-x86_64.AppImage \
  --output "$APPIMAGETOOL"
curl --fail --location --retry 3 \
  https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64 \
  --output "$APPIMAGE_RUNTIME_FILE"
printf '%s  %s\n' ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0 "$APPIMAGETOOL" \
  2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d "$APPIMAGE_RUNTIME_FILE" | sha256sum --check
export BUILD_DIR="$GITHUB_WORKSPACE/build/cloud"
export OUTPUT_DIR="$GITHUB_WORKSPACE/dist/appimage"
export CMAKE_BUILD_PARALLEL_LEVEL=2
export QEMBY_PLAYBACK_PROBE=ON
bash packaging/linux/build-appimage.sh
# Diagnostic executable and generated local clip are separate CI artifacts;
# neither is installed in the released AppImage.
cmake --build "$BUILD_DIR" --target qEmbyPlaybackProbe --parallel 2
mkdir -p "$GITHUB_WORKSPACE/dist/appimage-probe"
cp "$BUILD_DIR/bin/qEmbyPlaybackProbe" "$GITHUB_WORKSPACE/dist/appimage-probe/"
patchelf --set-rpath '$ORIGIN' "$GITHUB_WORKSPACE/dist/appimage-probe/qEmbyPlaybackProbe"
ffmpeg -loglevel error -f lavfi -i 'testsrc2=size=640x360:rate=30' \
  -t 20 -c:v mpeg4 -q:v 4 "$GITHUB_WORKSPACE/dist/appimage-probe/sample.mp4"
