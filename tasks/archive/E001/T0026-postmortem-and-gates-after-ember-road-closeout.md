---
id: T0026
title: Postmortem and gates after Ember Road closeout
status: done
epic: E001
priority: P0
tags: [pipeline, postmortem, gates, architecture]
created: 2026-06-21
updated: 2026-06-21
---

## What

Convert the Ember Road failure modes into reusable pipeline gates so future
game work stops before repeating the same path: weak reference readiness,
feature expansion after lead rejection, review-score/acceptance confusion, and
monolithic active runtime growth.

### Scope

- Add a pre-implementation workflow guard for active game tasks.
- Keep the guard dormant for a clean seed with no active game concept.
- Cover lead-rejection freeze, reference readiness, and runtime monolith risk.
- Document the gate as process, not as a game implementation change.

### Out Of Scope

- No Ember Road continuation.
- No new game concept.
- No runtime/gameplay/visual implementation.
- No broad refactor of product gate internals unless needed by tests.

## Done when

- [x] Workflow guard fails active game work that expands content/features while
      unresolved lead rejection is present.
- [x] Workflow guard fails implementation work when reference grounding is
      explicitly not ready.
- [x] Workflow guard flags a large active runtime unless an architecture/
      decomposition recovery task exists.
- [x] Clean-seed pipeline validation remains green with no active game concept.
- [x] Docs explain that builds/screenshots/reviews are evidence, not lead
      acceptance.

## Open questions

- None.

## Log

- 2026-06-21: Created after Ember Road closeout review. The goal is to fix the
  reusable AI pipeline, not the stopped game implementation.
- 2026-06-21: Added `tools/game_context/workflow_guard.mjs` and fixture tests.
  The guard is dormant in clean seed, blocks feature/content expansion under
  unresolved lead rejection, blocks runtime implementation when
  `reference_grounding.status` is not ready, and flags large
  `src/clean_seed_main.c` without an architecture/decomposition task.
- 2026-06-21: Wired workflow guard into `tools/pipeline_validate.mjs`, updated
  `docs/ai-pipeline/quality-validation.md` under export context budget, and
  validated:
  `node --test tools/game_context/workflow_guard.test.mjs`,
  `node --test tools/pipeline_validate.test.mjs`,
  `node --test tools/bootstrap/export_base.test.mjs`,
  `node tools/ai.mjs validate --review`, and
  `AI_PIPELINE_PYTHON=<bundled-python> node tools/ai.mjs validate --full`.
