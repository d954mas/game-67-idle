# Project Status

## Current Goal

No active game concept is selected. The repository is a clean AI-first native
game seed waiting for the next user-approved concept.

## Blocking Work

- No game work should start until the next concept, references, platform
  constraints, and first slice are captured.

## Non-blocking Debt

- T0010 is a deferred post-prototype asset consistency idea; do not start it
  before a future game has accepted art direction and batch content.

## Current Gate

Capture the next game concept and create exactly one scoped task or epic before
GDD, assets, gameplay, or runtime implementation.

## Required Validation

```powershell
node tools/taskboard/cli.mjs validate
node tools/ai.mjs validate --review
```

## Last Known Good Evidence

- Clean seed status is aligned with `AGENTS.md`: no active game concept.

## Next Priorities

1. Ask the user for the next game concept.
2. Capture that concept in `gamedesign/projects/<game-id>/`.
3. Create one scoped task or epic for the first native playable slice.
