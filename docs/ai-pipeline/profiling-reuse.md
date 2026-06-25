# Profiling And Reuse Reference

Portable rules for profiling, visual/asset routing, prototype closeout, and export.

## Profiling

Profiling is passive telemetry for repeated failures, slow commands, and gaps.

The PostToolUse hook records tool calls automatically. Review a session with
`node tools/ai_profile/status.mjs`; use `--verbose` for coverage gaps. `status` runs
`node tools/ai_profile/import_codex_session.mjs` to recover missed Codex failures.
Use `--agent-rollup` for subagent diagnostics; pass `--parent-thread-id` or
`--trace-session` for spawn/wait/close checks. Add
`--require-agent-rollup-ok` only for closeout; it exits nonzero on incomplete
rollups. Agent rollup reports best-effort profile/transcript telemetry; it is
diagnostic, not a strict gate. Tool-usage failures are orchestration friction,
not product/gate failures. If matching metadata exists but rollup is
omitted, `status` prints the command.

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

Portable by default: skills, taskboard,
`tools/pipeline_validate.mjs`, product gates, game-context tools, generated art
job scaffolding, reusable design knowledge, and starter agent/task files. The
allowlist lives in `tools/bootstrap/export_base.mjs`.

Runtime seed files (`src/`, `state/`, DevAPI, CMake presets) move only when the
exporter/runtime template explicitly supports that target.
