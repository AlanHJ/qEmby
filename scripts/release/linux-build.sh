#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
if [[ "${1:-}" == --prepare ]]; then
apt-get update
available_package() {
  local candidate
  for candidate in "$@"; do
    if apt-cache show "$candidate" >/dev/null 2>&1; then
      printf '%s' "$candidate"
      return 0
    fi
  done
  printf 'No package available for %s\n' "$*" >&2
  return 1
}
svg_dev="$(available_package qt6-svg-dev libqt6svg6-dev)"
websockets_dev="$(available_package qt6-websockets-dev libqt6websockets6-dev)"
svg_plugins=()
if apt-cache show qt6-svg-plugins >/dev/null 2>&1; then
  svg_plugins+=(qt6-svg-plugins)
fi
apt-get install -y --no-install-recommends \
  ca-certificates git build-essential cmake ninja-build pkg-config python3 file \
  dpkg-dev libmpv-dev libgl1-mesa-dev libqt6opengl6-dev qt6-base-dev qt6-base-private-dev \
  qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools "$websockets_dev" \
  "$svg_dev" "${svg_plugins[@]}" libxkbcommon-dev libxkbcommon-x11-dev \
  libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
  libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev libxcb-sync-dev \
  libxcb-xfixes0-dev libxcb-xkb-dev libxcb-util-dev libx11-xcb-dev
apt-get install -y --no-install-recommends xvfb xauth weston libgl1-mesa-dri fonts-dejavu-core ffmpeg
exit 0
fi
git config --global --add safe.directory "$GITHUB_WORKSPACE"
export BUILD_DIR="$GITHUB_WORKSPACE/build/cloud"
export OUTPUT_DIR="$GITHUB_WORKSPACE/dist/cloud"
export QEMBY_QT_ROOT="${QT_ROOT_DIR:?Install the pinned Qt 6.9.3 SDK first}"
export QEMBY_DEB_DEPLOY_QT=ON
export QEMBY_DEB_SHLIBDEPS=OFF
export QEMBY_DEB_BUNDLE_MPV=OFF
export QEMBY_DEB_EXTRA_DEPENDS=""
export QEMBY_PLAYBACK_PROBE=ON
export CMAKE_BUILD_PARALLEL_LEVEL=2
python3 scripts/release/verify_source.py
bash packaging/linux/build-deb.sh
cmake --build "$BUILD_DIR" --config Release --target qEmbyPlaybackProbe
for deb in "$OUTPUT_DIR"/*.deb; do
  dpkg-deb --info "$deb"
  QEMBY_DEB_SAFETY_TEST=1 python3 scripts/release/check_deb_install.py \
    --deb "$deb" --report "$BUILD_DIR/deb-safety-report.json"
done
python3 scripts/release/check_deb_playback.py --build-dir "$BUILD_DIR"
python3 scripts/release/package.py --platform linux
