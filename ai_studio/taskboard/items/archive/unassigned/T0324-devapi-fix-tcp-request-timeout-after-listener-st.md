---
id: T0324
title: "DevAPI fix: TCP request timeout after listener startup + synthetic click slot bug"
status: done
project: P001
epic: ""
priority: P1
tags: [devapi, runtime-automation, engine, vibejam-retro]
created: 2026-07-06
updated: 2026-07-06
---

## What

Lead directive 2026-07-06: "разбираемся и фиксим девапи". Two distinct bugs
from VibeJam evidence:

1. TCP request timeout after listener startup — hit 3x in one evening session
   (019f32a3, 2026-07-05 19:16-21:03): listener starts, requests hang. Blocked
   the visual gate for T0311/T0313/T0315 (shipped without visual proof).
2. Synthetic click slot bug — night diagnosis (Averroes packet 04:59 07-05):
   synthetic pointer lands in the next free slot, UI never sees it. Blocked the
   WORLD night track; never fixed.

DevAPI is the single automatic runtime-verification path; when it breaks, all
verification falls back on the lead. Fix must land engine-side (nt_devapi_net /
input injection) so template + all future games get it.

## Done when

- [x] Root cause of TCP timeout identified (no transport change needed: no
      devapi thread + shared fixed port 17890 + exclusive bind = probes hitting
      wrong/dead instances). Dominant trigger FIXED pipeline-side:
      devapi_client.py auto-picks a free ephemeral port per launch (env/arg
      still win) + preflight raises dead-child error with exit code and log
      tail instead of a silent 5s timeout. 34 module tests pass (7 new).
- [x] Synthetic click: root cause = ungated injection (engine player-gate
      already clears pointer slots on disable and drops real-device events —
      properly gated automation cannot hit the bug). Fixed pipeline-side:
      DevApiClient.player_gated() context manager (input.set_player_enabled,
      finally-safe re-enable) + rule in nt-runtime-automation skill + template
      ui_runtime feeds ALL pointer slots (both builds green). 36 module tests
      pass (2 new). Engine untouched by lead decision: backlog dropped
      (client retries cover instant refusals), mouse-singleton parked
      (tmp/engine_issue_drafts_2026-07-06.md).
- [x] Regression: 100x sequential requests vs fresh template devapi-debug game
      — 100/100 OK, avg 32ms, max 63ms, no hangs (auto port 59509 confirmed in
      launch log). smoke_bot end-to-end green with the new launcher.

## Open questions

## Log

- 2026-07-06: created from retro walkthrough plan item 1; background diagnosis
  packet launched same day.
- 2026-07-06 DIAGNOSIS (deep-reasoner, static, evidence-backed):
  BUG1 root cause — DevAPI has no thread; TCP served only by per-frame
  nt_devapi_update() poll. connect() succeeds the moment listen() is up (OS
  backlog), so a starved/blocked frame loop or a WRONG instance holding the
  fixed shared port 17890 (SO_EXCLUSIVEADDRUSE: second instance bind-fails and
  EXITS) yields "connected but no response in 5s". Backlog=1 + lazy client
  reaping aggravate. Matches taskkill workaround observed in jam logs.
  Fixes ranked: (1) unique port per session via AI_STUDIO_DEVAPI_PORT (already
  honored by devapi_client.py:22) — pipeline, no engine change; (2) engine:
  listen backlog 1 -> 16 (nt_devapi_net.c:252); (3) launcher preflight: detect
  child bind-fail exit before probing; (4) transport thread REJECTED (engine
  single-thread philosophy; unnecessary).
  BUG2 root cause — injected click uses reserved id 0x10000000; slot resolved
  as FIRST FREE (nt_input.c:309-320). Real mouse (id 0) holds slot 0 when the
  cursor touched the window -> injected click lands in slot 1. nt_ui reads all
  slots (works); game scene paths read pointers[0] only
  (sys_scene_interactions.c:41, sys_scene_pan.c:42) -> clicks invisible.
  Fix options: A) game/template-side all-slots primary-pointer helper;
  B) engine: NT_POINTER_MOUSE resolves to existing mouse slot (singleton) in
  pointer apply fns — durable, low test risk (recommended for template-first);
  C) inject with id 0 — REJECTED (breaks test_devapi_input asserts).
  Awaiting lead go/no-go on implementing (1)+(2)+(3)+(B).
- 2026-07-06: 2026-07-06: lead go on fix package; engine packet (backlog+mouse-singleton+tests) and pipeline packet (auto free port + launch preflight) dispatched
- 2026-07-06: 2026-07-06: LEAD RULE - no direct engine edits; convince -> issue -> PR. Engine packet stopped before any changes (verified: engine tree clean except unrelated builder CMake). Pipeline packet (port+preflight) continues. No-engine fallback for click bug: template-side all-slots pointer helper (Option A).
- 2026-07-06: 2026-07-06: closed - port+preflight (100x loop green), player_gated wrapper + skill rule + template all-slots; engine untouched (lead: root-cause rule, backlog dropped, singleton parked)
