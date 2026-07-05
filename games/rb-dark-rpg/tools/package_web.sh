#!/usr/bin/env bash
# Build the shipping web artifact and zip it for itch.io (index.html at zip root).
# Requires the native pack to be fresh (wasm preloads it) - run dev_rebuild.sh first
# after any asset/content change, then this script.
set -e
GAME_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$GAME_DIR/build/wasm-release/bin"
DIST="$GAME_DIR/build/dist"
source /c/develop/emsdk/emsdk_env.sh >/dev/null 2>&1 || true
cmake --build "$GAME_DIR/build/wasm-release" --target game
# Web streams the pack over HTTP (relative path), so it ships as a plain file
# next to index.html, taken from the freshest native build.
mkdir -p "$BIN/assets"
cp "$GAME_DIR/build/native-debug/bin/assets/game.ntpack" "$BIN/assets/game.ntpack"
mkdir -p "$DIST"
rm -f "$DIST/rb-dark-rpg-web.zip"
# NB: zipfile CLI flattens single-file args to their basename; pass the assets
# DIRECTORY so the archive keeps the assets/game.ntpack relative path.
(cd "$BIN" && python -m zipfile -c "$DIST/rb-dark-rpg-web.zip" index.html game.js game.wasm game.data assets)
ls -la "$DIST/rb-dark-rpg-web.zip"
echo "upload $DIST/rb-dark-rpg-web.zip to itch.io (HTML game, 'This file will be played in the browser')"
