---
id: T0061
title: Dedup python edge and chroma mask formulas into chroma_key_alpha
status: done
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

- [x] Shared numpy mask helpers (key_fringe, purple single-clause, purple 4-clause, green-screen spill, source_key_spill) now live once in `chroma_key_alpha.py`; audit + intake call them; edge-proof reuses them transitively via the kept `*_mask_array` wrappers. ~60 dup LOC removed.
- [x] The magenta tolerance (36) is parameterized (`magenta_tolerance`); the other thresholds were byte-identical across sites (no per-site value to thread). Scan algorithms NOT merged (intake = connected-component labeling; audit/edge-proof = dilation). The subtle 1-clause (intake) vs 4-clause (audit/edge-proof) purple difference was preserved as two helpers, not forced together.
- [x] Verified: targeted python unittests 58/58, broader asset suite 54/54, quick `pipeline_validate` 20/20; an equivalence harness confirmed every new helper is byte-identical to the original inline formula.

## Open questions

## Log

- 2026-06-15: Captured from the second simplification/speed iteration. Modest (~60-90 LOC), low risk but the masks are behaviorally load-bearing -- verify with tests.
- 2026-06-15: Promoted the duplicated numpy mask formulas into chroma_key_alpha.py (parameterized magenta tolerance; kept scans + the 1-vs-4-clause purple distinction separate). Independently verified: 58/58 targeted python tests OK, quick validate ok, equivalence harness byte-identical.
