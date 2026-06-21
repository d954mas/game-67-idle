# Profiling And Reuse Reference

Portable rules for profiling, visual/asset routing, prototype closeout, and export.

## Profiling

Profiling is passive telemetry for repeated failures, slow commands, and gaps.

The PostToolUse hook records tool calls automatically. Review a session with
`node tools/ai.mjs status`; use `--verbose` for coverage gaps. `status` runs
`node tools/ai.mjs import-codex-session` to recover missed Codex failures.
Use `--agent-rollup` for optional analysis-time subagent diagnostics; pass
`--parent-thread-id` for subagent session metadata, or `--trace-session` for
parent transcript spawn/wait/close checks. This is diagnostic evidence, not
automatic acceptance of agent work. When a parent session id and matching
subagent session metadata exist, but agent rollup is omitted, `status` prints
the focused `--agent-rollup` command so subagent work is not invisible.

Do not commit raw telemetry from `tmp/session_profiles/`; commit only durable
lessons, rules, tools, or tasks.

## Assets And Visual Work

Use skills instead of copying asset procedure into hot docs. Common route:
`primary-gdd-pipeline` -> `game-visual-art-direction` ->
`generated-game-ui-assets` / `game-asset-pipeline` ->
`game-feature-iteration` / `game-runtime-automation`; load only the matching
skill.

For downloaded/free source assets, use
`C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets` through
`game-asset-pipeline`. It is an OKF-style provenance catalog, not a runtime path:
search `catalog/**/*.md`, then copy selected files into project-local assets.

Generated/free art is allowed only as runtime-ready art that reaches the target.
Debug/procedural placeholders prove geometry, not final-art quality.

## Prototype Pause Or Close

When the lead says a prototype/game is done, stopped, or only a test, stop game
implementation. Do not archive, drop, or rewrite active work unless requested.

Preserve evidence historically. Promote only reusable lessons into pipeline docs,
skills, or tools.

## Reuse In A New Project

The portable AI workflow is exported with:

```powershell
node tools/bootstrap/export_base.mjs --target C:\projects\new-game
```

Portable by default: skills, taskboard, `tools/ai.mjs`,
`tools/pipeline_validate.mjs`, product gates, game-context tools, generated art
job scaffolding, reusable design knowledge, and starter agent/task files. The
allowlist lives in `tools/bootstrap/export_base.mjs`.

Runtime seed files (`src/`, `state/`, DevAPI, CMake presets) move only when the
exporter/runtime template explicitly supports that target.
