# AGENTS.md

## Current Work

- Focus: universal AI pipeline cleanup; keep process lighter than game work.
- `Backrooms Liminal` is closed unless the lead reopens game work.
- Engine submodule: `external/neotolis-engine`; use public APIs and evidence-backed engine tasks only.

## Source Map

- Game/runtime: `src/`.
- Runtime infra: `state/`, `tools/state_codegen/`, `src/devapi/`, `tools/devapi/`, `src/game_storage.*`, `external/cjson/`.
- Design: GDDs in `gamedesign/projects/<game-id>/`, knowledge in `gamedesign/knowledge/`, sources in `gamedesign/sources/`.
- Work/status: `tasks/`; rules in `tasks/README.md`.
- Skills: `.codex/skills/`; `.claude/skills/` is generated.
- Workflow: `AI_PIPELINE.md`; transient output: `tmp/`.

## Context Rules

- Workflow: `AI_PIPELINE.md` and `docs/ai-pipeline/agent-workflow.md`.
- For substantial work, load taskboard context, one task/evidence file, and one matching skill.
- Skip archives, old logs, broad design, builds, and generated files unless task-linked or requested.
- Non-trivial game/visual/pipeline/tooling work needs AI profiling scope or unavailable note.

## Game Work Rules

- Native PC is the playable harness; no web/mobile/server/frontend path for playable work without explicit approval.
- Start new games with a fresh project wiki and `tasks/active/` work items.
- Prefer small playable native slices; tests, spikes, prototypes, and slices use the same gates.
- Game visuals use real assets through the engine asset path; shape/debug renderers are debug-only.
- First playable screen: one location, primary path, next action, visible progress, clear locked states.
- If the user names a reference or mismatch, write the smallest honest Reference Digest before implementation.

## Gates

- Gate taxonomy and validation defaults: `docs/ai-pipeline/quality-validation.md`.
- Builds/probes/audits are evidence, not acceptance.
- Visual/readability work needs screenshot evidence, zoomed UI/text proof, and before/after judgment.
- Lead rejection freezes feature/content expansion until fixed.
- Repeated strict/product failures must change path: `node tools/product_gate/repeated_failure_guard.mjs`.

## Validation

- Docs/tasks: `node tools/taskboard/cli.mjs validate`.
- Skills/process: `node tools/skills_eval.mjs`.
- Pipeline: `node tools/ai.mjs validate`; add `--review` for context, `--full` for export/runtime.
- Product/readability: `node tools/ai.mjs gate` or product gate tool with screenshots.
- Native playable changes: smallest proving build/run plus screenshot/video.

When friction repeats, prefer a tool, validator, skill, or source-of-truth fix
over another long rule.
