# Profiling And Reuse Reference

Portable rules for profiling, visual/asset routing, prototype closeout, and
export. Load when changing profiler behavior, prototype lifecycle, visual
pipeline routing, or portable-base export.

## Profiling

Profiling is passive telemetry for repeated failures, slow commands, and long
gaps. It should not become another project.

The PostToolUse hook records every tool call automatically; there is no manual
step. Review a session with `node tools/ai.mjs status` (`--verbose` for coverage
gaps and parse errors). `node tools/ai.mjs import-codex-session` recovers missed
Codex failures (and `status` runs it by default).

Do not commit raw telemetry from `tmp/session_profiles/`; commit only durable
lessons, rules, tools, or tasks.

## Assets And Visual Work

Use skills instead of copying asset procedure into hot docs. Common route:
`primary-gdd-pipeline` -> `game-visual-art-direction` ->
`generated-game-ui-assets` / `game-asset-pipeline` ->
`game-feature-iteration` / `game-runtime-automation`; load only the matching
skill.

Generated/free art is allowed only as runtime-ready art that reaches the visual
target. Debug/procedural placeholders prove geometry, not final-art quality,
unless recorded as debug debt.

## Prototype Pause Or Close

When the lead says a prototype/game is done, stopped, or only a test, stop game
implementation. Follow the latest explicit task/status instruction. Do not
silently archive, drop, or rewrite active work unless requested.

Preserve evidence historically. Promote only reusable lessons into
pipeline docs/skills/tools.

## Reuse In A New Project

The portable AI workflow is exported with:

```powershell
node tools/bootstrap/export_base.mjs --target C:\projects\new-game
```

Portable by default: skills, taskboard, `tools/ai.mjs`,
`tools/pipeline_validate.mjs`, product gates, game-context tools, generated art
job scaffolding, reusable design knowledge, and starter agent/task files.
The exact allowlist lives in `tools/bootstrap/export_base.mjs`.

Runtime seed files (`src/`, `state/`, DevAPI, CMake presets) move only when the
exporter/runtime template explicitly supports that target.
