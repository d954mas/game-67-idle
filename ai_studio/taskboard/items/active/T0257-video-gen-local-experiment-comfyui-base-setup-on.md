---
id: T0257
title: "Video-gen local experiment: ComfyUI base setup on RTX 4080 + model pull + R1/R2/R3 verification (WAN I2V, matting, wings fixture)"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Phase 1 DONE: ComfyUI portable v0.27.0 in C:\projects\video_gen_experiment (cu126 variant - cu130 exceeds driver 12.9 ceiling, documented in README). CUDA smoke passed: cuda:0 RTX 4080 12GB, /system_stats ok, server killed clean. ~5.7GB disk. Phase 2 next: models per community report.
- 2026-07-03: Phase 2 DONE: WAN 2.2 I2V Q4_K_S high+low, umt5 Q4, VAE, Lightning LoRAs = 23.86GB (Apache-2.0, byte-verified); ComfyUI-GGUF pinned 6ea2651e. Smoke: cold 218s / warm 103s, 33f 480x480 16fps, real motion, VRAM ~3.8GB free. -s flag site-packages trap found+documented. FIRST-HAND R1 EVIDENCE: flat test shape hallucinated into photoreal creature - style drift is real. Phase 3 (wings R1/R2/R3) launched.
- 2026-07-03: Phase 3 DONE, verdict CONDITIONAL GO (report tmp/t0257_phase3_report.md): R1 style-drift PASS on real wings (prompt-hardening '2d game art, flat colors, no photorealism' = +0.7-0.8 codex-judge points, 4.17/5); R3 iteration PASS (3 edits, all first-try, ~126s each); R2 matting BLOCKED on permission to build cloned CorridorKey/MatAnyone (CorridorKey cloned+waiting at video_gen_experiment/tools/CorridorKey, lead can run Install_CorridorKey_Windows.bat). WAN green stays clean (drift 11/441). Recommendation: build generate->frames->sheet now, hold matte stage until R2 runs; key_matte baseline shows documented glow fringe.
