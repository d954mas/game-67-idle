---
id: T0201
title: "Canvas perf: journal O(n^2) — sidecar snapshots, O(1) seq, tool_runs cap, history depth limit"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Fix the journal O(n^2) growth confirmed by research (append 1ms->108ms, readHistory 0.7ms->109ms at 1000 ops): every op currently writes two full-project snapshots into journal.jsonl and re-parses the whole file for max seq. Apply C+D+I: (C) move snapshots to sidecar snapshots/<seq>.json keeping journal lines tiny; (D) O(1) seq via tail-read/persisted counter; (I) cap tool_runs (sidecar or limit). Add a history depth limit + compaction on project open (default ~200 ops, studio.config knob) - Photoshop caps steps, Figma checkpoints; we compact beyond the cap. Preserve history_seq undo/redo semantics (researcher's correctness note: appends stay physical-append-only).

## Done when

- [ ] journal line size is O(1) w.r.t. project size (snapshots in sidecar files)
- [ ] append + readHistory stay <5ms at 1000+ ops on a 100-element project (bench re-run recorded)
- [ ] history compaction keeps the last N ops (configurable), runs on project open, and undo/redo still pass all journal tests
- [ ] existing projects with fat journals migrate transparently on first open

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
