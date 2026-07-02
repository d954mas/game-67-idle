---
id: T0200
title: "Canvas perf: frontend churn — use op responses, file cache headers, img reuse, rAF drag, batched multi-ops"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Cut the per-op frontend churn found by the perf research: mutating actions re-fetch the whole project PLUS /history and rebuild every layer thumbnail on each op. Apply fixes A+B+G+H: (A) consume the op's returned {project} and fold canUndo/canRedo into op responses instead of reloadProject's two extra GETs; (B) Cache-Control immutable + ETag on the content-addressed /files route and reuse layer <img> nodes; (G) guard canvas backing-store realloc (only on real size change) + rAF-coalesce drag renders; (H) batch multi-select move/delete into one op call = one journal entry.

## Done when

- [ ] a mutating op (move/delete/rename) triggers zero extra GET requests (op response drives the re-render)
- [ ] layer thumbnails are not re-downloaded on unrelated ops (verified via server request log)
- [ ] drag renders coalesced via requestAnimationFrame; no backing-store realloc unless stage size changed
- [ ] multi-select delete/move of N elements = 1 HTTP call + 1 journal entry, undone by a single Ctrl+Z
- [ ] canvas tests green; bench delta recorded in the task log

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-02: Undo audit: marquee-move and multi-delete journal N entries per gesture (undo steps one element at a time) - batched ops (patchElements/removeElements) land here.
