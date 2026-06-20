# AGENTS.md

## Current Work

- Focus: post-prototype pipeline cleanup after `Mech Builder Battler`.
- The mech prototype is stopped; no more game/content/export work without a
  fresh explicit request.
- Engine submodule: `external/neotolis-engine`; use public APIs only.

## Source Map

- Game/runtime: `src/`.
- Runtime infra: `state/`, `tools/state_codegen/`, `src/devapi/`, `tools/devapi/`, `src/game_storage.*`.
- Game-specific tools/scripts: `tools/<game-id>/` only; reset removes that folder.
- Design: GDDs in `gamedesign/projects/<game-id>/`; reusable knowledge in `gamedesign/knowledge/`; sources in `gamedesign/sources/`.
- Shared source asset library: `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- Work/status: `tasks/`; rules in `tasks/README.md`.
- Skills: `.codex/skills/`; `.claude/skills/` is generated.
- Workflow: `AI_PIPELINE.md`; transient output: `tmp/`.

## Context Rules

- Workflow: `AI_PIPELINE.md` and `docs/ai-pipeline/agent-workflow.md`.
- Substantial work loads taskboard context, one task/evidence file, one skill.
- Skip archives, old logs, broad design, builds, and generated files unless linked.
- Profiling is automatic (PostToolUse hook); review a session with `node tools/ai.mjs status`.

## Game Work Rules

- Native PC is the playable harness. Mobile/web informs controls, UI scale,
  readability, session length, and performance; it does not replace PC without approval.
- Start new games with a fresh project wiki and `tasks/active/` work items.
- Prefer small native slices; tests, spikes, prototypes, and slices use same gates.
- Game visuals use real assets through engine asset paths; shape/debug renderers are debug-only.
- For important visuals, use legal downloaded/generated source assets early;
  shared downloads need license, provenance, and integrity notes before import.
- Coordinate convention is Y-up for game/world/UI projections and logical UI
  layout. If a platform/input backend reports Y-down window coordinates, convert
  only at the boundary; do not make gameplay or UI code Y-down internally.
- UI text uses engine fonts. Handmade pixel/shape `draw_text` is debug-only and
  not acceptable in product screenshots or playable visual passes.
- First playable screen: one location, primary path, next action, visible progress, clear locks.
- Named references/mismatches need the smallest honest Reference Digest before implementation.

## Gates

- Gate taxonomy and validation defaults: `docs/ai-pipeline/quality-validation.md`.
- Builds/probes/audits are evidence, not acceptance.
- Visual/readability work needs screenshot evidence, UI/text zoom, before/after judgment.
- Lead rejection freezes feature/content expansion until fixed.
- Repeated strict/product failures must change path: `node tools/product_gate/repeated_failure_guard.mjs`.

## Validation

- Docs/tasks: `node tools/taskboard/cli.mjs validate`.
- Skills/process: `node tools/skills_eval.mjs`.
- Pipeline: `node tools/ai.mjs validate`; add `--review` for context/caps, `--full` for export/runtime.
- Product/readability: `node tools/ai.mjs gate` or product gate tool with screenshots.
- Native playable changes: smallest proving build/run plus screenshot/video.

When friction repeats, prefer a tool, validator, skill, or source-of-truth fix
over another long rule.
