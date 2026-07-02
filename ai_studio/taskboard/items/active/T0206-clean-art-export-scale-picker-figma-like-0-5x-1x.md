---
id: T0206
title: "Clean art: export scale picker (Figma-like 0.5x/1x/2x) + resample filter choice"
status: backlog
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Figma-like export sizing (the lead's anti-noise supersampling lands here): export dialog + op parameter for scale (0.5x/1x/2x/custom width) with resample filter choice (auto: Lanczos for smooth art, nearest for pixel art; user-overridable). Non-destructive by definition - originals in files/ are never touched; scale=1 keeps the current pure-copy path, scaled exports go through PIL. CLI parity: export --scale 0.5 --filter lanczos.

## Done when

- [ ] export UI offers scale + filter; exported PNGs match requested dimensions
- [ ] generate-big -> export-small workflow verified to visibly reduce AI noise on a real generated sheet (before/after saved to the task log)
- [ ] scale=1 path unchanged (byte-identical copies); pixel tests for nearest vs lanczos at 0.5x
- [ ] render-screen export honors the same scale/filter options

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
