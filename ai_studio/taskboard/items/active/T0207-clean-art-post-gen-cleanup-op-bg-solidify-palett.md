---
id: T0207
title: "Clean art: post-gen cleanup op — bg solidify, palette quantize, denoise (non-destructive)"
status: backlog
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Deterministic post-generation cleanup op (Python, ours) for noisy AI art - lead: art must never be broken. Three composable passes: background-solidify (snap pixels within a color distance of the detected/chosen bg color exactly to it - also a keyer pre-pass), palette quantization (k-means/median-cut to N colors), and optional edge-preserving denoise (median/bilateral 3x3). NON-DESTRUCTIVE: result is a NEW immutable file + journaled element.src swap (Ctrl+Z restores), original stays in files/. Emits a before/after report (changed-pixel %, palette size). Per-element and per-region application. UI + CLI parity. DEPENDS ON: lead's unified matte work landing in raster2d/cutout (bg-solidify must share its color-distance math).

## Done when

- [ ] cleanup op produces a new file + journaled swap; undo restores the original pixel-for-pixel
- [ ] bg-solidify makes the background a single exact color on a real noisy generated sheet (matte cuts it cleanly with default thresholds)
- [ ] each pass has pixel tests on fixtures proving it only touches what it should (bg pass never alters foreground pixels beyond the distance threshold)
- [ ] before/after report saved with each run; UI shows it; CLI prints it

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
