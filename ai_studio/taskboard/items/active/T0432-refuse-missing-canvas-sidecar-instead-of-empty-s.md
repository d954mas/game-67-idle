---
id: T0432
title: Refuse missing Canvas sidecar instead of empty snapshot
status: backlog
project: P001
epic: E010
priority: P1
tags: [canvas, store, integrity]
created: 2026-07-14
updated: 2026-07-14
---

## What

Remove the remaining silent empty-snapshot fallback after the completed Canvas
store relocation. A missing/corrupt sidecar must fail with a stable diagnostic
or recover from an explicitly proven source; it must never publish `{}` as if
that were valid project state.

## Done when

- [ ] `snapshotForEntry` cannot turn a missing sidecar into an empty project.
- [ ] Recovery/refusal behavior has a focused regression test and stable error.
- [ ] Existing local-cache migration and normal project reads remain green.

## Open questions

- Is the journal sufficient for deterministic recovery, or should v1 refuse and
  require the existing repair path?

## Log

- 2026-07-14: Split from T0259 during full Taskboard grooming. The relocation is
  complete; this card owns only the residual integrity risk.
