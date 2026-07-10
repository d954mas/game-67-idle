---
id: T0353
title: Add thin studio verify facade and full Windows Linux CI
status: backlog
project: P001
epic: E015
priority: P0
tags: [ci, verification, node]
created: 2026-07-10
updated: 2026-07-10
---

## What

Add a thin Node `studio` command that routes to module-owned tools and provides
one fast local verification contract plus one complete Studio CI contract.

## Done when

- [ ] `studio describe --json` discovers supported domains and stable routes for
      `game create/doctor/build/run/test/package`, Canvas, Taskboard, and assets,
      with normalized machine-readable envelopes and exit behavior.

- [ ] `studio verify --changed` selects the narrowest relevant Studio/feature/
      reference-template checks and emits compact stable output.
- [ ] `studio verify --full` runs the complete Studio, shared-feature, and
      reference-template matrix on Windows and Linux GitHub runners.
- [ ] Studio verification never discovers or runs `games/<id>` tests, playable
      proof, packaging, doctor, or CI.
- [ ] Module domain logic stays in owning CLIs; `studio` is routing only and its
      normalized result never becomes a second domain implementation.
- [ ] The reference template's web build uses Node orchestration with output
      parity before the Bash wrapper is removed.
- [ ] CI separates blocking deterministic gates from advisory timing reports.

## Open questions

## Log

- 2026-07-10: There was no `.github/workflows` directory at plan creation.
