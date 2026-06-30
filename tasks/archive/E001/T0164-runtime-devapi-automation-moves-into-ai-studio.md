---
id: T0164
title: Runtime DevAPI automation moves into AI Studio
status: done
epic: E001
priority: P2
tags: [runtime, devapi, legacy, ai-studio]
created: 2026-06-30
updated: 2026-06-30
---

## What

Move the reviewed runtime/DevAPI automation helpers out of legacy `tools/` and
into an AI Studio module. This module owns local runtime proof helpers:
DevAPI client/CLI, native run loop helpers, framebuffer/window capture, video
recording, PNG codec, pixel health, UI readability zoom audit, and live-state
capture matrix helpers.

## Done when

- [x] DevAPI automation files live under `ai_studio/runtime_automation/` with a
      README that states owner, boundaries, and commands.
- [x] Internal paths and usage strings no longer route current work through
      `tools/devapi`.
- [x] Existing Python tests pass, including the pre-existing broken
      `StateCapture.shots_args()` expectation.
- [x] Architecture map owns the module and no longer reports `tools/devapi` as
      current unmapped legacy.
- [x] Taskboard, architecture map, doc references, and skills sync validate.

## Open questions

- Later: decide whether `game-runtime-automation` skill should move/rename under
  this module or remain a legacy skill until skills are reviewed.

## Log

- 2026-07-01: Review found `tools/devapi` owns runtime proof automation, not
  generic tooling. Pre-move pytest found an existing failure:
  `StateCapture.shots_args()` is expected by `state_capture_test.py` but missing.
- 2026-07-01: Moved DevAPI/runtime automation helpers to
  `ai_studio/runtime_automation/`, added README, updated current routes, and
  mapped the module in `ai_studio/tree.json`.
- 2026-07-01: Fixed the pre-existing `StateCapture.shots_args()` test contract.
  Validation passed: runtime automation pytest 20/20, game_project/export tests
  8/8, architecture map validation clean, taskboard validation clean, doc
  reference check clean, and skills sync clean.
- 2026-06-30: 2026-07-01: Closed after moving runtime DevAPI automation into ai_studio/runtime_automation, fixing StateCapture.shots_args, and validating runtime pytest, game_project/export tests, map, taskboard, doc refs, and skills sync.
