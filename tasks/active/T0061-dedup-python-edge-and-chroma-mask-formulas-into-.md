---
id: T0061
title: Dedup python edge and chroma mask formulas into chroma_key_alpha
status: backlog
epic: E003
priority: P3
tags: [assets, subtraction, tooling]
created: 2026-06-15
updated: 2026-06-15
---

## What

Most asset-tool dedup is already done (`chroma_key_alpha.py` is the de-facto
shared module; edge-proof imports masks from `audit_generated_ui_assets.py`).
The remaining duplication is the numpy MASK FORMULAS existing 3-4x each:
`key_fringe` (`audit_generated_ui_assets.py:98-102`,
`audit_source_sheet_intake.py:462-468`, `chroma_key_alpha.py:22-23` + inline
:221), `purple_halo` (3 spots), and `source_key_spill_mask_array` (byte-identical
between `audit_generated_ui_assets.py:129-144` and `chroma_key_alpha.py:202-214`).

## Done when

- [ ] The shared numpy mask formulas live once in `chroma_key_alpha.py`; intake + audit + edge-proof import them.
- [ ] Tolerances/constants (8/10/24/36, differ by call site by design) stay PARAMETERIZED, not merged into one fixed value; the different scan algorithms (intake = connected-component labeling; audit/edge-proof = dilation) are NOT merged.
- [ ] All four modules' unittests (run by pipeline_validate :185,189) still pass; add a mask-equality assertion.

## Open questions

## Log

- 2026-06-15: Captured from the second simplification/speed iteration. Modest (~60-90 LOC), low risk but the masks are behaviorally load-bearing -- verify with tests.
