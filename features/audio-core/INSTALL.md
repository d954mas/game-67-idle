# Audio Core Install

This is an in-place module. A consumer at `templates/<id>` or `games/<id>`
references `${CMAKE_CURRENT_SOURCE_DIR}/../../features/audio-core` and records
the exact `feature.json.version` it validated.

## CMake wiring

Define `AUDIO_CORE_DIR`, `AUDIO_CORE_INC`, `AUDIO_CORE_SRC`,
`AUDIO_CORE_VENDOR`, and `AUDIO_CORE_WEB`. Compile these common sources:

```cmake
${AUDIO_CORE_SRC}/audio.c
${AUDIO_CORE_SRC}/audio_resource.c
```

On native Windows/Linux also compile:

```cmake
${AUDIO_CORE_SRC}/audio_backend_miniaudio.c
${AUDIO_CORE_SRC}/audio_miniaudio_impl.c
```

Linux additionally needs threads, `${CMAKE_DL_LIBS}`, and `m`. On Emscripten
compile `audio_backend_web.c` and link
`--js-library ${AUDIO_CORE_WEB}/audio_web.library.js`.

Put `${AUDIO_CORE_INC}` before the game `src` include directory so a stale
local copy cannot shadow the shared public header.

## Game-owned integration

1. Add audio files to the game's asset tree with license/provenance metadata.
2. Add each file to `build_packs.c` as `NT_ASSET_BLOB` under a codec-neutral ID.
3. Add source files to the pack target's `DEPENDS` list.
4. Regenerate the game's asset hashes before compiling code that uses them.
5. Keep cue/music enums and the hash-to-cue mapping in game code.
6. Call init/update/shutdown from the game shell, forward persisted mix values,
   platform pause/resume, and a real browser input gesture.

The template reference is `templates/template/src/game_audio.c`.

## Validation

With the template build configured, run:

```powershell
cmake --build templates/template/build/native-debug --target test_audio_core `
  test_audio_resource test_audio_backend_native test_game_audio
ctest --test-dir templates/template/build/native-debug `
  -R "test_audio_core|test_audio_resource|test_audio_backend_native|test_game_audio|test_audio_web_library" `
  --output-on-failure
node --test features/audio-core/tests/test_audio_web_library.mjs
```

For a compatibility claim, also build and test the consumer's real native and
Emscripten targets. These commands validate the current checkout; the metadata
contract does not certify unfinished T0393 work or untested platforms.

## Uninstall

Remove the module sources/include directory and web link option from CMake,
remove the game catalog lifecycle calls, and remove only the game audio blobs
that have no other consumer. No save migration is required because audio-core
owns no persisted state.
