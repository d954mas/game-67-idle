---
id: T0234
title: "Canvas: history-jump/undo concurrency guard - refuse when head advanced since read"
status: doing
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Incident 2026-07-03: agent read Demo journal at head 823, lead kept working
live to head 876; agent's history-jump+ops forked at 817 and orphaned the
lead's entries 868-876 (recovered manually from sidecar snapshots). Guard:
agent-initiated history navigation must prove it read the CURRENT head.

Design (settled by orchestrator):
- ops jumpHistory/undoOp/redoOp accept optional `expectHead` (number). When
  provided and the journal head seq differs, refuse LOUDLY: error names the
  current head, the caller's stale value, and the remedy (re-read history).
  Nothing is written on refusal.
- CLI (actor=agent): `history-jump`, `undo`, `redo` REQUIRE `--expect-head N`
  — running without it is a loud error explaining why (live-project race) and
  where to get N (`history-list` must show the current head prominently).
- HTTP API passes optional `expectHead` through. The PAGE does not send it in
  v1 (its undo spam stays untouched); two-tab safety = follow-up if wanted.
- nt-canvas-operations skill doc updated: agent workflow = history-list →
  act with --expect-head.

## Done when

- [ ] jumpHistory/undoOp/redoOp with stale expectHead refuse loudly, journal
      untouched; with matching expectHead behave exactly as before.
- [ ] Agent CLI requires --expect-head on history-jump/undo/redo;
      history-list prints the current head. Page/API-without-param unchanged.
- [ ] Tests: drift refusal (advance head between read and jump), match path,
      CLI missing-flag loud error, page path unchanged. Full suite green.
- [ ] canvas README + .codex/skills/nt-canvas-operations/SKILL.md document
      the workflow.

## Open questions

- Page sending its own expectHead (two-tab safety) — deferred, lead to decide.

## Log

- 2026-07-03: created after the history-jump race incident (P1).
- 2026-07-03: design settled by orchestrator; delegated implementation to
  fast-worker (Sonnet).
