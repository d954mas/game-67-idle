---
id: T0353
title: Add thin studio verify facade and full Windows Linux CI
status: done
project: P001
epic: E015
priority: P0
tags: [ci, verification, node]
created: 2026-07-10
updated: 2026-07-12
---

## What

Add a thin Node `studio` command that routes to module-owned tools and provides
one fast local verification contract plus one complete Studio CI contract.

## Done when

- [x] `studio describe --json` discovers supported domains and stable routes for
      `game create/doctor/build/run/test/package`, Canvas, Taskboard, and assets,
      with normalized machine-readable envelopes and exit behavior.

- [x] `studio verify --changed` selects the narrowest relevant Studio/feature/
      reference-template checks and emits compact stable output.
- [x] `studio verify --full` runs the complete Studio, shared-feature, and
      reference-template matrix on Windows and Linux GitHub runners.
- [x] Studio verification never discovers or runs `games/<id>` tests, playable
      proof, packaging, doctor, or CI.
- [x] Module domain logic stays in owning CLIs; `studio` is routing only and its
      normalized result never becomes a second domain implementation.
- [x] The reference template's web build uses Node orchestration with output
      parity before the Bash wrapper is removed.
- [x] CI separates blocking deterministic gates from advisory timing reports.

## Open questions

## Log

- 2026-07-10: There was no `.github/workflows` directory at plan creation.
- 2026-07-10: Execution dependency: consume T0357's root environment and canonical build paths; do not recreate toolchain discovery in the facade.
- 2026-07-10: Also consume T0361's stable feature metadata/router contract before finalizing feature verification discovery.
- 2026-07-12: Checkpoint after T0354 at 0b8b5b347. Implement thin routing-only studio describe/verify facade and deterministic Windows/Linux CI while preserving unrelated T0393, E017, and planning WIP.
- 2026-07-12: Implementation commits 2b3c81a74, 736f13159, db7c67230, a82be6775, 57709dcf2, eee7291ca, 67d873f35, 1264203c7, and faf39009f add the facade, complete owner-suite inventory, clean-runner prerequisites, cross-platform native/web fixes, bounded failure context, and deterministic/advisory CI separation.
- 2026-07-12: Focused evidence: Studio facade tests 14/14; asset owner suite 144/144; Linux `/tmp` regression 6/6 in normal and forced-temp modes; CI/timing contract 4/4; opt-in Canvas timing 2/2; every review/fix cycle ended with two independent read-only reviews reporting 0 HIGH and 0 actionable findings.
- 2026-07-12: Full clean-runner evidence: GitHub Actions run 29203676405 on faf39009f passed Ubuntu (7m25s) and Windows (10m53s), each executing blocking `node ai_studio/studio.mjs verify --full`; https://github.com/d954mas/game-67-idle/actions/runs/29203676405.
- 2026-07-12: Quality: QTECH_001=pass; evidence: facade and owner-suite tests; two independent review cycles; GitHub Actions 29203676405 passed Ubuntu and Windows on faf39009f
