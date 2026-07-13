#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
audio="$repo_root/features/audio-core"
engine="$repo_root/external/neotolis-engine"
build="$(mktemp -d "${TMPDIR:-/tmp}/audio-core-linux.XXXXXX")"
trap 'rm -rf -- "$build"' EXIT

cc="${CC:-clang}"
cflags=(
  -std=c17 -O2 -DNDEBUG -DNT_ASSERT_MODE=1 -DNT_INTROSPECT_ENABLED=0
  -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion
  -Wformat=2 -Wundef -Wno-unused-parameter -Werror
  "-I$audio/include" "-I$audio/src" "-I$audio/tests"
  "-I$audio/vendor/miniaudio"
  "-I$engine/engine" "-I$engine/shared/include"
  "-I$engine/deps/unity/src"
)

"$cc" "${cflags[@]}" \
  "$audio/tests/test_audio.c" \
  "$audio/tests/fake_audio_environment.c" \
  "$audio/src/audio.c" \
  "$engine/engine/core/nt_assert.c" \
  "$engine/deps/unity/src/unity.c" \
  -lm -o "$build/test_audio_core"
"$build/test_audio_core"

"$cc" "${cflags[@]}" \
  "$audio/tests/test_audio_resource.c" \
  "$audio/src/audio_resource.c" \
  "$engine/deps/unity/src/unity.c" \
  -o "$build/test_audio_resource"
"$build/test_audio_resource"

"$cc" "${cflags[@]}" -DAUDIO_MINIAUDIO_TEST_NO_DEVICE=1 \
  -DAUDIO_TEST_MP3_PATH=\"$repo_root/templates/template/assets/audio/music/demo_jingle.mp3\" \
  "$audio/tests/test_audio_backend_native.c" \
  "$audio/src/audio_backend_miniaudio.c" \
  "$audio/src/audio_miniaudio_impl.c" \
  "$engine/deps/unity/src/unity.c" \
  -pthread -ldl -lm -o "$build/test_audio_backend_native"
"$build/test_audio_backend_native"

# Compile the actual Linux backend selection too; the deterministic test above
# deliberately uses Miniaudio's no-device seam.
"$cc" "${cflags[@]}" -c "$audio/src/audio_backend_miniaudio.c" -o "$build/audio_backend_linux.o"
"$cc" "${cflags[@]}" -c "$audio/src/audio_miniaudio_impl.c" -o "$build/audio_miniaudio_linux.o"

echo "audio-core Linux compile/tests: PASS"
