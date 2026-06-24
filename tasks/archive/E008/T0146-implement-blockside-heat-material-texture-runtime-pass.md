---
id: T0146
title: Implement Blockside Heat material texture runtime pass
status: dropped
epic: E008
priority: P0
tags: [prototype, blockside-heat, visual, assets, materials, lead-rejection]
created: 2026-06-24
updated: 2026-06-24
---

## What

Fix the lead visual rejection that models are one-color. The current runtime
loads GLB/GLTF geometry but renders it through one flat tint/fallback material.
Implement the smallest material/texture path that proves sourced assets keep
authored visual detail in product screenshots.

Scope exclusions: no new story beat, new district, traffic simulation, economy,
weapon inventory, or NPC behavior until this visual blocker is resolved.

## Done when

- [ ] `node tools/product_gate/visual_material_floor.mjs` passes without
      `--allow-color-only`.
- [ ] Runtime uses source material colors/textures, UVs, or an explicit
      per-primitive material table for the main visible model families.
- [ ] Native capture shows at least car/building/prop models no longer read as
      one flat object-level tint.
- [ ] Strict product/readability gate records the material rejection as resolved
      or identifies the next smallest visual blocker.

## Open questions

## Log

- 2026-06-24: Created after T0145 made the failure mechanical. Continue only
  visual/material recovery until the guard and screenshot pass.
