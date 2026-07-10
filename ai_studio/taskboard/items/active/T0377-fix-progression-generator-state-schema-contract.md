---
id: T0377
title: Fix progression generator state schema contract
status: backlog
project: P001
epic: E015
priority: P0
tags: [progression, schema]
created: 2026-07-10
updated: 2026-07-10
---

## What

Fix the existing Progression generator contract now, independently of the open
decision about replacing its authoring format with Balance Lua.

## Done when

- [ ] Progression generation requires explicit `--state-schema` and validates
      against the real game-owned progression/save schema.
- [ ] Duplicated `MAX_TRACK_ID_LEN=63` is removed; the generator derives the
      accepted identifier constraint from the owning schema/contract.
- [ ] Tests cover missing/wrong schema, boundary identifiers, and generated
      provenance using game-relative paths.
- [ ] If `T0368` later removes the generator, that decision explicitly
      supersedes this task; until then the current defect has an owner.

## Open questions

## Log

- 2026-07-10: Recovered by final transcript audit; it was accepted earlier but
  had been accidentally hidden behind the still-open one-file Lua proposal.
