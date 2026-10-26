#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-release-deb}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/dist/deb}"
GENERATOR="${CMAKE_GENERATOR:-Ninja}"
DEB_SHLIBDEPS="${QEMBY_DEB_SHLIBDEPS:-}"
DEB_DEPLOY_QT="${QEMBY_DEB_DEPLOY_QT:-OFF}"
DEB_BUNDLE_MPV="${QEMBY_DEB_BUNDLE_MPV:-OFF}"
DEB_MANUAL_DEPENDS="${QEMBY_DEB_MANUAL_DEPENDS:-}"
DEB_EXTRA_DEPENDS="${QEMBY_DEB_EXTRA_DEPENDS:-libqt6openglwidgets6, libqt6websockets6, libqt6svg6, qt6-svg-plugins}"
DEB_DIST_TAG="${QEMBY_DEB_DIST_TAG:-}"

reset_stale_cmake_cache_if_needed() {
    local build_dir="$1"
    local cache_file="$build_dir/CMakeCache.txt"

    if [[ ! -f "$cache_file" ]]; then
        return 0
    fi

    local cached_source_dir
    local cached_build_dir
    cached_source_dir="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file" | head -n 1)"
    cached_build_dir="$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$cache_file" | head -n 1)"

    if [[ -n "$cached_source_dir" && "$cached_source_dir" != "$ROOT_DIR" ]] ||
       [[ -n "$cached_build_dir" && "$cached_build_dir" != "$build_dir" ]]; then
        echo "Detected stale CMake cache in $build_dir" >&2
        echo "Cached source: ${cached_source_dir:-<unknown>}" >&2
        echo "Cached build dir: ${cached_build_dir:-<unknown>}" >&2
        echo "Recreating the build directory for the current machine..." >&2
        rm -rf "$build_dir"
    fi
}

default_manual_depends_for_dist() {
    local dist_tag="$1"
    local mpv_dep="libmpv2"

    case "$dist_tag" in
        jammy)
            mpv_dep="libmpv1 (>= 0.29)"
            ;;
        noble|oracular|bookworm|trixie)
            mpv_dep="libmpv2"
            ;;
    esac

    printf '%s' \
        "libc6 (>= 2.34), libdbus-1-3 (>= 1.9.14), libdrm2 (>= 2.4.62), libegl1, libfontconfig1 (>= 2.12.6), libfreetype6 (>= 2.9.1), libgbm1 (>= 17.1.0~rc2), libgcc-s1 (>= 3.4), libgl1, libglib2.0-0 (>= 2.45.6), libglx0, libgssapi-krb5-2 (>= 1.17), ${mpv_dep}, libopengl0, libstdc++6 (>= 12), libx11-6, libx11-xcb1 (>= 2:1.7.5), libxcb-cursor0 (>= 0.0.99), libxcb-glx0, libxcb-icccm4 (>= 0.4.1), libxcb-image0 (>= 0.2.1), libxcb-keysyms1 (>= 0.4.0), libxcb-randr0 (>= 1.12), libxcb-render-util0, libxcb-render0, libxcb-shape0, libxcb-shm0 (>= 1.10), libxcb-sync1, libxcb-util1 (>= 0.4.0), libxcb-xfixes0, libxcb-xkb1, libxcb1 (>= 1.8), libxkbcommon-x11-0 (>= 0.5.0), libxkbcommon0 (>= 0.5.0), libzstd1 (>= 1.4.0), zlib1g (>= 1:1.2.3.4)"
}

if [[ -z "$DEB_DIST_TAG" && -f /etc/os-release ]]; then
    # Native DEB is distro-specific. Prefer distro codenames like jammy/noble/trixie
    # so the output naming matches the style commonly used by Linux application releases.
    . /etc/os-release

    if [[ -n "${VERSION_CODENAME:-}" ]]; then
        DEB_DIST_TAG="${VERSION_CODENAME}"
    elif [[ -n "${UBUNTU_CODENAME:-}" ]]; then
        DEB_DIST_TAG="${UBUNTU_CODENAME}"
    elif [[ -n "${DEBIAN_CODENAME:-}" ]]; then
        DEB_DIST_TAG="${DEBIAN_CODENAME}"
    elif [[ -n "${ID:-}" && -n "${VERSION_ID:-}" ]]; then
        DEB_DIST_TAG="${ID}${VERSION_ID}"
        DEB_DIST_TAG="${DEB_DIST_TAG//[^A-Za-z0-9.+~:-]/}"
    fi
fi

reset_stale_cmake_cache_if_needed "$BUILD_DIR"

if [[ -z "$DEB_SHLIBDEPS" ]]; then
    if [[ "$DEB_DEPLOY_QT" == "ON" ]]; then
        DEB_SHLIBDEPS="OFF"
    else
        DEB_SHLIBDEPS="ON"
    fi
fi

if [[ "$DEB_SHLIBDEPS" == "OFF" && -z "$DEB_MANUAL_DEPENDS" ]]; then
    DEB_MANUAL_DEPENDS="$(default_manual_depends_for_dist "$DEB_DIST_TAG")"
fi

mkdir -p "$OUTPUT_DIR"

cmake_args=(
    -S "$ROOT_DIR"
    -B "$BUILD_DIR"
    -G "$GENERATOR"
    -DCMAKE_BUILD_TYPE=Release
    -DQEMBY_DEBIAN_PACKAGE_SHLIBDEPS="$DEB_SHLIBDEPS"
    -DQEMBY_DEBIAN_PACKAGE_EXTRA_DEPENDS="$DEB_EXTRA_DEPENDS"
    -DQEMBY_ENABLE_QT_DEPLOY="$DEB_DEPLOY_QT"
    -DQEMBY_BUNDLE_MPV_RUNTIME="$DEB_BUNDLE_MPV"
)

if [[ -n "$DEB_MANUAL_DEPENDS" ]]; then
    cmake_args+=("-DQEMBY_DEBIAN_PACKAGE_DEPENDS=$DEB_MANUAL_DEPENDS")
fi

if [[ -n "$DEB_DIST_TAG" ]]; then
    cmake_args+=("-DQEMBY_DEBIAN_PACKAGE_RELEASE=$DEB_DIST_TAG")
fi

if [[ -n "${QEMBY_QT_ROOT:-}" ]]; then
    cmake_args+=("-DQEMBY_QT_ROOT=$QEMBY_QT_ROOT")
fi

cmake "${cmake_args[@]}"

build_args=(--build "$BUILD_DIR" --config Release)
if [[ "${QEMBY_BUILD_KEEP_GOING:-OFF}" == "ON" && "$GENERATOR" == "Ninja" ]]; then
    build_args+=(-- -k 0)
fi
cmake "${build_args[@]}"
rm -rf "$OUTPUT_DIR/_CPack_Packages"
cpack --config "$BUILD_DIR/CPackConfig.cmake" -G DEB -B "$OUTPUT_DIR"

echo "DEB package output: $OUTPUT_DIR"

#QEMBY_QT_ROOT="/home/hj/Qt/6.9.3/gcc_64" ./packaging/linux/build-deb.sh
#QEMBY_QT_ROOT="/home/hj/Qt/6.9.3/gcc_64" QEMBY_DEB_DEPLOY_QT=ON ./packaging/linux/build-deb.sh
#QEMBY_QT_ROOT="/home/hj/Qt/6.9.3/gcc_64" ./packaging/linux/build-appimage.sh
