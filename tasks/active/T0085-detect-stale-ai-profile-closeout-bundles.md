---
id: T0085
title: Detect stale AI profile closeout bundles
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Update AI profile status so a complete closeout bundle is not treated as
reflection-ready when the profile has changed after the bundle was written.

## Done when

- [x] `status.mjs` reports bundle freshness, stale artifact names, and profile
      mtime in JSON and markdown.
- [x] `next_action` warns to rerun `closeout.mjs` or review/followups when
      there is no more urgent current-scope health issue and the bundle is
      stale.
- [x] AI profile tests cover complete fresh and complete stale bundles.
- [x] Profiling docs and reflection skill require rerunning closeout/review
      before reflection when status says the bundle is stale.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started after live profile had 200+ records while existing
  review artifacts still described 159 records; `status.mjs` reported only
  `Bundle complete: yes`, which could mislead reflection.
- 2026-06-13: Added bundle freshness metadata to `status.mjs`: profile mtime,
  artifact mtimes, stale artifact names, `bundle.fresh`, and markdown
  `Bundle fresh` output.
- 2026-06-13: Added status next-action guidance to rerun `closeout.mjs` before
  reflection when a complete bundle is stale and no higher-priority current
  profile health issue is active.
- 2026-06-13: Added AI profile tests for complete fresh and complete stale
  closeout bundles with deterministic mtimes.
- 2026-06-13: Updated profiling docs, reflection skill, skill eval anchors,
  and iteration log so generated reflection artifacts require `Bundle fresh:
  yes` before use.
- 2026-06-13: Evidence: `node --check tools/ai_profile/status.mjs`; `node
  --check tools/skills_eval.mjs`; `node --test tools/ai_profile/test.mjs`
  passed 16 tests; live `node tools/ai_profile/status.mjs` showed `Bundle
  fresh: no (summary, review, review_json, followups, followups_json stale)`;
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `node tools/pipeline_validate.mjs`
  passed, including exported AI profile tests.
- 2026-06-13: Moved to review after stale bundle detection, docs/skill updates, ai profile tests, live status proof, and reusable pipeline validation passed.
