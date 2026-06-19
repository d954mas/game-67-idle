# Project Status

## Current Goal

Improve the universal AI pipeline: reduce hot Markdown/context, remove duplicated
rules, and enforce repeated visual/product failure stops with tooling. Backrooms
Liminal is a finished/closed game experiment unless the lead explicitly reopens
game implementation.

## Blocking Work

- None known for the current pipeline cleanup.

## Non-blocking Debt

- Backrooms task files T0001-T0011 remain as historical/review evidence from the
  finished experiment. Do not resume game implementation from them unless the
  lead asks to reopen that game.
- T0011 still documents the engine render-target/framebuffer issue for future
  portal rendering work.
- Raw session profiling evidence may be stale; use it only for requested AI
  workflow reviews after importing/checking the current session data.

## Current Gate

Current gate is reusable pipeline quality, not game content:

- hot docs are short maps, not duplicated encyclopedias;
- detailed procedures live in skills/references/tools;
- repeated strict/product gate FAIL loops are caught by
  `tools/product_gate/repeated_failure_guard.mjs`;
- portable validation still passes.

## Required Validation

```powershell
node tools/product_gate/repeated_failure_guard.mjs
node --test tools/product_gate/test.mjs
node --test tools/pipeline_validate.test.mjs
node tools/skills_eval.mjs
node tools/taskboard/cli.mjs validate
node tools/pipeline_validate.mjs
```

## Last Known Good Evidence

- Backrooms runtime/perf evidence remains in `build/captures/` and
  `gamedesign/projects/backrooms-liminal/reviews/`.
- Engine render-target issue evidence remains in
  `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md`.
- Pipeline source-of-truth docs are `AGENTS.md`, `AI_PIPELINE.md`,
  `tasks/README.md`, and `.codex/skills/`.

## Next Priorities

1. Finish Markdown reduction without reintroducing duplicate rules.
2. Validate the reusable guard and portable pipeline.
3. Commit the pipeline cleanup as a universal process/tooling change.
