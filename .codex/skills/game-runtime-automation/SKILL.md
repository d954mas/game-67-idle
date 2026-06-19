---
name: game-runtime-automation
description: "Use when adding, using, or improving game runtime automation or visual QA for a running build: DevAPI command buses, endpoints/command.describe contracts, ui.tree/ui.click, frame.wait, synthetic input, gameplay bots, smoke tests, screenshots, recordings, native PC validation, and nonblank/readable/playable scene checks."
---

# Game Runtime Automation

Use to let an agent observe, drive, and verify a running game through runtime
contracts instead of screenshots, guesses, or ad hoc OS input.

## Load Only What Applies

- `references/runtime-workflow-rules.md`: launch/build discovery, adapters,
  bots, logs, native-first validation, evidence, release policy.
- `references/visual-qa-checklist.md`: Visual QA for player-visible output,
  native desktop/PC proof, WASM/web scope, readability, controls.
- `references/devapi-pattern.md`: DevAPI command bus, discovery, UI/input,
  frame sync, capture patterns, and required command metadata.

## Default Workflow

1. Read project rules and existing launch/build tasks.
2. Find the runtime bridge first: search for `devapi`, `automation`, `input`,
   `screenshot`, `record`, `bot`, `smoke`, and `--devapi`.
3. Prefer a narrow DevAPI command bus over ad hoc OS input.
4. Discover the runtime contract before acting: call `endpoints`, then
   `command.describe` for unfamiliar or risky commands.
5. Capture launch stdout/stderr to `build/logs` or equivalent whenever
   automation starts the game process.
6. Validate with native desktop/PC builds first. Run WASM/web checks only when
   requested or when the task targets web behavior.

## Non-Negotiables

- Runtime command registration is the source of truth. Inspect code only when
  metadata is missing or behavior conflicts with it.
- Keep reusable automation at the low-level device/input/frame/capture layer.
- Add semantic `action.*` endpoints only as game-local convenience wrappers.
- Do not enable automation in release builds unless explicitly requested by
  project policy.
- If a command appears in `endpoints`, it should work in that build or be
  explicitly marked disabled by runtime policy.
- Evidence beats claims: report useful screenshots, recordings, logs, or
  structured observations.
