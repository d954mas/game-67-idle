---
name: game-runtime-automation
description: "Use when adding, using, or improving game runtime automation or visual QA for a running build: DevAPI command buses, endpoints/command.describe contracts, ui.tree/ui.click, frame.wait, synthetic input, gameplay bots, smoke tests, screenshots, recordings, native PC validation, and nonblank/readable/playable scene checks."
---

# Game Runtime Automation

Use to observe, drive, and verify a running game through runtime contracts.

## Load Only What Applies

- `references/runtime-workflow-rules.md`: launch/build discovery, adapters, bots,
  logs, native-first validation, evidence, release policy.
- `references/visual-qa-checklist.md`: Visual QA for player-visible output,
  native desktop/PC proof, WASM/web scope, readability, controls.
- `references/devapi-pattern.md`: DevAPI command bus, discovery, UI/input,
  frame sync, capture patterns, and required command metadata.

## Default Workflow

1. Read project rules and existing launch/build tasks.
2. Find the bridge first: search `devapi`, `automation`, `input`, `screenshot`,
   `record`, `bot`, `smoke`, `--devapi`.
3. Prefer a narrow DevAPI command bus over ad hoc OS input.
4. Discover contract before acting: `endpoints`, then `command.describe`.
5. Capture launch stdout/stderr to `build/logs` when automation starts a game.
6. Validate with native desktop/PC builds first. Run WASM/web checks only when
   requested or when the task targets web behavior.

## Non-Negotiables

- Runtime command registration is source of truth; inspect code when metadata
  is missing or behavior conflicts.
- Keep reusable automation at the low-level device/input/frame/capture layer.
- Add semantic `action.*` endpoints only as game-local convenience wrappers.
- Do not enable automation in release builds unless project policy requests it.
- If a command appears in `endpoints`, it should work in that build or be
  explicitly marked disabled by runtime policy.
- Evidence beats claims: report screenshots, recordings, logs, or observations.
