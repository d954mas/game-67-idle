---
id: T0203
title: "Canvas feedback layer: toasts replace status bar, busy spinner, task limiter, export as download"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Replace the bottom status line (lead: looks strange) with a Figma-like feedback layer: transient toasts (success auto-hides ~3s, errors persist until dismissed, export/render results as a pinned toast with download links), a busy spinner on long ops, input NOT blocked during them, and a task limiter (max N concurrent long ops; extra requests queue with visible state). Export destination scope MOVED to T0206 (2026-07-02): the Figma-style inspector export panel owns asking-where (dir picker, remembered folder, zip/download fallback, CLI --to).

## Done when

- [ ] no permanent status bar; results/errors arrive as toasts on both home and workspace views
- [ ] detect/slice/render/export show a spinner, canvas stays interactive, triggering control disabled while in flight
- [ ] more than N (default 2) long ops queue visibly instead of spawning concurrent python runs

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-02: Export-destination scope moved to T0206 (Figma-style inspector export panel).
