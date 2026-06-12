---
id: T0019
title: "Recurring entropy cleanup: stale docs, unused skills, weak examples"
status: done
epic: E003
priority: P3
tags: [ai-pipeline, process]
created: 2026-06-11
updated: 2026-06-12
---

## What

Add a lightweight anti-entropy validation rule to the task store so active
actionable work cannot remain as empty placeholder tasks. This keeps future
agents from treating unrefined backlog items as ready work and reduces context
noise during resumes.

## Done when

- [x] `node tools/taskboard/cli.mjs validate` fails for active
  `backlog`/`todo`/`doing`/`review` tasks that still have empty `## What` or a
  placeholder-only `## Done when`.
- [x] `node tools/taskboard/cli.mjs validate` fails for active epics that still
  have empty `## Goal`, `## In scope`, or `## Out of scope`.
- [x] `idea` tasks remain allowed to be raw/unrefined.
- [x] Existing taskboard tests cover the new rule.
- [x] Current repo and a fresh export pass taskboard validation.

## Open questions

## Log

- 2026-06-12: Started T0019. Scope: anti-entropy validation for active
  actionable task quality; no broad stale-doc cleanup and no game/runtime work.
- 2026-06-12: Added taskboard validation for active actionable task bodies and
  active epic scope bodies. Filled E003 with a concise goal/scope guardrail so
  the active epic is a useful context index instead of an empty shell.
- 2026-06-12: Evidence passed: `node --test tools/taskboard/test.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node tools/skills_eval.mjs`;
  `node tools/bootstrap/export_base.mjs --target tmp/export-anti-entropy-test-20260612`;
  in the exported project, `node tools/taskboard/cli.mjs validate`,
  `node --test tools/taskboard/test.mjs`, and `node tools/skills_eval.mjs`.
