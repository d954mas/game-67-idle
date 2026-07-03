---
id: T0261
title: "Video R2 unblocked: build CorridorKey + MatAnyone (lead approved), run glow-matting comparison vs key_matte; if unsuitable - research alternative video matting tools"
status: review
project: ""
epic: ""
priority: P1
tags: [video, matting]
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: R2 DONE (report R2 section rewritten, evidence video_gen_experiment/phase3/r2/compare): CorridorKey PASS (only tool keeping soft gold glow 11.6% fractional-alpha AND despilling, edge-greenness -29.6; 2.5-4.1s/frame GPU); MatAnyone PARTIAL (best temporal stability but green fringe +78.5, no unmixing; 0.4s/frame); key_matte FAIL on glow (4.8%, muddy fringe) but stays for opaque sprites. +13GB disk. LICENSES: CorridorKey CC-BY-NC-SA w/ asset-processing carve-out; MatAnyone S-Lab NON-commercial - flag to lead. Recommendation: CorridorKey = Track B matte stage for glow/soft assets.
