---
name: game-runtime-automation
description: "Use when adding, using, or improving a game runtime automation harness: debug command buses, engine/game communication, synthetic input, gameplay bots, smoke tests, screenshots, recordings, visual evidence, or replacing temporary game-side automation with engine-native tooling."
---

# Game Runtime Automation

Use this skill to let an agent observe, drive, and verify a running game.

## Workflow

1. Read project rules and existing launch/build tasks.
2. Find the runtime bridge first: search for `devapi`, `automation`, `input`, `screenshot`, `record`, `bot`, `smoke`, `--devapi`.
3. Prefer a narrow command bus over ad hoc OS input:
   - `ping`
   - `endpoints`
   - `view`
   - `frame.current`
   - `frame.wait`
   - `ui.tree`
   - `ui.element`
   - `ui.click`
   - `ui.drag`
   - `ui.scroll`
   - `input.gesture`
   - `input.key`
   - `input.move`
   - `input.click`
   - `input.pointer`
   - `input.wheel`
   - `input.button`
   - a game-specific `game.state` or equivalent
4. Keep reusable automation at the low-level device/input/frame/capture layer.
5. Add semantic `action.*` endpoints only as game-local convenience wrappers.
6. Keep the transport and protocol stable even if implementation is temporary.
7. Add bots as scripts that perform observe -> act -> observe and exit nonzero on failure.
8. Capture evidence after interactions: screenshots for visual checks, short recordings for timing/animation issues.
9. Do not enable automation in release builds unless explicitly requested.
10. Prefer native desktop/PC validation for speed. Run WASM/web checks only when explicitly requested or when the task targets web behavior.

## Temporary Adapters

If the engine lacks automation features, add a game-local adapter behind a build flag. Keep it isolated so it can be deleted when the engine provides the same API.

Use the same external commands the future engine API should support. Treat the adapter as a compatibility shim, not game logic.

## Layering

Reusable engine layer:

- raw key/button/pointer/wheel events
- UI tree and widget bounds by stable id
- widget-targeted clicks, drags, scrolls, and gestures
- event queue ordering
- frame sync
- state snapshots
- screenshots and recordings

Game-specific layer:

- semantic actions like `move_right`, `open_menu`, or `buy_upgrade`
- scenario helpers and test fixtures
- domain snapshots such as economy, quests, enemies, or inventory

Do not put game-specific semantic actions into universal skills or engine APIs.

Widget ids must be stable enough for tests. Prefer explicit developer ids over generated layout indices.

## Evidence

Store temporary screenshots, recordings, and smoke logs under build/temp/captures or another ignored scratch path. Report paths to useful evidence.

For detailed endpoint and capture patterns, read `references/devapi-pattern.md`.
