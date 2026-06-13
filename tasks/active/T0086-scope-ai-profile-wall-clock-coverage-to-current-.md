---
id: T0086
title: Scope AI profile wall-clock coverage to current iteration
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Update `tools/ai_profile/status.mjs` so wall-clock coverage health is reported
for both whole-profile history and current scope. When current scope is active,
`next_action` should use current-scope coverage instead of repeatedly advising
checkpoint fixes for old profile history.

## Done when

- [x] `status.mjs` JSON and markdown include current-scope wall-clock coverage.
- [x] Low-coverage `next_action` is based on current-scope coverage when
      current scope has `updated_at`, while whole-profile coverage remains
      visible for retrospective history.
- [x] AI profile tests cover historical low coverage not affecting a fresh
      current scope and current-scope low coverage still recommending
      `checkpoint.mjs`.
- [x] Profiling docs and reflection skill mention current-scope wall-clock
      coverage versus whole-profile history.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started after live `status.mjs` still recommended checkpoint
  work from whole-profile low coverage even though current-scope context and
  work-item health were clean.
- 2026-06-13: Added current-scope wall-clock coverage to `status.mjs` JSON and
  markdown, and changed low-coverage next-action priority to use current-scope
  coverage when scope has `updated_at`.
- 2026-06-13: Added AI profile tests proving current-scope low coverage still
  recommends `checkpoint.mjs`, while historical-only low coverage does not.
- 2026-06-13: Updated profiling docs, reflection skill, skill eval anchors,
  and iteration log to distinguish current-scope wall-clock coverage from
  whole-profile history.
- 2026-06-13: Evidence: `node --check tools/ai_profile/status.mjs`; `node
  --check tools/skills_eval.mjs`; `node --test tools/ai_profile/test.mjs`
  passed 17 tests; live `node tools/ai_profile/status.mjs` showed
  `Current scope wall-clock coverage: unknown (0s / 0s)` while whole-profile
  coverage stayed low and next action moved to stale bundle refresh; `node
  tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `node tools/pipeline_validate.mjs`
  passed, including exported AI profile tests.
- 2026-06-13: Moved to review after current-scope wall-clock coverage status, docs/skill updates, ai profile tests, live status proof, and reusable pipeline validation passed.
