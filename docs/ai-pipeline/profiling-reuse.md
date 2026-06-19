# Profiling And Reuse Reference

Detailed portable rules for profiling, visual/asset routing, prototype closeout,
and export. Load this file when changing profiler behavior, prototype lifecycle,
visual pipeline routing, or portable-base export.

## Profiling

Profiling is passive telemetry for finding repeated failures, slow commands,
large context reads, and long gaps. It should not become another project.

Use `node tools/ai.mjs` for normal work: `start`, `run`, `context`,
`checkpoint`, `status`, and `reflect`.

Deep import/status/retrospective work is opt-in for AI workflow review or
profiler fixes. Do not commit raw telemetry from `tmp/session_profiles/`; commit
only durable lessons, rules, tools, or task changes.

## Assets And Visual Work

Use skills instead of copying asset procedure into hot docs. The common route is
`primary-gdd-pipeline` -> `game-visual-art-direction` ->
`generated-game-ui-assets` / `game-asset-pipeline` ->
`game-feature-iteration` / `game-runtime-automation`, loading only the skill
that matches the current task.

Generated/free art is allowed only as runtime-ready art that reaches the visual
target. Debug/procedural placeholders prove geometry; they do not satisfy a
final-art claim unless explicitly recorded as debug debt.

## Prototype Pause Or Close

When the lead says a prototype/game is done, stopped, or only a test, stop game
implementation. Then follow the latest explicit instruction for task/status
disposition. Do not silently archive, drop, or rewrite active work unless that
is part of the requested pipeline cleanup.

Preserve evidence historically. Promote only reusable lessons into pipeline
docs/skills/tools.

## Reuse In A New Project

The portable AI workflow is exported with:

```powershell
node tools/bootstrap/export_base.mjs --target C:\projects\new-game
```

Portable by default: agent skills, taskboard, `tools/ai.mjs`,
`tools/pipeline_validate.mjs`, product gate tools, game-context tools, generated
art job scaffolding, reusable design knowledge, and starter agent/task files.
The exact allowlist lives in `tools/bootstrap/export_base.mjs`.

Runtime seed files (`src/`, `state/`, DevAPI, CMake presets) move only when the
exporter/runtime template explicitly supports that target.
