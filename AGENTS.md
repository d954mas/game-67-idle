# AGENTS

## Now

- One active game at a time; concept in `## Project`. Closed prototypes are git tags.
- Engine `external/neotolis-engine`, public APIs only.

## Map

- A game is a FOLDER from `template/` (`new_game.mjs`): `<game>/{src,state,assets}/`.
  Root = shared pipeline (engine + tools + skills + docs + tasks), not a game.
  Per-game `tools/<game-id>/` + `gamedesign/projects/<game-id>/`; reusable
  `gamedesign/{knowledge,sources}/`.
- Asset library (3700+ glb; REUSE FIRST, skill `game-3d-models`):
  `C:\Users\ROG\YandexDisk\gamedev\assets\ai_pipeline_assets`.
- Work/status `tasks/` (`tasks/README.md`); skills `.codex/skills/` (`.claude/skills/`
  generated via `tools/sync.mjs`); workflow + commands `AI_PIPELINE.md`; temp `tmp/`.

## Invariants

- Native PC is playable/automation unless lead approves; mobile/web informs
  controls, scale, readability, sessions, performance.
- Game/world/UI logic is Y-up. Convert Y-down platform/input only at boundary.
- All text uses the engine text renderer with real fonts. Never write handmade
  pixel/shape text-drawing (`draw_text`) — not even for debug.
- Engine before you hack: use the engine's public API (UI widgets `nt_ui_*`,
  renderers, math) before hand-rolling; custom only when nothing fits, with a logged
  reason. Decompose — styles/theme in their own file.
- Source before generate: library → free CC0/OFL → generate
  (`tools/assets/source/find_assets.mjs`); procedural is debug-only, logged.
- Downloaded/shared/generated assets need license, provenance, integrity, `origin`
  (mine|ai|sourced); catalog reusable ones (`tools/assets/intake/`) before copying
  project-local. Runtime uses project-local copies.
- Every committed asset records a license; paid/non-redistributable binaries never
  enter git — they live in each game's gitignored `<game>/assets/restricted/`
  (only the `.md` committed), enforced by the guard.

## Working

- Load only what applies: substantial work loads taskboard + one task/evidence + one
  skill; skip archives/logs/broad-design/builds/generated unless linked. Match
  ceremony to change size (`AI_PIPELINE.md` Change-Size Tiers).
- Lead delegates read-heavy work to subagents (`subagent-packet-template --preset`),
  owns integration/status/validation. Workflow + delegation:
  `docs/ai-pipeline/agent-workflow.md`.
- Game work: prefer small native slices; new games get a fresh project wiki +
  `tasks/active/` items. First screen = one location, primary path, next action,
  visible progress, locks.

## Gates

- Gate taxonomy/done-criteria + validate commands:
  `docs/ai-pipeline/quality-validation.md` and `AI_PIPELINE.md`. Builds/probes/audits
  are evidence, not acceptance; lead rejection freezes feature/content expansion.
- 3D model work: `tools/product_gate/visual_material_floor.mjs` must pass —
  flat-tint/fallback GLB rendering is a product fail. Visual work: screenshot +
  before/after judgment.
- When friction repeats, prefer a tool, validator, skill, or source fix.

## Project

- Active game concept: none — root is the shared pipeline + `template/`.
- New game: `node tools/bootstrap/new_game.mjs --id <id>`, then customise + pull
  assets/systems. Closed prototypes are git tags.
