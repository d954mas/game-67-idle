---
id: T0091
title: Make AI profile review current-scope first
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make `review.mjs` current-scope-first so agents reading review markdown do not
mistake historical whole-profile findings for urgent current work after
follow-up filtering has already proven the active scope is clean.

## Done when

- [x] Review markdown starts with current-scope findings/actions before
      historical whole-profile findings.
- [x] Review JSON exposes `current_scope.findings` and
      `current_scope.suggested_actions`.
- [x] Clean current scope reports a clean-baseline action while preserving
      historical findings separately.
- [x] Regression tests cover clean current-scope review and current-scope
      problem reporting.
- [x] Profiling docs and reflection skill tell agents to read current-scope
      findings first.

## Open questions

## Log

- 2026-06-13: After `T0090`, followups were clean, but `review.md` still listed
  historical whole-profile findings under `Priority Findings`, which could
  mislead a later reflection agent into chasing stale issues.
- 2026-06-13: Implemented current-scope-first review markdown and JSON fields,
  added regression tests for clean and dirty current scopes, updated profiling
  docs/reflection skill/eval anchors, and validated with
  `node --test tools/ai_profile/test.mjs`, `node tools/skills_eval.mjs`,
  `node tools/taskboard/cli.mjs validate`, `git diff --check`, and
  `node tools/pipeline_validate.mjs`.
- 2026-06-13: Current-scope-first profile review implemented and validated.
