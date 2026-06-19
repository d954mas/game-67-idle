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
- Full deep asset validation currently needs a Python runner visible from Node
  with `PIL`, `numpy`, `scipy`, and `pymatting`; quick reusable pipeline
  validation remains the normal gate for workflow/tooling edits. Override with
  `AI_PIPELINE_PYTHON` when a project has a known Python command/venv; the
  value may include arguments such as `uv run python`.

## Current Gate

Current gate is reusable pipeline quality, not game content:

- hot docs are short maps, not duplicated encyclopedias;
- detailed procedures live in skills/references/tools;
- large skills can be split without losing eval coverage because
  `tools/skills_eval.mjs` checks `references/*.md` as part of the skill body;
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

- Historical pipeline evidence is summarized in `tasks/archive/README.md`.
- Pipeline source-of-truth docs are `AGENTS.md`, `AI_PIPELINE.md`,
  `docs/ai-pipeline/`, `tasks/README.md`, and `.codex/skills/`.
- Quick validation includes skill eval/sync, context budget, repeated product
  fail guard, taskboard validation/tests, profiling tests, game-context tests,
  and product-gate tests via `node tools/pipeline_validate.mjs`.
- Full/export validation is still explicit:
  `node tools/pipeline_validate.mjs --full`.

## Next Priorities

1. Continue splitting oversized skills only when their details can move behind
   references without losing eval coverage.
2. Keep process fixes mechanical: validators/tools first, prompt rules second.
3. Run quick reusable pipeline validation before committing pipeline changes.
