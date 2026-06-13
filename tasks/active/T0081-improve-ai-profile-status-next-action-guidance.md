---
id: T0081
title: Improve AI profile status next-action guidance
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Improve `tools/ai_profile/status.mjs` so its `next_action` distinguishes
current setup problems from historical profile gaps. If current persistent
scope is already set, old missing work-item records should not keep advising
the agent to set scope again.

## Done when

- [x] New or empty profiles recommend `start.mjs` as the default profiling
      entry point.
- [x] Profiles with historical missing work-item records do not recommend
      `scope.mjs set` when current scope is already valid and has a work item.
- [x] AI profile tests cover status guidance for a scoped profile with older
      unscoped records.
- [x] Profiling docs mention that status next-action distinguishes current
      setup from historical gaps.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started after live `status.mjs` showed current scope set to
  `T0080/profile-start-helper` but still recommended `scope.mjs set` because
  older records lacked work-item metadata.
- 2026-06-13: Updated `status.mjs` to recommend `start.mjs` for missing or
  empty profiles, check current scope readiness before historical work-item
  coverage, and let the next current issue surface when scope is already set.
- 2026-06-13: Added AI profile regression tests for missing-profile
  `start.mjs` guidance and for historical unscoped records with current valid
  scope.
- 2026-06-13: Updated profiling docs, reflection skill, skill eval anchors,
  and iteration log so status next-action separates current setup from
  historical metadata gaps.
- 2026-06-13: Evidence: `node --test tools/ai_profile/test.mjs` passed 7
  tests; `node --check tools/ai_profile/status.mjs`; `node --check
  tools/skills_eval.mjs`; `node tools/skills_sync.mjs`; `node
  tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; live `node
  tools/ai_profile/status.mjs` showed scope set to `T0081/status-next-action`
  and next action `context.mjs`; `node tools/pipeline_validate.mjs` passed,
  including exported AI profile tests.
- 2026-06-13: Moved to review after status next-action fix, docs/skill updates, ai profile tests, live status check, and reusable pipeline validation passed.
