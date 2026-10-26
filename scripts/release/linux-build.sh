#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates git build-essential cmake ninja-build pkg-config python3 \
  dpkg-dev libmpv-dev libgl1-mesa-dev qt6-base-dev qt6-base-private-dev \
  qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools qt6-websockets-dev \
  qt6-svg-dev qt6-svg-plugins libxkbcommon-dev libxkbcommon-x11-dev \
  libxcb-cursor-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
  libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev libxcb-sync-dev \
  libxcb-xfixes0-dev libxcb-xkb-dev libxcb-util-dev libx11-xcb-dev
git config --global --add safe.directory "$GITHUB_WORKSPACE"
export BUILD_DIR="$GITHUB_WORKSPACE/build/cloud"
export OUTPUT_DIR="$GITHUB_WORKSPACE/dist/cloud"
export QEMBY_DEB_DEPLOY_QT=OFF
export QEMBY_DEB_SHLIBDEPS=ON
export CMAKE_BUILD_PARALLEL_LEVEL=2
bash packaging/linux/build-deb.sh
for deb in "$OUTPUT_DIR"/*.deb; do
  dpkg-deb --info "$deb"
  apt-get install -y "$deb"
done
python3 scripts/release/package.py --platform linux
