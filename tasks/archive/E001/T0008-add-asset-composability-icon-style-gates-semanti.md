---
id: T0008
title: "Add asset-composability + icon-style gates (semantic fidelity: no castle=armor / rock+sign combos; one icon style per set)"
status: done
epic: E001
priority: P2
tags: [pipeline, visual, gate, noise]
created: 2026-06-17
updated: 2026-06-17
---

## What

Add asset-generation/audit gates that catch semantic and style mismatches before
runtime integration. Examples from the failed prototype: an armor icon reading
as a castle, mixed icon styles in one set, composite props that fuse unrelated
objects, and UI crops that technically pass but look noisy in the screen.

## Done when

- [x] A reusable gate/checklist exists for semantic fidelity and one-style-per-set
      icon families.
- [x] The gate names where it runs in the generated asset pipeline.
- [x] At least one existing noisy Voxelheim asset example is documented as a
      test case or rejection example.

## Open questions

## Log
- 2026-06-17 implemented `tools/assets/audit_asset_semantic_style.mjs`,
  documented the reusable gate in
  `gamedesign/knowledge/asset_semantic_style_gate.md`, and added Voxelheim
  rejected examples for the coin-like Frost Blocks icon and castle-as-armor
  semantic failure.
