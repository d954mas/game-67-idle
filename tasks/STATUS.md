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

- Backrooms runtime/perf evidence remains in `build/captures/` and
  `gamedesign/projects/backrooms-liminal/reviews/`.
- Engine render-target issue evidence remains in
  `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md`.
- Pipeline source-of-truth docs are `AGENTS.md`, `AI_PIPELINE.md`,
  `tasks/README.md`, and `.codex/skills/`.
- `generated-game-ui-assets` now keeps its hot `SKILL.md` as a short router and
  loads workflow/gate details from
  `.codex/skills/generated-game-ui-assets/references/ui-workflow-gates.md`.
- `game-visual-art-direction` also keeps its hot `SKILL.md` as a short router
  and loads visual workflow/gate detail from
  `.codex/skills/game-visual-art-direction/references/visual-workflow-gates.md`.
- `game-feature-iteration` now keeps its hot `SKILL.md` as a short router and
  loads playable/reference/product/build gate detail from
  `.codex/skills/game-feature-iteration/references/playable-feature-gates.md`.
- `game-asset-pipeline` now keeps its hot `SKILL.md` as a short router and
  loads source/cutout/pack-builder detail from
  `.codex/skills/game-asset-pipeline/references/`.
- `primary-gdd-pipeline` now keeps its hot `SKILL.md` as a short router and
  loads core DoD, reference, fake-shot, runtime asset, and handoff gates from
  `.codex/skills/primary-gdd-pipeline/references/gdd-core-gates.md`.
- `delegated-image-generation` now keeps its hot `SKILL.md` as a short router
  and loads generation paths, verification/prompt gotchas, throughput, and asset
  handoff detail from `.codex/skills/delegated-image-generation/references/`.
- `game-state-management` now keeps its hot `SKILL.md` as a short router and
  loads state workflow/rules, state contract, and review checklist detail from
  `.codex/skills/game-state-management/references/`.

## Next Priorities

1. Continue splitting oversized skills only when their details can move behind
   references without losing eval coverage.
2. Keep process fixes mechanical: validators/tools first, prompt rules second.
3. Run quick reusable pipeline validation before committing pipeline changes.
