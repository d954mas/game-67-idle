---
id: T0377
title: Fix progression generator state schema contract
status: done
project: P001
epic: E015
priority: P0
tags: [progression, schema]
created: 2026-07-10
updated: 2026-07-12
---

## What

Fix the existing Progression generator contract now, independently of the open
decision about replacing its authoring format with Balance Lua.

## Done when

- [x] Progression generation requires explicit `--state-schema` and validates
      against the real game-owned progression/save schema.
- [x] Duplicated `MAX_TRACK_ID_LEN=63` is removed; the generator derives the
      accepted identifier constraint from the owning schema/contract.
- [x] Tests cover missing/wrong schema, boundary identifiers, and generated
      provenance using game-relative paths.
- [x] If `T0368` later removes the generator, that decision explicitly
      supersedes this task; until then the current defect has an owner.

## Open questions

## Log

- 2026-07-10: Recovered by final transcript audit; it was accepted earlier but
  had been accidentally hidden behind the still-open one-file Lua proposal.
- 2026-07-10: Execute after T0359 stabilizes the shared state-generator decomposition/provenance boundaries.
- 2026-07-12: Checkpoint: started after T0361. Scope is the progression generator/state-schema boundary only: explicit state schema, derived identifier limit, provenance, consumer codegen wiring, and focused tests. T0393 audio runtime and unrelated web-dressup work remain excluded.
- 2026-07-12: TDD evidence: initial RED produced 9 expected failures across 4 tests; review regressions separately proved UTF-8 byte-bound and unsafe provenance failures. GREEN generator contract suite 5/5, Python contracts 3/3, feature contracts 8/8 plus live validator, and real template/web-dressup codegen.
- 2026-07-12: Contract evidence: generator requires and validates game_seed.progression, derives string_max-1 in UTF-8 bytes, emits comment-safe game-relative provenance, and both consumers pass the schema in command and DEPENDS. Breaking CLI contract is progression-core 2.0.0.
- 2026-07-12: Review cycle 1 found UTF-8 capacity, provenance-comment, SemVer MAJOR, and reproducible version/revision pin issues; all fixed. Two independent final read-only rechecks returned 0 HIGH and 0 actionable MEDIUM/LOW.
- 2026-07-12: Reproducibility evidence: feature/consumer implementation commit f86c58d483c9d8636b4d922e2982cb7335f70c52; web-dressup progression-core pin is 2.0.0 at that exact revision. The adjacent external audio dependency hunk remains unstaged.
- 2026-07-12: Quality: QTECH_001=pass; evidence: schema/CLI/boundary/provenance tests, real two-consumer codegen, version validator, exact revision pin, and two clean reviews.
