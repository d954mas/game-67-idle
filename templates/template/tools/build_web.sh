#!/usr/bin/env bash
# Build one wasm preset and place the asset pack next to index.html, so
# `node tools/serve_web.mjs --preset <preset>` immediately serves a working game.
# One command from the done-when; mirror of rb-dark's package_web.sh minus the
# itch zip (that stays in T0323's package_web.sh, which calls this script).
#
#   bash tools/build_web.sh [--preset wasm-release|wasm-debug|wasm-devapi-debug]
#
# Presets: wasm-release (default, human), wasm-debug (human, carries ASan),
#          wasm-devapi-debug (agent; engine DevAPI over the web transport).
set -e

PRESET="wasm-release"
while [ $# -gt 0 ]; do
    case "$1" in
        --preset) PRESET="$2"; shift 2 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

case "$PRESET" in
    wasm-release)      CFG_FLAGS="-DCMAKE_BUILD_TYPE=Release" ;;
    wasm-debug)        CFG_FLAGS="-DCMAKE_BUILD_TYPE=Debug" ;;
    wasm-devapi-debug) CFG_FLAGS="-DCMAKE_BUILD_TYPE=Debug -DGAME_DEVAPI_ENABLED=ON" ;;
    *) echo "unknown preset: $PRESET (use wasm-release|wasm-debug|wasm-devapi-debug)" >&2; exit 2 ;;
esac

GAME_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# EMSDK: resolve the root ($EMSDK first, then the rb-dark hardcoded fallback --
# a portability wart, kept as fallback). Source emsdk_env.sh to put emcc/ninja on
# PATH for the build step, and derive the toolchain file for configure.
EMSDK_ROOT="${EMSDK:-}"
if [ -z "$EMSDK_ROOT" ] && [ -d /c/develop/emsdk ]; then
    EMSDK_ROOT="/c/develop/emsdk"
fi
if [ -n "$EMSDK_ROOT" ] && [ -f "$EMSDK_ROOT/emsdk_env.sh" ]; then
    source "$EMSDK_ROOT/emsdk_env.sh" >/dev/null 2>&1 || true
fi
EM_TOOLCHAIN=""
if [ -n "$EMSDK_ROOT" ]; then
    EM_TOOLCHAIN="$EMSDK_ROOT/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
fi

# 1) Fresh native pack: the pack builder is native-only, so wasm copies it from
# the native build. Configure native-debug once, then always rebuild the pack
# incrementally (fast; picks up any asset change).
NATIVE_DIR="$GAME_DIR/build/native-debug"
if [ ! -f "$NATIVE_DIR/CMakeCache.txt" ]; then
    cmake -S "$GAME_DIR" -B "$NATIVE_DIR" -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
fi
cmake --build "$NATIVE_DIR" --target game_asset_packs

# 2) Web build for the preset. Configure via the Emscripten toolchain file
# directly: `emcmake` is not a bash command on Windows (only emcmake.bat/.py
# exist, no extensionless wrapper), so relying on it breaks a fresh game. The
# toolchain file finds emcc by its own location, so emcc need not be on PATH.
WEB_DIR="$GAME_DIR/build/$PRESET"
if [ ! -f "$WEB_DIR/CMakeCache.txt" ]; then
    if [ -f "$EM_TOOLCHAIN" ]; then
        cmake -S "$GAME_DIR" -B "$WEB_DIR" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$EM_TOOLCHAIN" $CFG_FLAGS
    else
        emcmake cmake -S "$GAME_DIR" -B "$WEB_DIR" -G Ninja $CFG_FLAGS  # fallback: emcmake on PATH (e.g. Linux CI)
    fi
fi
cmake --build "$WEB_DIR" --target game

# 3) Copy the pack flat next to index.html (engine streams it over HTTP,
# relative to the page URL).
mkdir -p "$WEB_DIR/bin/assets"
cp "$NATIVE_DIR/bin/assets/game.ntpack" "$WEB_DIR/bin/assets/game.ntpack"

echo "built $PRESET; serve with: node tools/serve_web.mjs --preset $PRESET"
