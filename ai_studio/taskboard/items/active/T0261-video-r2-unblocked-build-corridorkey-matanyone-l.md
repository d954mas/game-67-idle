---
id: T0261
title: "Video R2 unblocked: build CorridorKey + MatAnyone (lead approved), run glow-matting comparison vs key_matte; if unsuitable - research alternative video matting tools"
status: review
project: ""
epic: ""
priority: P1
tags: [video, matting]
created: 2026-07-03
updated: 2026-07-04
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: R2 DONE (report R2 section rewritten, evidence video_gen_experiment/phase3/r2/compare): CorridorKey PASS (only tool keeping soft gold glow 11.6% fractional-alpha AND despilling, edge-greenness -29.6; 2.5-4.1s/frame GPU); MatAnyone PARTIAL (best temporal stability but green fringe +78.5, no unmixing; 0.4s/frame); key_matte FAIL on glow (4.8%, muddy fringe) but stays for opaque sprites. +13GB disk. LICENSES: CorridorKey CC-BY-NC-SA w/ asset-processing carve-out; MatAnyone S-Lab NON-commercial - flag to lead. Recommendation: CorridorKey = Track B matte stage for glow/soft assets.
- 2026-07-04: STATIC eval done (tmp/research_corridorkey_static_2026-07-05.md, evidence video_gen_experiment/static_eval): CorridorKey wins ONLY glow-on-green (7.6% vs 4.9% soft, smooth feather); LOSES magenta (architecturally green/blue checkpoints only - no magenta model; pink rim, spill 299px) and neutral (no cutout; dual_plate territory); key_matte stays default crisp+magenta path. Latency: 13-16s cold per single image (model load) vs 0.02-0.24s key_matte. Integration sketch: explicit 'corridorkey' method, green-only, loud-reject magenta, NOT in auto router, matte-stage subprocess not warm worker. AWAITING LEAD: wire it or keep CK video-only.
- 2026-07-04: Magenta research DONE (tmp/research_corridorkey_magenta_2026-07-05.md): community = nothing (green/blue baked in checkpoint WEIGHTS, --screen-color only picks ckpt+despill channel); BUT hue+180 preprocessing shim cleanly fools it (magenta->green in, FG rotated back out, alpha untouched) - strict upgrade over blue-on-magenta: subject dE 2.7->2.0, rim contam ->0%. invert kills dark subjects; swap R<->G gives cyan = reject. key_matte still beats CK outright on FLAT magenta (dE 0, 200x faster). Amendment sent to wiring worker: magenta = auto hue180 shim (provenance shim:'hue180'), neutral still loud-rejects; worker may defer to follow-up if mid-verification.
