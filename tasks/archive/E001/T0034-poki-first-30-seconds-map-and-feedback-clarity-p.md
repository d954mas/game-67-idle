---
id: T0034
title: Poki first-30-seconds map and feedback clarity pass
status: done
epic: E001
priority: P1
tags: [ux,poki,first-30-seconds,ui]
created: 2026-06-12
updated: 2026-06-12
---

## What

Make the first 30 seconds clearer and more inviting for a casual/Poki test player without rebuilding the whole art direction yet.

Current problem: the first map is technically readable, but the locked future shrine appears before the obvious tutorial route, copy is heavy, and rewards are reported like logs instead of bright player feedback.

Out of scope: full cartoon art replacement, new asset pipeline, full web packaging, new combat systems.

## Done when

- [x] Fresh map shows one obvious available route first and hides/separates future routes until context exists.
- [x] First action and first reward copy clearly tell the player what to do and what they earned.
- [x] Shrine/Hunter's Ford progression labels remain clear after unlocks.
- [x] Existing smoke/playtest automation still covers the core loop.
- [x] Native and web QA builds still complete.
- [x] Director review identifies remaining visual gaps for the next iteration.

## Open questions

None for this pass.

## Log

- 2026-06-12: Started after T0033. Focus is first-30s clarity for Poki/casual testing, not full art replacement.
- 2026-06-12: Completed director iteration 1. Fresh map now shows only `GO! / Old Road`; Hunter's Ford and Sealed Shrine are hidden until route/relic context exists. Updated first prompt and reward copy, first-30s contract, smoke test, and agent playtest. Native debug, first-30s contract, smoke, full-loop playtest, `game-wasm-qa`, and mobile web visual audit passed. Remaining blockers are visual polish and reproducible `game-wasm-release`, tracked outside this task.
