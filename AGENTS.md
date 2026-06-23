# AGENTS

## Now

- Focus: one active game at a time; current concept in `## Project`.
- Closed prototypes (snapshots are git tags): Cozy Automation, Dragon Grove.
- Engine: `external/neotolis-engine`; public APIs only.

## Map

- Game/runtime: `src/`. Infra: `state/`, `tools/state_codegen/`, DevAPI,
  `src/game_storage.*`. Game tools: `tools/<game-id>/` (reset removes them).
- Design: `gamedesign/projects/<game-id>/`; knowledge `gamedesign/knowledge/`;
  sources `gamedesign/sources/`.
- Asset library (3700+ engine-ready glb, OKF; REUSE FIRST — skill `game-3d-models`):
  `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- Work/status: `tasks/` (rules `tasks/README.md`). Skills: `.codex/skills/`
  (`.claude/skills/` generated). Workflow: `AI_PIPELINE.md`; temp: `tmp/`.

## Invariants

- Native PC is playable/automation unless lead approves; mobile/web informs
  controls, scale, readability, sessions, performance.
- Game/world/UI logic is Y-up. Convert Y-down platform/input only at boundary.
- All text uses the engine text renderer with real fonts. Never write handmade
  pixel/shape text-drawing (`draw_text`) — not even for debug.
- Source before you generate: library → free CC0/OFL → generate
  (`tools/assets/source/find_assets.mjs` does both, records the call);
  procedural/shape-renderer is debug-only with a logged reason.
- Downloaded/shared/generated assets need license, provenance, integrity, and
  `origin` (mine|ai|sourced); catalog reusable ones (`tools/assets/intake/`)
  before copying project-local. Runtime uses project-local copies.

## Context

- Workflow: `AI_PIPELINE.md`, `docs/ai-pipeline/agent-workflow.md`.
- Substantial work loads taskboard context, one task/evidence file, one skill;
  skip archives, logs, broad design, builds, generated files unless linked.
- Lead delegates parallelizable read-heavy work to subagents (bounded packets via
  `subagent-packet-template --preset`); owns integration, status, validation.
  Method: `docs/ai-pipeline/orchestration-playbook.md`.

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
- Pipeline: `node tools/ai.mjs validate` (`--review` context/caps, `--full`
  export/runtime).
- Product/readability: `node tools/ai.mjs gate` or product gate with screenshots.
- Native playable changes: smallest proving build/run plus screenshot/video;
  playable smoke prints named checks plus a compact summary.

When friction repeats, prefer a tool, validator, skill, or source fix.

## Project

- No active game concept — lead picks the next game; reuse the asset library
  first (skill `game-3d-models`), then open fresh `tasks/active/` items.
