#!/bin/bash
# tools/install_mac.sh - EIGHT-1 (2026-10-26): build the MS2000R and MS2000R 8-voice Audio Units from this checkout and
# install them for the current user (docs/eight_voice.md). On a fresh Mac, one command:
#
#   gh repo clone simon-mcevoy/ms2000r-emu ~/Code/ms2000r-emu -- -b macos-8-voice && ~/Code/ms2000r-emu/tools/install_mac.sh [ROM folder]
#
# The ROM folder (flash.bin, and boot-362.bin or boot-362.ms2000.bin beside it or in its "full FW" subfolder;
# hd44780_a00.bin optional) is copied to ~/Documents/MS2000R and the plugins are pointed at it. Without it the plugins
# keep finding their ROMs as before (the Settings choice, MS2K_HOME, ~/Documents/MS2000R).
# Needs the Xcode Command Line Tools and CMake (installed with Homebrew if missing). Safe to run again after a git pull:
# it rebuilds what changed and reinstalls. Keep the checkout out of iCloud-synced folders (Desktop, Documents).
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$REPO/build_vst.nosync"
COMPONENTS="$HOME/Library/Audio/Plug-Ins/Components"
ROMS="$HOME/Documents/MS2000R"
SETTING="$HOME/Library/MS2000R/home.txt"          # the plugin's "Choose the MS2000 folder" setting
[ -x /opt/homebrew/bin/brew ] && PATH="/opt/homebrew/bin:$PATH"

say() { printf '%s\n' "$*"; }
fail() { say ""; say "STOPPED: $*"; exit 1; }

# a folder the plugins can boot from - as ms2k::hasRoms (source/core/rom_paths.h)
has_roms() {
    [ -f "$1/flash.bin" ] || return 1
    local d
    for d in "$1/full FW" "$1/Full FW" "$1"; do
        if [ -f "$d/boot-362.ms2000.bin" ] || [ -f "$d/boot-362.bin" ]; then return 0; fi
    done
    return 1
}

say "MS2000R / MS2000R 8-voice: build and install from $REPO"
say ""
[ "$(uname -m)" = "arm64" ] || say "Note: this Mac has an Intel processor; the build is only tested on Apple Silicon."
if [ $# -gt 0 ]; then
    ROMSRC="$(cd "$1" 2>/dev/null && pwd)" || fail "no folder '$1'."
    has_roms "$ROMSRC" || fail "'$ROMSRC' is not an MS2000 ROM folder (flash.bin and boot-362.bin, beside it or in 'full FW')."
fi

# the tools
if ! xcode-select -p >/dev/null 2>&1; then
    xcode-select --install >/dev/null 2>&1 || true
    fail "the Xcode Command Line Tools are needed. Finish the installer that has opened, then run this again."
fi
if ! command -v cmake >/dev/null 2>&1; then
    command -v brew >/dev/null 2>&1 || fail "CMake is needed. Install Homebrew (https://brew.sh), then run this again."
    say "Installing CMake with Homebrew..."
    brew install cmake
fi

say "1. Building (the first build downloads JUCE and takes a few minutes)"
mkdir -p "$BUILD"
LOG="$BUILD/install_mac.log"
touch "$REPO/source/plugin/shim/ms2k_au_shim.cpp"   # the 4-voice bundle is reassembled only when its loader relinks
if ! { cmake -S "$REPO" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DMS2K_BUILD_VST3=ON &&
       cmake --build "$BUILD" --target MS2000R_AUShim MS2000R8_AU -j "$(sysctl -n hw.ncpu)"; } > "$LOG" 2>&1; then
    tail -25 "$LOG"
    fail "the build failed. The whole log: $LOG"
fi

say "2. The Audio Units -> $COMPONENTS"
mkdir -p "$COMPONENTS"
for c in MS2000R MS2000R8; do
    rm -rf "${COMPONENTS:?}/$c.component"
    ditto "$BUILD/AU_multi/$c.component" "$COMPONENTS/$c.component"
    xattr -dr com.apple.quarantine "$COMPONENTS/$c.component" 2>/dev/null || true
    codesign --verify --deep --strict "$COMPONENTS/$c.component" 2>/dev/null || fail "$c.component's signature does not check out."
done

say "3. The ROMs"
mkdir -p "$(dirname "$SETTING")"
if [ -n "${ROMSRC:-}" ]; then
    if [ "$ROMSRC" != "$ROMS" ]; then
        if [ -e "$ROMS/flash.bin" ] && ! cmp -s "$ROMSRC/flash.bin" "$ROMS/flash.bin"; then
            backup="$ROMS before $(date '+%Y-%m-%d %H.%M.%S')"
            mv "$ROMS" "$backup"
            say "   A different flash.bin was in $ROMS; that folder is kept as: $backup"
        fi
        mkdir -p "$ROMS"
        ditto "$ROMSRC" "$ROMS"
        xattr -dr com.apple.quarantine "$ROMS" 2>/dev/null || true
    fi
    printf '%s\n' "$ROMS" > "$SETTING"
    say "   copied to $ROMS"
elif [ -f "$SETTING" ] && has_roms "$(tr -d '\n' < "$SETTING")"; then
    say "   the plugins use $(tr -d '\n' < "$SETTING") (set before)"
elif has_roms "$ROMS"; then
    printf '%s\n' "$ROMS" > "$SETTING"
    say "   the plugins use $ROMS"
else
    ROMSMISSING=1
    say "   none found yet: run this again with the ROM folder, e.g. tools/install_mac.sh /Volumes/USB/MS2000_Full_Firmware"
fi

say "4. Telling macOS about the plugins and checking them with Apple's auval"
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 2
for code in Ms2r Ms28; do
    check="$(auval -v aumu "$code" Msem 2>&1 || true)"
    case "$check" in
        *"AU VALIDATION SUCCEEDED"*) say "   $code passed." ;;
        *) printf '%s\n' "$check" | tail -15 | sed 's/^/   /'
           fail "the plugins are installed but $code did not validate." ;;
    esac
done

say ""
say "Done. In your DAW, rescan the plugins (Logic: Settings > Plug-in Manager > Reset & Rescan Selection);"
say "they are MS2000R and MS2000R 8-voice by \"MS2000 emulation project\"."
[ -z "${ROMSMISSING:-}" ] || say "They make no sound until they have the ROMs (step 3)."
