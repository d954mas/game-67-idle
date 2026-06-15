---
id: T0017
title: Keep live status short after closed prototypes
status: done
epic: E003
priority: P1
tags: [pipeline, taskboard, context]
created: 2026-06-15
updated: 2026-06-15
---

## What

Prevent closed prototype evidence from bloating the live task context. Keep
`tasks/STATUS.md` as a compact current-state index and make taskboard validation
fail when it grows into an archive.

## Done when

- [x] `tasks/STATUS.md` is shortened to current pipeline state, with historical
  Splash Rods/Rune Marches details represented as pointers instead of embedded
  evidence.
- [x] `node tools/taskboard/cli.mjs validate` fails when `tasks/STATUS.md`
  exceeds the live-status context budget.
- [x] Tests cover the new status budget rule.
- [x] `tasks/README.md` documents the budget and the reason for it.

## Open questions

None.

## Log
- 2026-06-15: Shortened `tasks/STATUS.md` from 6700 to 3230 chars,
  added a 6000-char live-status validation budget, CLI remediation hint, and
  taskboard tests. Validation: `node tools/taskboard/cli.mjs validate` and
  `node --test tools/taskboard/test.mjs` passed.
- 2026-06-15: Closed after status budget guardrail, compact live status, taskboard tests, and validation passed.
