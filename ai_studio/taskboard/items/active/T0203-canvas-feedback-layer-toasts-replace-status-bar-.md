---
id: T0203
title: "Canvas feedback layer: toasts replace status bar, busy spinner, task limiter, export as download"
status: review
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

- [x] no permanent status bar; results/errors arrive as toasts on both home and workspace views
- [x] detect/slice/render/export show a spinner, canvas stays interactive, triggering control disabled while in flight
- [x] more than N (default 2) long ops queue visibly instead of spawning concurrent python runs

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-02: Export-destination scope moved to T0206 (Figma-style inspector export panel).
- 2026-07-02: BUILT (deep-reasoner) + ACCEPTED, commit 4732ab1c. site/toasts.js (kinds success/info/error/pinned/progress; bottom-right at right:288px past the inspector so Export button never covered; success/info auto-hide 3s hover-pause; errors persist w/ op name; pinned = export/render results w/ download links; cap 5 evicting oldest TRANSIENT so errors/results survive; container pointer-events:none) + site/long_op_queue.mjs (PURE FIFO limiter, tested in node: max 2 concurrent, visible queue position shifts, cancel-queued via toast x, running op shows no x - no server cancellation to honor, failure frees slot, reload clears). Status bar deleted from both views; setStatus/setStatusLinks kept as SHIMS into toasts (~35 call sites untouched - shim over rip-out); undo/redo = NO toast by design (high-frequency noise; failures still error-toast; region-clamp info kept). 5 python-backed actions (detect/slice/exportElements/exportProject/renderScreen) run through runLongOp(label, fn, {control}): progress toast w/ spinner resolves in place, triggering button disabled, canvas interactive. Pure page layer - NO ops/api/cli change, NO server restart needed. Known edges logged: export queued >~seconds behind 2 running ops can expire browser user-activation for the dir picker -> honest error toast (redesign out of scope); "+N more" collapse replaced by drop-oldest-transient. Tests 159->165 (long_op_queue.test.mjs 6). Gates re-run at acceptance: 165/165, map strict 299/405, docs ok.
