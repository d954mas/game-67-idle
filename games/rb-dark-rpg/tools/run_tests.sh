#!/usr/bin/env bash
# Build and run all game test executables with one summary pass/fail.
# A hung test (e.g. assert dialog) is killed after 60s and counted as FAIL.
GAME_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cmake --build "$GAME_DIR/build/native-debug" >/dev/null || {
  echo "BUILD FAILED"
  exit 1
}
fail=0
for t in "$GAME_DIR"/build/native-debug/bin/tests/test_*.exe; do
  name="$(basename "$t")"
  if timeout 60 "$t" >/dev/null 2>&1; then
    echo "PASS $name"
  else
    echo "FAIL $name"
    fail=1
  fi
done
if [ "$fail" = 0 ]; then echo "ALL TESTS PASS"; else echo "TESTS FAILED"; fi
exit $fail
