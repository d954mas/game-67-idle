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
