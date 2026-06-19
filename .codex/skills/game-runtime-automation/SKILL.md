---
name: game-runtime-automation
description: "Use when adding, using, or improving game runtime automation, or visually testing a running build: DevAPI command buses, endpoints/command.describe contracts, ui.tree/ui.click, frame.wait, synthetic input, gameplay bots, smoke tests, screenshots, recordings, native PC validation, or replacing temporary game-side automation with engine-native tooling. Also covers visual QA: checking how the game looks, auditing UI/camera/rendering/animation output, comparing desktop and web builds, and confirming a scene is nonblank, readable, correctly framed, and playable."
---

# Game Runtime Automation

Use this skill to let an agent observe, drive, and verify a running game through
runtime contracts instead of screenshots, guesses, or ad hoc OS input.

## Load Only What Applies

- `references/runtime-workflow-rules.md`: launch/build discovery, project
  scaffold, temporary adapters, release-build policy, bots, logs, native-first
  validation, screenshots, recordings, and evidence storage.
- `references/visual-qa-checklist.md`: Visual QA procedure for checking what the
  player sees: native desktop/PC proof, WASM/web scope, nonblank output,
  readable UI text, controls respond, and first-screen focus questions.
- `references/devapi-pattern.md`: DevAPI command bus, `endpoints`,
  `command.describe`, `ui.tree`, `ui.click`, `frame.wait`, ordered batches,
  input gestures, capture patterns, and Required command metadata.

## Default Workflow

1. Read project rules and existing launch/build tasks.
2. Find the runtime bridge first: search for `devapi`, `automation`, `input`,
   `screenshot`, `record`, `bot`, `smoke`, and `--devapi`.
3. Prefer a narrow DevAPI command bus over ad hoc OS input.
4. Discover the runtime contract before acting: call `endpoints`, then
   `command.describe` for unfamiliar or risky commands.
5. Capture launch stdout/stderr to `build/logs` or an equivalent known log
   whenever automation starts the game process.
6. Validate with native desktop/PC builds first. Run WASM/web checks only when
   requested or when the task targets web behavior.

## Non-Negotiables

- Runtime command registration is the source of truth. Inspect code only when
  metadata is missing or behavior conflicts with the runtime contract.
- Keep reusable automation at the low-level device/input/frame/capture layer.
- Add semantic `action.*` endpoints only as game-local convenience wrappers.
- Do not enable automation in release builds unless explicitly requested by
  project policy.
- If a command appears in `endpoints`, it should work in that build or be
  explicitly marked disabled by runtime policy.
- Evidence beats claims: report useful screenshots, recordings, logs, or
  structured observations.
