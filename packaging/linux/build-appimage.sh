#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/packaging/linux/common.sh"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-release-appimage}"
DIST_DIR="${OUTPUT_DIR:-$ROOT_DIR/dist/appimage}"
APPDIR="${APPDIR:-$DIST_DIR/AppDir}"
GENERATOR="${CMAKE_GENERATOR:-Ninja}"
APPIMAGETOOL="${APPIMAGETOOL:-}"
APPIMAGE_BUNDLE_MPV="${QEMBY_APPIMAGE_BUNDLE_MPV:-ON}"


if [[ -z "$APPIMAGETOOL" ]]; then
    APPIMAGETOOL="$(command -v appimagetool || true)"
fi

if [[ -z "$APPIMAGETOOL" && -x "$ROOT_DIR/tools/appimagetool.AppImage" ]]; then
    APPIMAGETOOL="$ROOT_DIR/tools/appimagetool.AppImage"
fi

if [[ -z "$APPIMAGETOOL" && -x "$ROOT_DIR/tools/appimagetool-x86_64.AppImage" ]]; then
    APPIMAGETOOL="$ROOT_DIR/tools/appimagetool-x86_64.AppImage"
fi

if [[ -z "$APPIMAGETOOL" ]]; then
    echo "appimagetool not found." >&2
    echo "Please do one of the following:" >&2
    echo "  1. Install appimagetool into PATH" >&2
    echo "  2. Put appimagetool.AppImage in $ROOT_DIR/tools/" >&2
    echo "  3. Run with APPIMAGETOOL=/path/to/appimagetool.AppImage" >&2
    exit 1
fi

qemby_check_build_cache "$ROOT_DIR" "$BUILD_DIR"

qemby_validate_work_dir "$ROOT_DIR" "$DIST_DIR"
mkdir -p "$DIST_DIR"
qemby_reset_owned_appdir "$ROOT_DIR" "$APPDIR"

cmake_args=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -G "$GENERATOR"
    -DCMAKE_BUILD_TYPE=Release
    -DQEMBY_ENABLE_QT_DEPLOY=ON
    -DQEMBY_BUNDLE_MPV_RUNTIME="$APPIMAGE_BUNDLE_MPV"
)

if [[ -n "${QEMBY_QT_ROOT:-}" ]]; then
    cmake_args+=("-DQEMBY_QT_ROOT=$QEMBY_QT_ROOT")
fi

cmake "${cmake_args[@]}"

cmake --build "$BUILD_DIR" --config Release
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --config Release --prefix /usr

install -m 0755 "$ROOT_DIR/packaging/linux/AppRun" "$APPDIR/AppRun"
cp "$APPDIR/usr/share/applications/qemby.desktop" "$APPDIR/qemby.desktop"
sed -i 's|^Exec=.*|Exec=AppRun %U|' "$APPDIR/qemby.desktop"
ln -snf "usr/share/icons/hicolor/scalable/apps/qemby.svg" "$APPDIR/qemby.svg"
ln -snf "usr/share/icons/hicolor/scalable/apps/qemby.svg" "$APPDIR/.DirIcon"

VERSION="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
if [[ -z "$VERSION" ]]; then
    VERSION="$(sed -n 's/^PROJECT_VERSION:STATIC=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
fi
if [[ -z "$VERSION" ]]; then
    VERSION="$(sed -n 's/^project(qEmby VERSION \\([^ ]*\\) LANGUAGES CXX).*/\\1/p' "$ROOT_DIR/CMakeLists.txt")"
fi
ARCH="$(uname -m)"
APPIMAGE_PATH="$DIST_DIR/qemby-${VERSION}-${ARCH}.AppImage"

if [[ "$APPIMAGETOOL" == *.AppImage ]]; then
    chmod +x "$APPIMAGETOOL"
    "$APPIMAGETOOL" --appimage-extract-and-run "$APPDIR" "$APPIMAGE_PATH"
else
    "$APPIMAGETOOL" "$APPDIR" "$APPIMAGE_PATH"
fi

echo "AppImage output: $APPIMAGE_PATH"
