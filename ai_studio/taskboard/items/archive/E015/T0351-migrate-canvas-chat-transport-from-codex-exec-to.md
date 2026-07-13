---
id: T0351
title: Migrate Canvas Chat transport from codex exec to app-server
status: done
project: P001
epic: E015
priority: P0
tags: [canvas, chat, app-server]
created: 2026-07-10
updated: 2026-07-11
---

## What

After `T0350`, migrate Canvas Chat from spawning/resuming `codex exec` to the
subscription-authenticated Codex app-server mode while preserving session,
streaming, cancel, clear-context, Canvas op, and journal behavior.

## Done when

- [x] App-server starts through the existing subscription authentication path;
      no separate API billing/key requirement is introduced.
- [x] Canvas owns the chat/app-server adapter; Studio Shell only hosts HTTP.
- [x] Session continuity, streaming progress, cancel, clear context, errors, and
      operation-range links have parity tests against the current behavior.
- [x] The permission decision from `T0350` gates every mutating request before
      it reaches app-server tooling.
- [x] Startup, shutdown, restart, stale-session, and unavailable-server paths
      fail loudly and leave no orphaned operation state.
- [x] The old `codex exec` production path and obsolete docs are removed after
      parity; an injectable test seam may remain.

## Open questions

None.
## Log

- 2026-07-10: Ordered strictly after `T0350` by lead decision.
- 2026-07-10: Dependency/ownership fixed: after T0350; T0352 alone owns later physical Canvas decomposition.
- 2026-07-11: Checkpoint: T0349 closed at 99265b13b. Starting Canvas-owned stdio JSONL app-server adapter from the official thread/turn protocol; subscription auth is reused through the installed Codex process. Studio Shell remains HTTP host, T0352 physical decomposition, T0393/E016, games/web-dressup, and external engine remain out of scope.
- 2026-07-11: TDD evidence: protocol tests were introduced before production wiring, then expanded across three review/fix cycles for exact subscription handshake, thread continuity, scoped approvals, concurrent turns, interrupt acknowledgement, timeout/EOF/EPIPE teardown, process-tree cleanup, shutdown propagation, deterministic CLI selection, bounded delta streaming, and active-clear refusal.
- 2026-07-11: Implementation: Canvas now owns a persistent newline-delimited app-server adapter using initialize/initialized, managed ChatGPT account verification, thread start/resume, turn start/completion, exact T0350 approval routing, scoped cancellation, and fail-loud restart lifecycle. Studio Shell only mounts HTTP and awaits bounded shutdown cleanup; the legacy codex-exec production path and obsolete tests/docs are removed.
- 2026-07-11: Verification: Chat and Studio Shell suites pass, Canvas regression passes 732 with 2 intentional skips, Architecture Map passes 22/22 with 352 mapped / 784 scanned and 0 issues, Taskboard validation reports 0 problems, syntax and scoped diff checks pass. Two live compatible Codex 0.144 subscription turns returned APP_SERVER_OK and APP_SERVER_OK_2 with real thread ids; incompatible npm Codex 0.140 reached authenticated turn/start and failed loudly on its model-version 400 without API-key fallback.
- 2026-07-11: Review convergence: three independent review/fix cycles closed concurrency, approval binding, interrupt, cleanup, streaming, resolver, ownership, EOF/EPIPE, and shutdown findings. Final architecture/correctness/ownership and tests/process/performance rechecks both report 0 HIGH and 0 actionable MEDIUM/LOW.
- 2026-07-11: Quality: QTECH_001=pass; evidence: Chat and Studio Shell regression, Canvas regression, live subscription app-server turns, lifecycle failure coverage, Architecture Map, Taskboard validation, and independent review convergence all pass
- 2026-07-11: Closed after three review/fix cycles converged at 0 HIGH and 0 actionable; all acceptance criteria, live subscription proof, lifecycle evidence, and QTECH_001 evidence are present.
- 2026-07-11: Final Windows live correction proof: a shutdown-order race exposed by observed `taskkill` cleanup was fixed without suppressing real cleanup failures; compatible Codex 0.144 then returned `APP_SERVER_CLEAN` and the awaited process-tree teardown returned `SHUTDOWN_CLEAN`. Both independent closure reviewers reconfirmed 0 HIGH and 0 actionable.
