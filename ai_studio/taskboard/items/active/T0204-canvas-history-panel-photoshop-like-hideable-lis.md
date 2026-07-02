---
id: T0204
title: "Canvas history panel: Photoshop-like hideable list + jump-to-step op"
status: backlog
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Photoshop-style history panel over the existing journal: a hideable panel listing operations (icon + label + time), current position marked; clicking an entry jumps the project to that step (new jumpHistory op applying undo/redo steps to reach the target seq - CLI parity: history-jump <id> --seq N). Grayed redo-tail entries stay clickable. Depends on T0201 compaction semantics for what is listed.

## Done when

- [ ] panel toggles from the toolbar (and a hotkey), hidden by default, remembers its state
- [ ] clicking any listed step restores that project state; canvas/layers/inspector re-render consistently
- [ ] CLI history-jump reaches the same states as the panel
- [ ] journal tests extended for jumpHistory (incl. jumping into the redo tail)

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
