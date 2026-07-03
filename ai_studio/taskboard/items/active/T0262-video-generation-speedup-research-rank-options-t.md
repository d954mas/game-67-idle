---
id: T0262
title: "Video generation speedup: research + rank options (TeaCache/Sage/steps/resolution ladder/faster engines), then apply best to WAN stack"
status: review
project: ""
epic: ""
priority: P1
tags: [video, perf]
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Research DONE (tmp/research_T0262_speedup_2026-07-05.md). Key: TeaCache/MagCache WRONG TOOL on already-distilled 4-8 step stack (assumptions break, motion stutter); SageAttention is the one worthwhile accelerator (1.15-1.4x). Package 1 ZERO-INSTALL draft profile: 384px + 25 frames + 4 total steps + batch seeds -> projected warm 35-57s (vs 103s). Package 2: + SageAttention on a COPY of portable -> 30-50s. Draft->final ladder = SAME engine locked seed (cross-engine draft defeats prediction). APPLY step queued until T0261 frees the experiment folder/GPU; must first confirm actual current step count (4 vs 8).
- 2026-07-03: APPLY DONE: baseline was ALREADY 4 steps (research q answered); honest re-baseline: real multi-run warm 480/33 = ~54s (103s was shallow-warm w/ partial disk read). Draft 384/25 = ~35.5s steady (x2 seeds), final 54s, peak VRAM 10.4/12.3GB. Draft quality ACCEPT (flap readable, no photoreal collapse); ladder correspondence STRONG (same seed draft->final, codex-vision judged). Edit loop: ~35s/motion-try, explore 4 seeds + ship one ~3min. Finding: high/low split timing-NEUTRAL at 4 steps (swap happens once regardless) - quality knob not speed knob. Files: draft/final_workflow_api.json + README profiles section.
