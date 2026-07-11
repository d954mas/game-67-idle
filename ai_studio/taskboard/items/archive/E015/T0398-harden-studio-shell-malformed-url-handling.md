---
id: T0398
title: Harden Studio Shell malformed URL handling
status: done
project: P001
epic: E015
priority: P1
tags: [studio-shell, http, correctness]
created: 2026-07-10
updated: 2026-07-11
---

## What

Make Studio Shell reject malformed URL escapes without terminating the local
server. Keep this as generic HTTP correctness, separate from Canvas Chat
permission and app-server transport work.

## Done when

- [x] URL/path decoding is guarded at the request boundary and malformed
      percent escapes return a deterministic `400` response.
- [x] The server remains alive and handles a valid request after malformed
      `/%`, truncated escapes, and invalid encoded-path fixtures.
- [x] Error output is concise, does not echo unsafe raw content, and cannot
      double-send or leave a hanging response.
- [x] Focused Studio Shell tests reproduce the current crash before the fix
      and prove survival afterwards.
- [x] No Canvas permission, routing, or transport policy is added here.

## Open questions

None.

## Log

- 2026-07-10: Full-file coverage found unguarded
  `decodeURIComponent(url.pathname)` in `studio_shell/server.mjs`.
- 2026-07-11: Checkpoint: confirmed unguarded Studio Shell request URL/path decoding; starting a focused RED survival test before the boundary fix.
- 2026-07-11: RED evidence: the real child-process server reset the first raw
  `/%` connection with `ECONNRESET` and terminated before it could serve the
  valid follow-up request.
- 2026-07-11: GREEN evidence: the focused raw-TCP integration test passes 1/1
  for `/%`, `/%A`, invalid UTF-8 `/%C0%AF`, valid encoded static `%2E`, literal
  `%25`, exact single `400 bad request` responses, and a final `200` from the
  same process. Server syntax check passes.
- 2026-07-11: Regression evidence: Chat API 21/21, Canvas API 30/30, Items
  Viewer 16/16, Taskboard validation, and cached diff check pass.
- 2026-07-11: Review convergence: cycle 1 independent architecture and
  tests/process reviews report 0 HIGH and 0 actionable findings; request
  parsing stays at the shell boundary and adds no Canvas permission, routing,
  or transport policy.
- 2026-07-11: Quality: QTECH_001=pass; evidence: reproduced crash, real-process
  survival test, 68 focused tests, downstream API regression suites, exact
  response framing, scoped diff validation, and two clean independent reviews.
- 2026-07-11: Closed after one clean review cycle: 0 HIGH, 0 actionable; real-process malformed-target survival proof passes.
