#!/usr/bin/env bash
# Build and package Quell as an x86_64 VST3 for macOS 10.13 High Sierra and later.
#
#   ./build-macos.sh [Release|Debug]
#
# Output:
#   dist/Quell-VST3-macOS10.13-x86_64.zip
#
# Requires Xcode (or the Command Line Tools), CMake, and the vendored JUCE
# checkout at ../JUCE. The GitHub Actions workflow supplies those prerequisites.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS." >&2
    exit 1
fi

JUCE_DIR="$SCRIPT_DIR/../JUCE"
if [[ ! -f "$JUCE_DIR/CMakeLists.txt" ]]; then
    echo "JUCE was not found at $JUCE_DIR." >&2
    echo "Fetch JUCE 8.0.6 next to Quell:" >&2
    echo "  git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git \"$JUCE_DIR\"" >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake was not found. Install it with: brew install cmake" >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode or the Command Line Tools were not found." >&2
    echo "Install them with: xcode-select --install" >&2
    exit 1
fi

CONFIG="${1:-Release}"
BUILD_DIR="build-mac"
VST3_BUNDLE="$BUILD_DIR/Quell_artefacts/$CONFIG/VST3/Quell.vst3"
VST3_BINARY="$VST3_BUNDLE/Contents/MacOS/Quell"
DIST_DIR="dist"
ZIP="$DIST_DIR/Quell-VST3-macOS10.13-x86_64.zip"

CMAKE_ARGS=(
    -S . -B "$BUILD_DIR"
    -G "Unix Makefiles"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
    -DCMAKE_OSX_ARCHITECTURES=x86_64
)

# Newer SDKs can emit libc++ calls that 10.13-10.15 do not export. The classic
# linker avoids them, but it only exists in newer Xcode versions. Probe first
# so the script still works with an older Xcode that cannot target those SDKs.
PROBE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/quell-ld-classic.XXXXXX")"
trap 'rm -rf "$PROBE_DIR"' EXIT

if printf 'int main() { return 0; }\n' | \
    xcrun clang++ -x c++ - -Wl,-ld_classic -o "$PROBE_DIR/probe" >/dev/null 2>&1; then
    echo "Using the classic linker for older macOS compatibility."
    CMAKE_ARGS+=(
        -DCMAKE_EXE_LINKER_FLAGS=-Wl,-ld_classic
        -DCMAKE_SHARED_LINKER_FLAGS=-Wl,-ld_classic
        -DCMAKE_MODULE_LINKER_FLAGS=-Wl,-ld_classic
    )
else
    echo "Classic linker unavailable; using the default linker."
    CMAKE_ARGS+=(
        -DCMAKE_EXE_LINKER_FLAGS=
        -DCMAKE_SHARED_LINKER_FLAGS=
        -DCMAKE_MODULE_LINKER_FLAGS=
    )
fi

rm -rf "$PROBE_DIR"
trap - EXIT

echo
echo "== Configure =="
cmake "${CMAKE_ARGS[@]}"

echo
echo "== Build VST3 =="
cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.ncpu)" --target Quell_VST3

if [[ ! -f "$VST3_BINARY" ]]; then
    echo "VST3 binary was not produced at $VST3_BINARY" >&2
    exit 1
fi

echo
echo "== Verify High Sierra x86_64 target =="
file "$VST3_BINARY"
lipo -info "$VST3_BINARY"
if ! lipo -info "$VST3_BINARY" | grep -q "x86_64"; then
    echo "The VST3 is not an x86_64 binary." >&2
    exit 1
fi

BUILD_INFO="$(vtool -show-build "$VST3_BINARY" 2>/dev/null || otool -l "$VST3_BINARY")"
printf '%s\n' "$BUILD_INFO"
if ! printf '%s\n' "$BUILD_INFO" | grep -Eq 'minos 10\.13|version 10\.13'; then
    echo "The VST3 does not declare macOS 10.13 as its minimum version." >&2
    exit 1
fi

echo
echo "== Package =="
mkdir -p "$DIST_DIR"
rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$VST3_BUNDLE" "$ZIP"
shasum -a 256 "$ZIP"
ls -lh "$ZIP"

echo
echo "Created:"
echo "  $SCRIPT_DIR/$ZIP"
