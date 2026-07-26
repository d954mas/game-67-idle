# Capture Stage Core Install

Status: approved specification only. There is no installable source module yet.

The intended installation model is an in-place module referenced from
`features/capture-stage-core/` by both `templates/template` and games.

## Planned wiring

1. Add the shared core source and include directory to a capture-enabled native
   target.
2. Define a game-owned static stage catalog and callbacks.
3. Register the optional `game.capture.*` adapter only when both capture tooling
   and DevAPI are enabled.
4. Add game-owned shot documents under `capture/shots/`.
5. Extend the game's base `recording-native` target with the
   `authored-capture` preset: DevAPI, capture-stage-core, 60 Hz host wiring, and
   capture diagnostics. Simple `capture live` remains independent of this
   feature.
6. Validate one template stage and one second real-game consumer before marking
   the feature reusable.

Exact source paths, flags, commands, and uninstall steps will be added through
the approved
[`CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md`](../../ai_studio/runtime_automation/CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md).
Until then, this file deliberately makes no installation claim.
