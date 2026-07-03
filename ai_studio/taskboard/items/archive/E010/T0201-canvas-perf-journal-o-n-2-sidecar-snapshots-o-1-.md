---
id: T0201
title: "Canvas perf: journal O(n^2) — sidecar snapshots, O(1) seq, tool_runs cap, history depth limit"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-03
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
- 2026-07-03: Read-only audit launched: verify which Done-when items already shipped (capToolRuns, canvasHistoryDepth, sidecar snapshots observed in code) vs what remains; verdict per item + remaining packets or close recommendation.
- 2026-07-03: Audit 2026-07-04: ALL 4 done-when items DONE and shipped long since - sidecar snapshots (commitMutation+writeSnapshot, thin journal 48KB vs 6.3MB legacy .bak), O(1) seq (lastJournalSeq 64KB tail read), capToolRuns(50)+spill at 16 sites, canvasHistoryDepth(200) compaction post-mutation (stronger than 'on open'), migration ensureThinJournal idempotent w/ .bak. Bench recorded tmp/canvas_bench_2026-07-02.json: append@1000=2.74ms, readHistory=0.79ms, undo=2.2ms - all under the 5ms target. 25 journal/sidecar/config tests green. Stale backlog card - closing.
