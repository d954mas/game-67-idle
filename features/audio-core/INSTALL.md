# Audio Core Install

This is an in-place module. Resolve `GAME_REPO_ROOT` once, then reference
`${GAME_REPO_ROOT}/features/audio-core`; this is valid for templates, public
games, and `games/private/<id>`. Record the exact `feature.json.version` validated.

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
`--js-library ${AUDIO_CORE_WEB}/audio_web.library.js`. The web backend compiles
miniaudio's decoder into its own translation unit for streamed clips, so it
needs no other source.

Put `${AUDIO_CORE_INC}` before the game `src` include directory so a stale
local copy cannot shadow the shared public header.

## Game-owned integration

1. Add audio files to the game's asset tree with license/provenance metadata.
   Encode MP3 with its Xing/LAME header (`ffmpeg -write_xing 1`): both backends
   read the encoder delay and padding from that header and trim them. A file
   encoded without one carries about 25 ms of encoder silence that nothing can
   remove: the cue fires late and a loop gaps. ffmpeg writes the header by
   seeking back, so encode to a file: an MP3 written to a pipe has none.
   Open music with `audio_clip_stream` and short cues with `audio_clip_load`.
2. Add short cues to `game.ntpack` and music to a separate `music.ntpack`,
   as `NT_ASSET_BLOB` under codec-neutral IDs. `audio_clip_stream` changes
   decoding, not network delivery: music inside the core pack still delays
   startup. The blank template automatically packs authored tracks under
   `assets/audio/music/`; its README documents generated IDs and opt-in delivery.
3. Add source files to the pack target's `DEPENDS` list.
4. Regenerate the game's asset hashes before compiling code that uses them.
5. Keep cue/music enums and the hash-to-cue mapping in game code.
6. Call init/update/shutdown from the game shell, forward persisted mix values,
   platform pause/resume, and a real browser input gesture.
7. Permit the music request only after the first complete playable frame was
   presented (`swap_buffers` completed). Do not request its BLOB at audio init
   or include it in startup readiness. Load it in the background when music is
   wanted; request the track BLOB and stream it once the music pack is ready.
   A slow or failed request must leave gameplay and core cues working. Keep
   engine HTTP retries with backoff; after a terminal error, provide an
   explicit retry, such as switching music off and on. Stop audio before
   unmounting the music pack, including a pending request during shutdown.

The feature-test consumer reference is `ai_studio/feature_testing/consumer/src/game_audio.c`.

## Validation

With the template build configured, run:

```powershell
cmake --build ai_studio/feature_testing/consumer/build/feature-verify --target test_audio_core `
  test_audio_resource test_audio_backend_native test_game_audio
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify `
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
