# Project Status

## Current Goal

Improve the universal AI pipeline: reduce hot Markdown/context, remove duplicated
rules, and enforce repeated visual/product failure stops with tooling. Backrooms
Liminal is a finished/closed game experiment unless the lead explicitly reopens
game implementation.

## Blocking Work

- None known for the current pipeline cleanup.

## Non-blocking Debt

- T0001-T0011 are historical Backrooms review evidence. Do not resume game
  implementation unless the lead explicitly reopens it.
- T0011 tracks the engine render-target/framebuffer issue for future portal
  rendering work.
- Raw profiling evidence can be stale; import/check current session data before
  AI workflow reviews.
- Full deep asset validation needs a Node-visible Python with `PIL`, `numpy`,
  `scipy`, and `pymatting`; normal workflow/tooling gate is quick validation.
  Use `AI_PIPELINE_PYTHON` for project-specific Python commands or venvs.

## Current Gate

Reusable pipeline quality, not game content:

- hot docs stay short maps;
- detailed procedures live in skills/references/tools;
- `tools/skills_eval.mjs` includes skill `references/*.md`;
- repeated strict/product FAIL loops are guarded;
- portable validation remains explicit.

## Required Validation

Normal gate: `node tools/pipeline_validate.mjs`.

Use `node tools/pipeline_validate.mjs --full` for portable export/runtime/deep
asset coverage.

## Last Known Good Evidence

- Historical pipeline evidence: `tasks/archive/README.md`.
- Source-of-truth docs: `AGENTS.md`, `AI_PIPELINE.md`, `docs/ai-pipeline/`,
  `tasks/README.md`, `.codex/skills/`.
- Quick validation: `node tools/pipeline_validate.mjs`.
- Full/export validation: `node tools/pipeline_validate.mjs --full`.

## Next Priorities

1. Continue splitting oversized skills only when their details can move behind
   references without losing eval coverage.
2. Keep process fixes mechanical: validators/tools first, prompt rules second.
3. Run quick reusable pipeline validation before committing pipeline changes.
