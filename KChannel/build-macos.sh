#!/usr/bin/env bash
# Build K Channel as a VST3 for macOS 10.13 High Sierra and later.
#
#   ./build-macos.sh                 # Release, build-mac
#   ./build-macos.sh Debug           # Debug
#   KCHANNEL_LD_CLASSIC=ON ./build-macos.sh
#
# Requires Xcode (or the Command Line Tools) and CMake on a Mac.
# Runs under macOS's stock bash 3.2, so keep this bash-3-compatible.

set -euo pipefail

cd "$(dirname "$0")"
ROOT="$(pwd)"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS; a macOS VST3 cannot be linked on Windows." >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake was not found. Install it with: brew install cmake" >&2
    exit 1
fi

CONFIG="${1:-Release}"
BUILD_DIR="${2:-build-mac}"
JUCE_DIR="${KCHANNEL_JUCE_DIR:-$ROOT/../JUCE}"

if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    echo "JUCE was not found at $JUCE_DIR" >&2
    echo "Fetch it with:" >&2
    echo "  git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git \"$JUCE_DIR\"" >&2
    exit 1
fi

CMAKE_ARGS=(
    -S "$ROOT" -B "$BUILD_DIR"
    -G "Unix Makefiles"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
    -DCMAKE_OSX_ARCHITECTURES=x86_64
    -DKCHANNEL_JUCE_DIR="$JUCE_DIR"
    -DKCHANNEL_FORMATS=VST3
)

if [[ "${KCHANNEL_LD_CLASSIC:-OFF}" == "ON" ]]; then
    CMAKE_ARGS+=( -DKCHANNEL_LD_CLASSIC=ON )
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --config "$CONFIG" --parallel "$(sysctl -n hw.ncpu)"

VST3="$BUILD_DIR/KChannel_artefacts/$CONFIG/VST3/K Channel.vst3"

if [[ ! -d "$VST3" ]]; then
    echo "The VST3 bundle was not produced at $VST3" >&2
    exit 1
fi

mkdir -p dist
ZIP="dist/KChannel-VST3-macOS10.13-x86_64.zip"
rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$VST3" "$ZIP"

echo
echo "Built:"
echo "  VST3: $VST3"
echo "  ZIP : $ZIP"
echo
echo "Install for the current user with:"
echo "  mkdir -p \"\$HOME/Library/Audio/Plug-Ins/VST3\""
echo "  cp -R \"$VST3\" \"\$HOME/Library/Audio/Plug-Ins/VST3/\""
echo "  xattr -dr com.apple.quarantine \"\$HOME/Library/Audio/Plug-Ins/VST3/K Channel.vst3\" 2>/dev/null || true"
echo "  codesign --force --deep --sign - \"\$HOME/Library/Audio/Plug-Ins/VST3/K Channel.vst3\" 2>/dev/null || true"
