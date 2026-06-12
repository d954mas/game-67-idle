---
id: T0020
title: "cli.mjs validate: add remediation hints to problem messages"
status: done
epic: E003
priority: P3
tags: [ai-pipeline, tooling]
created: 2026-06-11
updated: 2026-06-12
---

## What

Make `node tools/taskboard/cli.mjs validate` easier for agents to recover from
by printing short remediation hints for common task-store problems.

## Done when

- [x] `validate` keeps the existing `problem:` lines but adds `hint:` lines for
  missing IDs/titles, duplicate IDs, invalid status, missing epic references,
  unrefined actionable tasks, and empty active epics.
- [x] Hints are generated in CLI output, without changing the `validateStore`
  return shape.
- [x] Tests cover at least one CLI validate failure with hints.
- [x] Current repo and a fresh export pass taskboard tests and validation.

## Open questions

## Log

- 2026-06-12: Started T0020. Scope: remediation hints for taskboard validate
  output only; no game/runtime work and no task-store schema change.
- 2026-06-12: Added CLI-only remediation hints for common validate problems and
  covered actionable-task hint output with a CLI test.
- 2026-06-12: Evidence passed: `node --test tools/taskboard/test.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node tools/skills_eval.mjs`;
  `node tools/bootstrap/export_base.mjs --target tmp/export-validate-hints-test-20260612`;
  in the exported project, `node tools/taskboard/cli.mjs validate`,
  `node --test tools/taskboard/test.mjs`, and `node tools/skills_eval.mjs`.
