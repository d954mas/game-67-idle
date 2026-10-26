# Blank Game Foundation

This template starts a runnable game with an intentionally empty screen. It
provides a native and web build, the state/save foundation, optional DevAPI,
and an explicit game lifecycle without entities, UI, content, audio, economy,
or portal behavior.

## Start here

For local iteration, run `node tools/build_web.mjs --preset wasm-debug --preview`.
Use `node tools/game.mjs test --only <ctest-name>` for a changed native contract;
this focused mode skips unrelated tooling suites. Ordinary `test` retains them.

Before accepting changes to the starter itself, run
`node tools/game.mjs test --template-proof`. Preview is explicitly non-proof;
use an ordinary build for capture and release checks.

`src/main.c` sequences the shell: startup, one frame, and shutdown. The frame is
four phases in a fixed order, `platform begin -> tick -> present -> platform end`,
each in its own `src/shell/shell_<phase>.c`; `src/shell/shell.h` states the
contract. Game logic belongs in feature modules and is sequenced by
`src/features/game_features.c`.
Presentation belongs in renderer or UI modules; it receives state and input but
does not own game rules. The supplied renderer only clears the frame.

The template seeds `features/game-state` and the `platform-sdk` web bootstrap.
It installs no ad points, economy, or portal gameplay policy. Add dependencies only
when the game uses them. For a fuller integration reference with optional
examples, see `ai_studio/feature_testing/consumer`.

## Lifecycle

The web shell exposes effective safe-area insets to UI Kit through
`window.__gameSafeAreaInsets()`. Phone-sized screens with a coarse primary
pointer reserve at least 32 CSS pixels at the top in portrait and at both sides
in landscape. Larger browser insets win; they are never added to this minimum.
Desktop and larger touch screens use the browser values unchanged. Screen
orientation takes precedence over viewport aspect ratio so an opened keyboard
does not rotate the protected edges. This minimum is a defensive layout margin,
not a measurement of the physical cutout. Keep the world canvas full-screen and
use the effective insets for UI placement and hit regions.

`init -> update -> update_root -> react -> record -> draw_world -> draw_ui -> shutdown`

`World` holds runtime state. Persistent state is declared in
`state/game_state.schema.json` and is registered through the save lifecycle in
`src/shell/shell_boot.c` and loaded in `src/shell/shell_tick.c`.

## Web and release tooling

Run `node tools/build_web.mjs --preset wasm-release`, then
`node tools/game.mjs verify --target itch --template-proof`. The CI scaffold and release procedure
are retained so a game can add features without rebuilding its delivery path;
the build and release CLI itself is Studio-owned (`ai_studio/game_tools`) and
`tools/game.mjs` and `tools/build_web.mjs` are thin entrypoints into it. See `release/README.md`.

The template proof requires an empty frame and two WebGL context-loss/recovery
cycles. Normal game verification requires rendered content. The foundation has
no persistent GPU resources; after recovery it acknowledges the first presented
frame so the web bootstrap can clear its recovery overlay.

## Optional music delivery

Place licensed, game-owned MP3 tracks under `assets/audio/music/`. The native
pack builder discovers them recursively, writes `assets/music.ntpack` beside
`game.ntpack`, and generates typed IDs in the build's `generated/game/music.h`.
For example, `assets/audio/music/town.mp3` becomes `audio/music/town` and
`ASSET_BLOB_AUDIO_MUSIC_TOWN`. Short cues belong in the core game pack.

`game_music_pack` starts the optional request on an update after the first
complete frame has been swapped to the screen. Never add music readiness to
the startup barrier. Slow delivery, HTTP retry or invalid music cannot stop
the world. Once audio/settings are installed, pass their enabled and nonzero
master/music mix to `game_music_pack_update(wanted)` instead of the starter's
`true`; muted music then waits. After terminal failure, changing wanted from
false to true remounts and tries again. HTTP failures retain the engine's
normal backoff. Shut audio down before the shell unmounts the pack.

This is delivery infrastructure, not a music player. Install `audio-core`
following `features/audio-core/INSTALL.md`, request track BLOBs only after
`game_music_pack_state() == NT_PACK_STATE_READY`, and decode long MP3 tracks
with `audio_clip_stream`. Playback also needs a real browser gesture and the
game's pause/attention policy. The integration consumer demonstrates that
catalog wiring without putting demonstration audio into this blank starter.

Without tracks, the starter links no resource HTTP/FS loader, downloads no
music and emits no music pack. Removing the last track removes stale staged
music and its generated header. Web export already stages the optional pack;
the copied game owns its music files, licenses and later catalog changes.

Focused checks from this folder:

```text
node tools/game.mjs test --template-proof --only test_game_music_pack --only music_packaging
```
