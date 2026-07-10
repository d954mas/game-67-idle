---
id: T0360
title: Refactor Items tooling around one strict catalog model
status: backlog
project: P001
epic: E015
priority: P1
tags: [items, tooling, validation]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make Items operations and code generation consume one strict catalog model,
with explicit project context and pure rendering. Keep game-owned tests in the
game while giving the reusable feature unit fixtures and reference integration.

## Done when

- [ ] Every Items CLI entry takes one explicit `--project-root`; missing or
      ambiguous context fails instead of guessing from cwd.
- [ ] Full validation requires the game-local `content/items.lock.json` and
      fails clearly when its destructive-change contract is missing or stale.
- [ ] Operations and code generation share one parsed/validated catalog model;
      validation is separate from a side-effect-free renderer.
- [ ] Generated provenance paths are game-relative and deterministic.
- [ ] Feature unit fixtures cover catalog/ownership edge cases and the reference
      template supplies integration proof; Studio does not discover game tests.
- [ ] The public ownership API and current catalog semantics remain compatible.

## Open questions

- Typed Balance references and numeric ownership are deliberately deferred to
  `T0369`; do not decide them during this refactor.

## Log

- 2026-07-10: Consolidate duplicate internal models, not Items and Balance
  ownership policy.
