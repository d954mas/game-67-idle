# Profiling And Reuse Reference

Portable rules for profiling, visual/asset routing, prototype closeout, and export.

## Profiling

Profiling is passive telemetry for repeated failures, slow commands, and gaps.

The PostToolUse hook records tool calls automatically. Review a session with
`node ai_studio/core_harness/profiling/status.mjs`; use `--verbose` for coverage
gaps. Run `node ai_studio/core_harness/profiling/hook_record.mjs codex --recover-only`
before status when Codex failures are missing from the profile. Use `--agents` for
subagent diagnostics. Agent rollup reports best-effort profile/transcript
telemetry; it is diagnostic, not a strict gate. Tool-usage failures are
orchestration friction, not product/gate failures.

Do not commit raw telemetry from `tmp/session_profiles/`; commit only durable
lessons, rules, tools, or tasks.

## Assets And Visual Work

Use module owners instead of copying asset procedure into hot docs:

- `nt-asset-workflow`: source search, license/provenance, art jobs, prep,
  storage, viewer, pull/promote, generated-source handoff.
- `nt-asset-image-generation`: raster creation only after source-first search.
- `ai_studio/quality`: player-facing visual/art/asset acceptance rules.
- `game-runtime-automation`: screenshot/video proof when the running game is the
  claim.

For downloaded/free source assets, use Asset Storage through
`ai_studio/assets/storage/` and `nt-asset-workflow`. The global library is not a
game runtime path; projects use local copies with source/license metadata.

Generated/free art is accepted only when it reaches the target quality for the
current stage. Debug/procedural placeholders prove geometry, not final-art
quality.

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
module-owned validators, quality rules, game-context tools, generated art
job scaffolding, reusable design knowledge, and starter agent/task files. The
allowlist lives in `tools/bootstrap/export_base.mjs`.

Runtime seed files (`src/`, `state/`, DevAPI, CMake presets) move only when the
exporter/runtime template explicitly supports that target.
