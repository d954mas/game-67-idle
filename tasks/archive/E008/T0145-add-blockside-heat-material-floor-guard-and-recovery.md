---
id: T0145
title: Add Blockside Heat material-floor guard and recovery path
status: done
epic: E008
priority: P0
tags: [prototype, blockside-heat, visual, assets, materials, lead-rejection, pipeline]
created: 2026-06-24
updated: 2026-06-24
---

## What

Lead feedback: models are still one-color because the runtime accepts sourced
GLB/GLTF geometry while discarding material/texture evidence. Add a mechanical
guard so this is caught automatically, and keep content expansion frozen until
the runtime has a real material/texture path.

Scope exclusions: no new story beats, map expansion, traffic simulation,
economy, weapon inventory, or NPC behavior until the material floor is green.

## Done when

- [x] `tools/product_gate/visual_material_floor.mjs` fails the current flat
      tint/fallback-material path.
- [x] Product-gate tests cover fail/pass material-floor scenarios.
- [x] `node tools/ai.mjs validate` routes through the material-floor guard.
- [x] Status/taskboard identify material rendering as the current blocker.

## Open questions

## Log

- 2026-06-24: Created from lead correction: the pipeline must automatically
  reject "GLB geometry + one flat tint" before showing it as accepted visuals.
- orchestration: used
  objective: add a mechanical material-floor guard for active 3D asset games
  allowed files: tools/product_gate/visual_material_floor.mjs, tools/product_gate/test.mjs, tools/pipeline_validate.mjs, docs/ai-pipeline/quality-validation.md, AGENTS.md, tasks/STATUS.md, tasks/active/T0144-add-blockside-heat-score-staging-lead-beat.md, tasks/active/T0145-add-blockside-heat-material-floor-guard-and-recovery.md
  tool-use guard: keep scope to guard/tests/docs/task status; do not add gameplay or asset rendering implementation in this task
  expected output: guard fails the current flat-tint GLB path and tests cover fail/pass fixtures
  evidence command: node --test tools/product_gate/test.mjs; node tools/ai.mjs validate --dry-run; node tools/product_gate/visual_material_floor.mjs
  stop condition: guard is wired into validation and current runtime is mechanically red for material-floor failure
  independent reviewer: taskboard validate plus product_gate tests
- 2026-06-24: Done. Evidence: `node --test tools/product_gate/test.mjs` PASS
  (53 tests); `node tools/ai.mjs validate --dry-run` includes
  `visual material floor guard`; `node tools/taskboard/cli.mjs validate` PASS;
  `node tools/product_gate/visual_material_floor.mjs` FAILS current runtime with
  `assets/shaders/blockside_mesh_inst.frag` color-only shader and
  `src/clean_seed_main.c` flat material path.
