---
id: T0200
title: "Canvas perf: frontend churn — use op responses, file cache headers, img reuse, rAF drag, batched multi-ops"
status: review
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

- [x] a mutating op (move/delete/rename) triggers zero extra GET requests (op response drives the re-render)
- [x] layer thumbnails are not re-downloaded on unrelated ops (verified via server request log)
- [x] drag renders coalesced via requestAnimationFrame; no backing-store realloc unless stage size changed
- [x] multi-select delete/move of N elements = 1 HTTP call + 1 journal entry, undone by a single Ctrl+Z
- [x] canvas tests green; bench delta recorded in the task log

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-02: Undo audit: marquee-move and multi-delete journal N entries per gesture (undo steps one element at a time) - batched ops (patchElements/removeElements) land here.
- 2026-07-02: BUILT (deep-reasoner agent) + ACCEPTED, commit e37d8a5a. A: sendMutation folds history{seq,canUndo,canRedo} into every response carrying a project (shared historyFlags in ops.mjs, no duplication with readHistory); page applyMutation/ingestProject keeps the reconcileRegionEdit semantics; reloadProject kept only for genuine resync. B: /files route sends Cache-Control public,max-age=31536000,immutable + ETag(sha-name) + Last-Modified; layers thumbnails reuse <img> nodes keyed by element.id (id not src: content-addressed dedup can share one src across elements) with src guard + pruneThumbCache. G: resizeCanvas reallocs backing store only on real size/DPR change; requestRender() rAF-coalesces all drag-path renders (marquee stays on refresh - needs live layer selection, not a hotspot). H: patchElements/removeElements in store+ops (one commitMutation = one journal entry, atomic, empty batch no-op), HTTP elements-set/elements-remove, CLI parity (elements-set --json / elements-remove --elements); page multi-delete = one call, marquee drag commit = reparent-first then ONE elements-set so single Ctrl+Z undoes the move.
- 2026-07-02: MEASURED: journal entries per 8-element move gesture 8->1, delete 8->1; follow-up GETs per mutating op 2->0; HTTP calls per N-element gesture N->1; PATCH e2e 7.5ms (historyFlags read within noise); no metadata-path regression (patchElement 2.4-2.9ms, readHistory@1023 0.79ms). Gates re-run by lead-side acceptance: canvas 110/110 (10 new batched/API/CLI tests), map strict 296 mapped, docs ok. Deliberately NOT done: browser FPS harness (rAF verified by code, not faked numbers), bench.mjs batched row (honest deltas above), 304 short-circuit (immutable already prevents revalidation). Server :8780 restarted on the new backend.
