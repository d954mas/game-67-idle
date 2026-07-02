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

Replace the bottom status line (lead: looks strange) with a Figma-like feedback layer: transient toasts (success auto-hides ~3s, errors persist until dismissed, export/render results as a pinned toast with download links), a busy spinner on long ops, input NOT blocked during them, and a task limiter (max N concurrent long ops; extra requests queue with visible state). Export downloads files (download attr / zip) instead of opening tabs. Same op layer untouched - page-only feedback plumbing.

## Done when

- [ ] no permanent status bar; results/errors arrive as toasts on both home and workspace views
- [ ] detect/slice/render/export show a spinner, canvas stays interactive, triggering control disabled while in flight
- [ ] more than N (default 2) long ops queue visibly instead of spawning concurrent python runs
- [ ] export delivers files as downloads; multi-file export offers one-click download-all

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
