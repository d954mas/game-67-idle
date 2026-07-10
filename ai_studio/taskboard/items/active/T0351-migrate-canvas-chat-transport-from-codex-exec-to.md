---
id: T0351
title: Migrate Canvas Chat transport from codex exec to app-server
status: backlog
project: P001
epic: E015
priority: P0
tags: [canvas, chat, app-server]
created: 2026-07-10
updated: 2026-07-10
---

## What

After `T0350`, migrate Canvas Chat from spawning/resuming `codex exec` to the
subscription-authenticated Codex app-server mode while preserving session,
streaming, cancel, clear-context, Canvas op, and journal behavior.

## Done when

- [ ] App-server starts through the existing subscription authentication path;
      no separate API billing/key requirement is introduced.
- [ ] Canvas owns the chat/app-server adapter; Studio Shell only hosts HTTP.
- [ ] Session continuity, streaming progress, cancel, clear context, errors, and
      operation-range links have parity tests against the current behavior.
- [ ] The permission decision from `T0350` gates every mutating request before
      it reaches app-server tooling.
- [ ] Startup, shutdown, restart, stale-session, and unavailable-server paths
      fail loudly and leave no orphaned operation state.
- [ ] The old `codex exec` production path and obsolete docs are removed after
      parity; an injectable test seam may remain.

## Open questions

- This task changes transport behind the current stable boundary. `T0352` alone
  owns later physical file moves/decomposition so the refactor is not repeated.

## Log

- 2026-07-10: Ordered strictly after `T0350` by lead decision.
