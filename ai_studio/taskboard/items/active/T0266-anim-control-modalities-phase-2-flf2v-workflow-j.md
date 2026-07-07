---
id: T0266
title: "Anim control modalities phase 2: FLF2V workflow JSONs (same-frame loop + pose A->B), per-character LoRA experiment (AI-Toolkit 12GB), ToonCrafter pilot vs FLF"
status: backlog
project: P001
epic: E010
priority: P2
tags: [video, animation, research]
created: 2026-07-04
updated: 2026-07-06
---

## What

Research phase 2 animation-control modalities: FLF2V workflow JSONs, per-character
LoRA training feasibility, and ToonCrafter versus FLF tradeoffs.

## Done when

- [ ] FLF2V loop and pose A->B workflow options are tested or rejected.
- [ ] LoRA draft/final timing is measured on the local 12GB GPU.
- [ ] A recommendation identifies which modality should feed the Canvas video route.

## Open questions

## Log
- 2026-07-04: Lead ask: MEASURE LoRA training on this box + speedup ladder: draft profile (rank 16, 384px, fewer steps/images, ~30-60min?) vs final (rank 32, 512px, ~3h) - deliverable answers 'does a draft LoRA predict the final one' (identity-lock validation level, not per-frame: training has no shared-seed ladder like generation). Levers to bench: rank, resolution, steps, dataset size, latent caching.
- 2026-07-06: Заморожен вместе с видео-программой (см. T0265 лог 2026-07-06 и docs/FREEZE_VIDEO_ANIM_2026-07-06.md): FLF/LoRA-исследование возобновлять только после выбора нового движка (fal.ai или новая локальная модель).
