# Project Status

Short live project-status index. Workflow rules live in `tasks/README.md`.

## Current Goal

Clean template base for the next game iteration.

## Active Product State

- No active game concept is selected.
- Active runtime: native `Game Seed` template in `src/main.c`.
- Current build target: `game_seed`.
- Current task queue: empty until the next concept/GDD is started.

## Source Pointers

- Start here: `README.md`, `AGENTS.md`, `AI_PIPELINE.md`.
- Reusable design knowledge: `gamedesign/knowledge/`.
- Runtime state schema: `state/game_state.schema.json`.
- DevAPI probes: `tools/devapi/smoke_test.py`, `tools/devapi/full_probe.py`.

## Current Evidence

- Template runtime is expected to build with `cmake --build --preset native-debug`.
- DevAPI smoke should validate `ui.tree`, `ui.click`, `game.state`, state set/get, and screenshot capture.

## Blocking Work

- None.

## Next Priorities

1. Choose or create the next game concept.
2. Create a fresh GDD/design folder under `gamedesign/`.
3. Add only the first playable slice tasks to `tasks/active/`.
4. Implement and validate through the native runtime first.

## Validation Policy

- Normal game work: run the narrow native scenario or probe that proves the changed behavior.
- AI/tooling work: use passive, narrow tests first. Broad portable validation requires explicit release/portable/shared-behavior need.
- Web work remains out of scope unless the user explicitly asks for web/mobile or approves a stated exception.
