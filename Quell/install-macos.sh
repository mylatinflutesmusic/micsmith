#!/usr/bin/env bash
# Install a downloaded Quell macOS VST3 zip for the current user or all users.
#
#   ./install-macos.sh [--system] [path/to/Quell-VST3-macOS10.13-x86_64.zip]
#
# With no path, looks in ./dist and then ~/Downloads. Handles both the
# packaged VST3 zip and GitHub Actions' artifact wrapper zip. --system installs
# to /Library/Audio/Plug-Ins/VST3 and uses sudo for the install steps.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ZIP_NAME="Quell-VST3-macOS10.13-x86_64.zip"
SYSTEM_WIDE=0
ZIP_ARG=""

usage()
{
    echo "usage: $0 [--system] [path/to/$ZIP_NAME]" >&2
    echo "  --system  install for all users under /Library/Audio/Plug-Ins/VST3" >&2
}

run_privileged()
{
    if [[ "$SYSTEM_WIDE" -eq 0 ]]; then
        "$@"
    elif [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    else
        sudo "$@"
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --system)
            SYSTEM_WIDE=1
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage
            exit 2
            ;;
        *)
            if [[ -n "$ZIP_ARG" ]]; then
                echo "Only one zip path can be installed at a time." >&2
                usage
                exit 2
            fi
            ZIP_ARG="$1"
            ;;
    esac
    shift
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS." >&2
    exit 1
fi

if [[ "$(id -u)" -eq 0 && -n "${SUDO_USER:-}" && "$SYSTEM_WIDE" -eq 0 ]]; then
    echo "Run this helper as your normal user without sudo." >&2
    echo "Use --system for an install shared by all users." >&2
    exit 1
fi

if [[ "$SYSTEM_WIDE" -eq 1 ]]; then
    TARGET_DIR="/Library/Audio/Plug-Ins/VST3"

    if [[ "$(id -u)" -ne 0 ]] && ! sudo -v; then
        echo "System-wide install requires administrator access." >&2
        exit 1
    fi
else
    TARGET_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
fi

TARGET="$TARGET_DIR/Quell.vst3"

if [[ -n "$ZIP_ARG" ]]; then
    ZIP="$ZIP_ARG"
else
    ZIP=""

    for candidate in "$SCRIPT_DIR/dist/$ZIP_NAME" "$HOME/Downloads/$ZIP_NAME"; do
        if [[ -f "$candidate" ]]; then
            ZIP="$candidate"
            break
        fi
    done
fi

if [[ -z "$ZIP" ]]; then
    echo "Could not find $ZIP_NAME." >&2
    usage
    exit 1
fi

case "$ZIP" in
    /*) ;;
    *) ZIP="$(pwd)/$ZIP" ;;
esac

if [[ ! -f "$ZIP" ]]; then
    echo "Zip not found: $ZIP" >&2
    exit 1
fi

TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/quell-install.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

echo "Extracting $(basename "$ZIP")..."
ditto -x -k "$ZIP" "$TMP_DIR"

VST3_SRC="$TMP_DIR/Quell.vst3"

# A GitHub Actions artifact download is a zip containing the uploaded zip.
# Extract one nested zip when the bundle is not at the top level.
if [[ ! -d "$VST3_SRC" ]]; then
    for nested in "$TMP_DIR"/*.zip; do
        [[ -f "$nested" ]] || continue

        echo "Extracting nested artifact $(basename "$nested")..."
        mkdir -p "$TMP_DIR/nested"
        ditto -x -k "$nested" "$TMP_DIR/nested"

        if [[ -d "$TMP_DIR/nested/Quell.vst3" ]]; then
            VST3_SRC="$TMP_DIR/nested/Quell.vst3"
            break
        fi
    done
fi

if [[ ! -d "$VST3_SRC" ]]; then
    echo "No Quell.vst3 bundle found in $ZIP." >&2
    exit 1
fi

if [[ ! -x "$VST3_SRC/Contents/MacOS/Quell" ]]; then
    echo "The bundle does not contain Contents/MacOS/Quell." >&2
    exit 1
fi

if ! run_privileged mkdir -p "$TARGET_DIR"; then
    echo "Could not create $TARGET_DIR." >&2
    exit 1
fi

if [[ -e "$TARGET" || -L "$TARGET" ]]; then
    echo "Replacing existing $TARGET"

    if ! run_privileged rm -rf "$TARGET"; then
        echo "Could not remove the existing Quell.vst3." >&2
        echo "Close any DAW using it and retry." >&2
        exit 1
    fi
fi

if ! run_privileged ditto "$VST3_SRC" "$TARGET"; then
    echo "Could not copy Quell.vst3 to $TARGET." >&2
    exit 1
fi

# Downloaded archives carry com.apple.quarantine. Remove it recursively so
# Gatekeeper does not refuse the plug-in, then ad-hoc sign the extracted bundle
# so it remains valid without a Developer ID certificate.
if ! run_privileged xattr -dr com.apple.quarantine "$TARGET" 2>/dev/null; then
    echo "No quarantine attribute was removed; it may already be absent."
fi

if ! run_privileged codesign --force --deep --sign - "$TARGET"; then
    echo "Ad-hoc signing failed." >&2
    exit 1
fi

if ! run_privileged codesign --verify --deep --strict --verbose=2 "$TARGET"; then
    echo "Code signature verification failed." >&2
    exit 1
fi

echo
if [[ "$SYSTEM_WIDE" -eq 1 ]]; then
    echo "Installed for all users:"
else
    echo "Installed for the current user:"
fi
echo "  $TARGET"
echo
echo "Quarantine removed and bundle ad-hoc signed."
echo "Restart your DAW and rescan VST3 plug-ins."
