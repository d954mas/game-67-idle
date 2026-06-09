---
name: game-runtime-automation
description: "Use when adding, using, or improving game runtime automation: DevAPI command buses, endpoints/command.describe contracts, ui.tree/ui.click, frame.wait, synthetic input, gameplay bots, smoke tests, screenshots, recordings, native PC validation, or replacing temporary game-side automation with engine-native tooling."
---

# Game Runtime Automation

Use this skill to let an agent observe, drive, and verify a running game.

## Workflow

1. Read project rules and existing launch/build tasks.
2. Find the runtime bridge first: search for `devapi`, `automation`, `input`, `screenshot`, `record`, `bot`, `smoke`, `--devapi`.
3. Prefer a narrow DevAPI command bus over ad hoc OS input.
4. Discover the runtime contract before acting: call `endpoints`, then `command.describe` for unfamiliar or risky commands.
5. Treat runtime command registration as the source of truth. Inspect code only when metadata is missing or behavior conflicts with the runtime contract.
6. Keep reusable automation at the low-level device/input/frame/capture layer.
7. Add semantic `action.*` endpoints only as game-local convenience wrappers.
8. Keep the transport and protocol stable even if implementation is temporary.
9. Add bots as scripts that perform observe -> act -> frame.wait -> observe and exit nonzero on failure.
10. Capture evidence after interactions: screenshots for visual checks, short recordings for timing/animation issues.
11. Do not enable automation in release builds unless explicitly requested by project policy.
12. Validate with native desktop/PC builds first. Run WASM/web checks only when explicitly requested or when the task targets web behavior.

## Temporary Adapters

If the engine lacks automation features, add a game-local adapter behind a build flag. Keep it isolated so it can be deleted when the engine provides the same API.

Use the same external commands the future engine API should support. Treat the adapter as a compatibility shim, not game logic.

## Command Registry

Register each DevAPI command once with its handler and metadata. Do not create a separate JSON command database as a second source of truth.

Required command metadata:

- `method`
- `layer`: `engine` or `game`
- `summary`
- `params_shape`
- `result_shape`
- `frame_behavior`
- `side_effects`

Provide discovery commands:

- `endpoints`: quick list of available commands.
- `command.describe`: full contract for one command.
- Optional `command.describe_all` or `endpoints detail=true`: full startup discovery.
- Optional `features`: active feature groups and policy state.

If a command appears in `endpoints`, it should work in that build. Disabled or unavailable commands should be absent, unless the runtime explicitly marks them disabled.

Use build/runtime policy as an allow-list. A project may expose read-only commands while disabling input, UI interaction, capture, or game-specific commands.

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
