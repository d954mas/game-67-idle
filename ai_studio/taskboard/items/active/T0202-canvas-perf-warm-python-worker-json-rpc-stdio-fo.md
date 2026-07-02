---
id: T0202
title: "Canvas perf: warm Python worker (JSON-RPC stdio) for raster2d bridge"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Kill the 165-278ms Python cold-spawn floor under every detect/slice/render (bench + research: numpy+PIL import is ~50-60% of each call). Implement a persistent Python worker (JSON-RPC over stdio) behind the raster2d bridge runPython so BOTH clients (site + agent CLI) get faster with no interface change: lazy spawn, single queue, respawn + retry-once on crash, idle-kill after timeout, fallback to per-call spawn on worker failure. Coordinate with the lead's in-flight matte work in ai_studio/assets/tools/raster2d before touching the bridge.

## Done when

- [ ] second and later detect/slice/render calls skip interpreter+import cost (bench: detect median under ~300ms; slice under ~300ms after inc6's single-spawn crop)
- [ ] worker crash mid-call degrades gracefully (retry once, then per-call spawn) with a logged error
- [ ] no zombie python processes after server shutdown (verified on Windows)
- [ ] canvas + raster2d tests green

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
