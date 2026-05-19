#!/usr/bin/env bash
# Install the seastar build toolchain and run ./configure.py inside the
# CI container. Called from .github/workflows/test.yaml after checkout.
#
# Expects these environment variables:
#   COMPILER       clang++-N | g++-N  (e.g. clang++-22, g++-16)
#   ENABLES        string of --enable-* flags passed to configure.py
#   STANDARD       C++ standard (e.g. 20, 23)
#   MODE           build mode (dev, debug, release, fuzz)
#   OPTIONS        extra configure.py args (may contain spaces)
#   ENABLE_CCACHE  true | false (controls --compiler-cache=ccache)
#
# Locally:
#   COMPILER=clang++-22 ENABLES='' STANDARD=23 MODE=dev OPTIONS='' \
#     ENABLE_CCACHE=false ./install-build-env.sh
#
# ::group:: / ::endgroup:: are GitHub Actions log-folding markers so
# each phase shows up collapsed in the CI log, mirroring what separate
# steps used to look like. Harmless plain text when run locally.
set -euo pipefail

# Suppress debconf frontend warnings when apt installs packages with
# postinst prompts (the container has no tty / no readline / no dialog).
export DEBIAN_FRONTEND=noninteractive

group() {
    echo "::endgroup::"
    echo "::group::$*"
}
trap 'echo "::endgroup::"' EXIT
echo "::group::install-dependencies.sh"

./install-dependencies.sh

packages=()
case "$COMPILER" in
    clang++-*)
        version="${COMPILER#clang++-}"
        # libstdc++-16-dev: libboost-all-dev pulls gcc-16-base but not
        # libstdc++-16-dev; clang prefers the highest gcc dir, so
        # without this `ld: cannot find -lstdc++` aborts the link.
        packages+=("clang-${version}" libstdc++-16-dev)
        CC="clang-${version}"
        CPP="$COMPILER"
        ;;
    g++-*)
        version="${COMPILER#g++-}"
        packages+=("gcc-${version}" "g++-${version}")
        CC="gcc-${version}"
        CPP="$COMPILER"
        ;;
    *) echo "install-build-env.sh: unknown COMPILER='$COMPILER'" >&2; exit 1 ;;
esac

if [[ "$ENABLES" == *cxx-modules* ]]; then
    if [[ "$COMPILER" != clang++-* ]]; then
        echo "install-build-env.sh: cxx-modules requires clang++, got '$COMPILER'" >&2
        exit 1
    fi
    packages+=("clang-tools-${version}")
fi

group "apt-get install ${packages[*]}"
apt-get install -y "${packages[@]}"

ccache_opt=()
if [[ "$ENABLE_CCACHE" != "false" ]]; then
    ccache_opt=(--compiler-cache=ccache)
fi

# --cook fmt for clang >= 20: the system libfmt-dev on ubuntu:26.04 is
# 10.1.1, and clang-20+ enforces consteval strictly enough to reject
# fmt/chrono.h's FMT_STRING("{:.{}f}") path. clang-19 and the gcc
# matrix items work fine with the system fmt.
#
# For C++26, cook fmt-12-1-dev instead: see seastar issue #3411 —
# libstdc++-16 makes std::optional model std::ranges::range, which
# tripped two overlapping formatter specializations in fmt 11.x/12.x.
cook_args=()
if [ "$STANDARD" = "26" ]; then
    cook_args=(--cook fmt-12-1-dev)
elif [[ "$COMPILER" == clang++-* ]]; then
    clang_ver="${COMPILER#clang++-}"
    if [ "$clang_ver" -ge 20 ]; then
        cook_args=(--cook fmt)
    fi
fi

group "configure.py"
# OPTIONS / ENABLES intentionally unquoted: each carries multiple
# whitespace-separated args (e.g. "--cook dpdk --dpdk-machine corei7-avx").
# shellcheck disable=SC2086
./configure.py                  \
    --c++-standard "$STANDARD"  \
    --compiler "$CPP"           \
    --c-compiler "$CC"          \
    --mode "$MODE"              \
    "${cook_args[@]}"           \
    "${ccache_opt[@]}"          \
    $OPTIONS                    \
    $ENABLES
