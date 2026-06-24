# AGENTS

## Now

- Focus: one active game at a time; current concept in `## Project`. Closed
  prototypes are git tags.
- Engine: `external/neotolis-engine`; public APIs only.

## Map

- Game/runtime: `src/`; infra `state/` + `tools/state_codegen/` + DevAPI; game
  tools `tools/<game-id>/`. Design `gamedesign/projects/<game-id>/`; reusable
  knowledge+sources `gamedesign/{knowledge,sources}/`.
- Asset library (3700+ glb, OKF; REUSE FIRST — skill `game-3d-models`):
  `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- Work/status: `tasks/` (rules `tasks/README.md`). Skills: `.codex/skills/`
  (`.claude/skills/` generated). Workflow: `AI_PIPELINE.md`; temp: `tmp/`.

## Invariants

- Native PC is playable/automation unless lead approves; mobile/web informs
  controls, scale, readability, sessions, performance.
- Game/world/UI logic is Y-up. Convert Y-down platform/input only at boundary.
- All text uses the engine text renderer with real fonts. Never write handmade
  pixel/shape text-drawing (`draw_text`) — not even for debug.
- Engine before you hack: use the engine's public API (UI widgets `nt_ui_*`,
  renderers, math) before hand-rolling; custom only when nothing fits, with a logged
  reason. Decompose — styles/theme in their own file.
- Source before you generate: library → free CC0/OFL → generate
  (`tools/assets/source/find_assets.mjs` does both, records the call);
  procedural/shape-renderer is debug-only with a logged reason.
- Downloaded/shared/generated assets need license, provenance, integrity, and
  `origin` (mine|ai|sourced); catalog reusable ones (`tools/assets/intake/`)
  before copying project-local. Runtime uses project-local copies.
- Every committed asset records a license; paid/non-redistributable binaries never
  enter git — they go under gitignored `assets/restricted/` (only the `.md` is
  committed), enforced by the guard.

## Context

- Workflow: `AI_PIPELINE.md`, `docs/ai-pipeline/agent-workflow.md`.
- Substantial work loads taskboard + one task/evidence + one skill; skip archives/
  logs/broad-design/builds/generated unless linked.
- Lead delegates read-heavy work to subagents (`subagent-packet-template --preset`);
  owns integration/status/validation. `docs/ai-pipeline/orchestration-playbook.md`.

## Game Work

- Start new games with fresh project wiki and `tasks/active/` work items.
- Prefer small native slices; tests, spikes, prototypes use same gates.
- First screen: one location, primary path, next action, visible progress, locks.

## Gates

- Gate taxonomy/defaults: `docs/ai-pipeline/quality-validation.md`.
- Builds/probes/audits are evidence, not acceptance.
- Visual work: screenshot evidence + before/after judgment.
- Lead rejection freezes feature/content expansion until fixed.
- 3D model work: `tools/product_gate/visual_material_floor.mjs` must pass —
  flat-tint/fallback GLB rendering is a product fail.
- Repeated strict/product failures must change path:
  `node tools/product_gate/repeated_failure_guard.mjs`.

## Validate

- Docs/tasks: `node tools/taskboard/cli.mjs validate`.
- Skills/process: `node tools/skills_eval.mjs`.
- Pipeline: `node tools/ai.mjs validate` (`--full` = export/runtime).
- Product: `node tools/ai.mjs gate` (with screenshots).
- Native playable: smallest proving build/run + screenshot; smoke prints named
  checks + summary.

When friction repeats, prefer a tool, validator, skill, or source fix.

## Project

- Active game concept: `Blockside Heat` (blockside-heat) — low-poly Roblox-like
  open-world crime prototype: street jobs, vehicles, NPCs, toy-like city chaos.
