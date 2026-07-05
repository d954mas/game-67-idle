---
id: T0263
title: "Track B v1 pipeline: staged video tool generate->matte->frames->sheet in isolated folder, canvas seam LAST (gated on R2 verdict + speedup)"
status: review
project: P001
epic: E010
priority: P1
tags: [video, pipeline]
created: 2026-07-03
updated: 2026-07-03
---

## What

Build the isolated Track B video pipeline with staged generate, matte, frames,
and sheet tools, keeping the Canvas integration seam last.

## Done when

- [x] Staged video tool folders and orchestrator are landed.
- [x] A golden wings run proves generate->matte->frames->sheet output.
- [ ] Canvas seam is approved, implemented, or split into a separate task.

## Open questions

## Log
- 2026-07-03: v1 LANDED 170569af (my review + suites: sheet 9/9, canvas 560, chat 51): staged pipeline ai_studio/assets/tools/video/{generate,frames,matte,sheet} + run.mjs orchestrator + videoGenRoot config. Live golden run wings draft seed 70263: generate 79.2s cold / frames 0.6s / CorridorKey matte 42.7s (glow preserved 12.8% soft alpha) / sheet 0.3s -> 1920x1920 5x5 25f flipbook + meta v1. Known: CorridorKey forced eager (triton replay errors on this box), per-frame flicker caveat, sparkle blobs = WAN content not alpha bug. Canvas seam = next increment, needs lead.
