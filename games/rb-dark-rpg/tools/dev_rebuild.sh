#!/usr/bin/env bash
# Rebuild-safe iteration loop: kill a running game.exe (it holds the exe lock
# and breaks lld-link), rebuild the game target, optionally relaunch.
# Usage: tools/dev_rebuild.sh [-r]   (-r = relaunch after build)
set -e
GAME_DIR="$(cd "$(dirname "$0")/.." && pwd)"
taskkill //IM game.exe //F >/dev/null 2>&1 || true
cmake --build "$GAME_DIR/build/native-debug" --target game
if [ "$1" = "-r" ]; then
  "$GAME_DIR/build/native-debug/bin/game.exe" &
  echo "relaunched game.exe (pid $!)"
fi
