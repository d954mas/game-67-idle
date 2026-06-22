#!/bin/sh
# Smallest reliable build+run for the Little Lives overnight run.
# Boot smoke: build green, then DevAPI smoke (python tmp/ll_smoke.py) round-trips.
set -e
cmake --preset native-debug
cmake --build --preset native-debug --target game_seed
# Run headless with DevAPI for scripted smoke + screenshots:
#   build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720 &
#   python tmp/ll_smoke.py
echo "build OK: build/game_seed/native-debug/game_seed.exe"
