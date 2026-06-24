# Project Status

## Current Goal

Recover the `Blockside Heat` visual contract before any more feature expansion:
GLB/GLTF assets must render with source materials/textures or an explicit
per-primitive material path, not one flat tint/fallback material.

## Blocking Work

- Lead visual rejection: sourced models currently render as flat one-color
  geometry. `tools/product_gate/visual_material_floor.mjs` must pass before
  more story, map, traffic, NPC, economy, or weapon work.

## Non-blocking Debt

- Wider world is still sparse beyond the immediate block; latest visual gate
  records this as minor art-quality debt.

## Current Gate

Material floor gate: `node tools/product_gate/visual_material_floor.mjs`.
Current expected result is FAIL until the runtime proves textured/material GLB
rendering or an explicit accepted debug-only bypass exists.

## Required Validation

`node tools/taskboard/cli.mjs validate`
`node tools/ai.mjs validate --review`

## Last Known Good Evidence

- `gamedesign/projects/blockside-heat/concept.md`
- `gamedesign/projects/blockside-heat/visual/targets/blockside-heat-first-slice-target.png`
- `gamedesign/projects/blockside-heat/visual/live_state_acceptance_matrix.json`
- `tools/blockside-heat/capture_states.py`
- `tmp/blockside-heat/capture-states-report.json`
- Latest screenshot: `tmp/blockside-heat/repo-tool-cache-latest.png`
- Latest strict PASS is stale for the new material complaint:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`

## Next Priorities

1. Run and keep the new material floor guard red on the current failure.
2. Implement a material/texture rendering pass for at least the main model
   families before any new gameplay/story content.
3. Re-capture screenshot evidence and rerun strict product/readability gate.
