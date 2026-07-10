---
id: T0202
title: "Canvas perf: warm Python worker (JSON-RPC stdio) for raster2d bridge"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-10
---

## What

Kill the 165-278ms Python cold-spawn floor under every detect/slice/render (bench + research: numpy+PIL import is ~50-60% of each call). Implement a persistent Python worker (JSON-RPC over stdio) behind the raster2d bridge runPython so BOTH clients (site + agent CLI) get faster with no interface change: lazy spawn, single queue, respawn + retry-once on crash, idle-kill after timeout, fallback to per-call spawn on worker failure. Coordinate with the lead's in-flight matte work in ai_studio/assets/tools/raster2d before touching the bridge.

## Done when

- [x] second and later detect/slice/render calls skip interpreter+import cost (measured on this box: renderGroup 103ms cold → ~7-8ms warm; detectRegions 429ms cold → ~19ms warm; both medians far under 300ms)
- [x] worker crash mid-call degrades gracefully — AMENDED to the no-silent-fallbacks law (lead 2026-07-02): the crashed op FAILS LOUDLY (no retry-once, no per-call cold-spawn fallback) and a fresh worker is spawned to serve the NEXT request; queued-but-never-started ops re-dispatch on the fresh worker (first run, not a retry). Crash carries the worker stderr tail.
- [x] no zombie python processes after server shutdown (verified on Windows: process-exit + SIGINT/SIGTERM hook kills the child; a one-shot host exits on its own via idle-unref; `tasklist` showed 0 orphan python.exe after runs)
- [x] canvas + raster2d tests green (225 canvas tests + the touched image-tools .mjs suites, all green)

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-03: BUILT. Warm worker landed in the shared image-tools `_bridge` (its honest home — every image-tool + canvas Python spawn already funnels through `_bridge/bridge.mjs` runPython).
  - Design: a GENERIC script-runner worker, not per-tool RPC methods. `_bridge/worker.py` is a long-lived process speaking line-delimited JSON over stdio; each request is `{id, script, argv}` and it runs that script's `__main__` via `runpy.run_path(run_name="__main__")` with a synthesized `sys.argv`, capturing the script's stdout/stderr/SystemExit into the JSON response. The heavy stack (numpy/scipy/PIL) stays cached in `sys.modules` across calls; only the small target script is re-parsed each call. This keeps EXACT parity (same argv/argparse main the cold `python script.py` path ran) and serves ALL entrypoints with one implementation — no coupling of `_bridge` to canvas tool semantics.
  - Transport swap: `_bridge/bridge.mjs` `runPython(root, args)` keeps its signature but now resolves the pinned venv interpreter (`resolvePythonPath`, T0218 config-only — reused, no discovery) and dispatches to the worker manager. This upgraded EVERY caller with zero call-site churn: image-tools regions detect (normalize_background.py + detect_regions.py), the slice/matte tools, and canvas export_images.py. ops.mjs's own module-local candidate-discovery `runPython`/`pythonCandidates` (used by crop_regions.py + render_group.py) were DELETED; those two now call the same bridge runner (`runToolPython`). All canvas Python is now one warm path on the pinned venv.
  - Converted spawn sites (all of them): detectRegions (2 spawns/call), sliceRegions (crop_regions.py), exportElements (export_images.py), renderGroup + exportProject (render_group.py per screen). None left cold.
  - Manager (`_bridge/worker.mjs`): lazy spawn per interpreter; FIFO queue, one op in flight (the page long-op queue caps concurrency at 2 → they serialize on the single worker, second waits, no deadlock). Idle-timeout kill (5 min; `AI_STUDIO_IMAGE_WORKER_IDLE_MS` override for tests). Crash/external-kill/failed-spawn → the in-flight op rejects LOUDLY with the worker stderr tail, then a fresh worker respawns for the queue (no silent retry; no cold-spawn fallback). Lifecycle: child + stdio unref'd while idle so a one-shot host (CLI/tests) still exits; `process.once('exit')` + SIGINT/SIGTERM/SIGHUP hook kills the child so nothing is orphaned on Windows.
  - Proof: new `_bridge/worker.test.mjs` (warm<cold, loud script failure keeps worker alive, crash-mid-request loud-reject + respawn, FIFO, idle-kill) and `canvas/tests/worker_warm.test.mjs` (renderGroup + detectRegions warm<cold + parity). Gates green: canvas `node --test` 225 pass / 0 skip; arch-map `--strict` 0 unmapped/0 missing; doc_reference_check ok. Store/journal/page untouched (pure transport). No task-status change.
- 2026-07-11: T0375 status reconciliation: done; all 4 acceptance criteria are checked and the card log contains worker/protocol test evidence.
