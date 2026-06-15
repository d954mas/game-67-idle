---
id: T0037
title: Teach validation planner game-context scoped checks
status: done
epic: E003
priority: P1
tags: [ai-profile, game-context, validation]
created: 2026-06-15
updated: 2026-06-15
---

## What

Teach the AI validation planner that `tools/game_context/*` and explicit
game-context changes require the game-context scoped test suite. Without this,
new-prototype/startup gate edits look like docs-only changes and can skip the
tests that protect the next prototype kickoff.

## Done when

- [x] `plan_validation.mjs --change game-context` recommends
  `node --test tools/game_context/test.mjs`.
- [x] `plan_validation.mjs --file tools/game_context/new_prototype.mjs` infers
  the game-context change kind.
- [x] Tests cover explicit and file-inferred game-context validation planning.
- [x] Process/status docs mention the scoped planner route.

## Open questions

- none; this is a scoped validation-planner fix.

## Log

- 2026-06-15: Started after finding `tools/game_context/new_prototype.mjs`
  was inferred as `docs` only by `plan_validation.mjs`.
- 2026-06-15: Added `game-context` change kind, aliases, file inference for
  `tools/game_context/`, and the `game-context-tests` scoped check in
  `tools/ai_profile/plan_validation.mjs`.
- 2026-06-15: Validation passed:
  `node tools/ai_profile/plan_validation.mjs --change game-context --risk medium --json`;
  `node tools/ai_profile/plan_validation.mjs --file tools/game_context/new_prototype.mjs --json`;
  `node --test tools/game_context/test.mjs`; `node --test tools/ai_profile/test.mjs`;
  `node tools/ai.mjs validate --change profiling --change docs --change game-context --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
