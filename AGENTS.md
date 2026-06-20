# AGENTS

## Now

- Focus: clean AI-first native game seed.
- No active game concept; capture user's concept before game work.
- Engine: `external/neotolis-engine`; public APIs only.

## Map

- Game/runtime: `src/`.
- Infra: `state/`, `tools/state_codegen/`, DevAPI, `src/game_storage.*`.
- Game tools/scripts: `tools/<game-id>/` only; reset removes them.
- Design: projects in `gamedesign/projects/<game-id>/`; reusable knowledge:
  `gamedesign/knowledge/`; source notes in `gamedesign/sources/`.
- Asset library:
  `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- Work/status: `tasks/`; rules in `tasks/README.md`.
- Skills: `.codex/skills/`; `.claude/skills/` is generated.
- Workflow: `AI_PIPELINE.md`; temp: `tmp/`.

## Invariants

- Native PC is playable/automation unless lead approves otherwise.
  Mobile/web informs controls, scale, readability, sessions, performance.
- Game/world/UI logic is Y-up. Convert Y-down platform/input only at boundary.
- Product/playable visuals use engine fonts/text renderer;
  handmade pixel/shape `draw_text` is debug-only.
- Important visuals use real legal assets early; debug/procedural
  placeholders are temporary debug only.
- Downloaded/shared assets need license, provenance, integrity before import.
  Runtime uses project-local copies.

## Context

- Workflow: `AI_PIPELINE.md` and `docs/ai-pipeline/agent-workflow.md`.
- Substantial work loads taskboard context, one task/evidence file, one skill.
- Skip archives, logs, broad design, builds, generated files unless linked.
- Context budget cleanup/validation is request/review-only, not a dev gate.

## Game Work

- Start new games with fresh project wiki and `tasks/active/` work items.
- Prefer small native slices; tests, spikes, prototypes use same gates.
- First screen: one location, primary path, next action, visible progress, locks.
- Named refs/mismatches need the smallest honest Reference Digest first.

## Gates

- Gate taxonomy/defaults: `docs/ai-pipeline/quality-validation.md`.
- Builds/probes/audits are evidence, not acceptance.
- Visual work needs screenshot evidence, UI/text zoom, before/after judgment.
- Lead rejection freezes feature/content expansion until fixed.
- Repeated strict/product failures must change path:
  `node tools/product_gate/repeated_failure_guard.mjs`.

## Validate

- Docs/tasks: `node tools/taskboard/cli.mjs validate`.
- Skills/process: `node tools/skills_eval.mjs`.
- Pipeline: `node tools/ai.mjs validate`; add `--review` for context/caps,
  `--full` for export/runtime.
- Product/readability: `node tools/ai.mjs gate` or product gate with screenshots.
- Native playable changes: smallest proving build/run plus screenshot/video.
- Playable smoke prints named acceptance checks plus a compact summary.

When friction repeats, prefer a tool, validator, skill, or source fix.
