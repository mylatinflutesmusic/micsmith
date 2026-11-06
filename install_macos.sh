#!/usr/bin/env bash
# Install the macOS build into the current user's plug-in folders.
#
#   ./install_macos.sh              # uses build-mac, Release
#   ./install_macos.sh build-mac Debug

set -euo pipefail

cd "$(dirname "$0")"

BUILD_DIR="${1:-build-mac}"
CONFIG="${2:-Release}"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

VST3_SRC="$BUILD_DIR/MicSmith_artefacts/$CONFIG/VST3/MicSmith.vst3"
AU_SRC="$BUILD_DIR/MicSmith_artefacts/$CONFIG/AU/MicSmith.component"

if [[ ! -d "$VST3_SRC" ]]; then
    echo "Not built yet: $VST3_SRC is missing. Run ./build_macos.sh first." >&2
    exit 1
fi

mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/MicSmith.vst3" "$AU_DIR/MicSmith.component"

cp -R "$VST3_SRC" "$VST3_DIR/"
echo "Installed $VST3_DIR/MicSmith.vst3"

if [[ -d "$AU_SRC" ]]; then
    cp -R "$AU_SRC" "$AU_DIR/"
    echo "Installed $AU_DIR/MicSmith.component"
fi

# Downloaded/copied bundles can carry a quarantine flag; ad-hoc signing stops
# Gatekeeper from refusing to load an unsigned plug-in.
for target in "$VST3_DIR/MicSmith.vst3" "$AU_DIR/MicSmith.component"; do
    [[ -e "$target" ]] || continue
    xattr -dr com.apple.quarantine "$target" 2>/dev/null || true
    codesign --force --deep --sign - "$target" 2>/dev/null || true
done

echo
echo "Done. Restart your DAW and rescan plug-ins."
echo "Validate the Audio Unit with:  auval -v aufx Msm1 Msmi"
