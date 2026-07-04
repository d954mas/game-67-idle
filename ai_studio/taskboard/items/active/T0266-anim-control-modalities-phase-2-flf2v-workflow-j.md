---
id: T0266
title: "Anim control modalities phase 2: FLF2V workflow JSONs (same-frame loop + pose A->B), per-character LoRA experiment (AI-Toolkit 12GB), ToonCrafter pilot vs FLF"
status: backlog
project: ""
epic: ""
priority: P2
tags: [video, animation, research]
created: 2026-07-04
updated: 2026-07-04
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-04: Lead ask: MEASURE LoRA training on this box + speedup ladder: draft profile (rank 16, 384px, fewer steps/images, ~30-60min?) vs final (rank 32, 512px, ~3h) - deliverable answers 'does a draft LoRA predict the final one' (identity-lock validation level, not per-frame: training has no shared-seed ladder like generation). Levers to bench: rank, resolution, steps, dataset size, latent caching.
