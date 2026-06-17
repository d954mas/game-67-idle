---
id: T0015
title: Promote runtime UI edge audit to reusable tooling
status: done
epic: E001
priority: P2
tags: [pipeline, assets, ui, edge-audit, chroma, reusable]
created: 2026-06-17
updated: 2026-06-17
---

## What

Promote the temporary Voxelheim CTA purple/chroma audit into reusable tooling
for future generated UI assets. The reusable tool should audit source PNGs and
runtime screenshot crops for key-color/purple/chroma fringe on buttons, panels,
icons, and other UI sprites.

## Done when

- [x] Reusable tool exists under `tools/assets/` or `tools/product_gate/`.
- [x] It supports source PNG and screenshot crop auditing.
- [x] It reports counts, sample coordinates, and pass/fail thresholds.
- [x] It is documented from the live-state matrix/source-to-runtime edge audit.
- [x] Tests or fixture command cover a clean and failing case.

## Open questions

- Should this be a generic key-color/fringe detector or a configurable
  color-family detector first?

## Log
- 2026-06-17 created after Voxelheim's temporary `tmp/audit_cta_purple.py`
  proved the failure mode.
- 2026-06-17 implemented `tools/assets/audit_runtime_ui_edges.py` with unit
  tests, documentation, source PNG audit, and runtime screenshot crop audit.
