---
id: T0030
title: Mech lighting and material pop pass
status: review
epic: ""
priority: P0
tags: [implementation, visual, lighting, materials, shaders, 3d, mechs, native]
created: 2026-06-20
updated: 2026-06-20
---

## What

Improve the material and lighting read of the sourced Assault Walker and robot
enemy so the scene feels more like a bright toy/plastic/metal mech game and
less like flat colored geometry. This slice uses the existing normals and mesh
materials; it does not add new gameplay, export targets, or a new asset
pipeline.

Scope boundaries:

- In scope: shader lighting/material response, rim/specular/fill/bounce tuning,
  T0030 screenshots, product gate, and validation evidence.
- Out of scope: new models, rigged animation, UI redesign, web/mobile export,
  generated texture packs, or balance/economy changes.

## Done when

- [x] Native screenshot shows stronger material/light read on the Assault
      Walker than T0029 without hurting readability.
- [x] DevAPI smoke captures a T0030 lighting/material screenshot.
- [x] Native `game_seed` builds and DevAPI smoke passes.
- [x] Strict visual product gate passes with `art_quality >= 4`, or fail is
      logged with next corrective action.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Log

- 2026-06-20: Created after T0029. The arena has more depth; next visual debt is
  mech surface/material pop from lighting and shader response.
- 2026-06-20: Tuned mesh shaders for stronger rim/specular/fill/top light and
  ground-bounce material response. Added T0030 hangar/battle smoke captures.
- 2026-06-20: Native build and DevAPI smoke passed. Evidence:
  `gamedesign/projects/mech-builder-battler/evidence/t0030_mech_lighting_material_pop_2026-06-20.md`.
- 2026-06-20: product gate PASS (desktop-mech-lighting-material-pop); review:
  `gamedesign/projects/mech-builder-battler/reviews/product_read_gate_2026-06-20T01-08-20_desktop-mech-lighting-material-pop.md`;
  screenshot: `build/captures/mech_t0030_lighting_material_battle_smoke.png`;
  next: continue to the next narrow slice.
- 2026-06-20: Validation passed: `node tools/taskboard/cli.mjs validate`,
  `node tools/ai.mjs validate --with-assets`, and `git diff --check`.
