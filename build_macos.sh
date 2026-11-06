#!/usr/bin/env bash
# Build MicSmith on macOS: Audio Unit + VST3 + Standalone, targeting macOS 10.13.
#
#   ./build_macos.sh            # Release
#   ./build_macos.sh Debug      # Debug
#
# Requires Xcode (or the Command Line Tools) and CMake.
#
# Note: this runs under macOS's stock bash 3.2, so it deliberately avoids
# constructs that only work on bash 4+.

set -euo pipefail

cd "$(dirname "$0")"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS." >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake was not found. Install it with:  brew install cmake" >&2
    exit 1
fi

if [[ ! -d JUCE ]]; then
    echo "The JUCE/ folder is missing. Either copy the whole project folder to" >&2
    echo "this Mac, or fetch JUCE next to this script:" >&2
    echo "" >&2
    echo "  git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git JUCE" >&2
    exit 1
fi

CONFIG="${1:-Release}"
BUILD_DIR="build-mac"

CMAKE_ARGS=(
    -S . -B "$BUILD_DIR"
    -G "Unix Makefiles"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
    -DCMAKE_OSX_ARCHITECTURES=x86_64
)

# MICSMITH_EXTRA_CMAKE_FLAGS lets you pass e.g.
#   MICSMITH_EXTRA_CMAKE_FLAGS="-DMICSMITH_LD_CLASSIC=ON" ./build_macos.sh
# which is the usual fix when the plug-in will not load on 10.13-10.15.
if [[ -n "${MICSMITH_EXTRA_CMAKE_FLAGS:-}" ]]; then
    # Deliberate word splitting: the variable holds a flag list.
    # shellcheck disable=SC2206
    EXTRA_FLAGS=( ${MICSMITH_EXTRA_CMAKE_FLAGS} )
    cmake "${CMAKE_ARGS[@]}" "${EXTRA_FLAGS[@]}"
else
    cmake "${CMAKE_ARGS[@]}"
fi

cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.ncpu)"

echo
echo "Built:"
echo "  VST3       : $BUILD_DIR/MicSmith_artefacts/$CONFIG/VST3/MicSmith.vst3"
echo "  Audio Unit : $BUILD_DIR/MicSmith_artefacts/$CONFIG/AU/MicSmith.component"
echo "  Standalone : $BUILD_DIR/MicSmith_artefacts/$CONFIG/Standalone/MicSmith.app"
echo
echo "Install for the current user with:  ./install_macos.sh"
