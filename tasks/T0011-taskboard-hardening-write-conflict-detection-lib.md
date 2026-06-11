---
id: T0011
title: "Taskboard hardening: write-conflict detection + lib roundtrip tests"
status: done
epic: ""
priority: P2
tags: [ai-pipeline, tooling, tested]
created: 2026-06-11
updated: 2026-06-11
---

## What

Protect the task store against silent lost updates when the board UI and
agents edit the same file, and cover the core library with tests.

## Done when

- [x] docs carry a rev (file mtime); PATCH with stale rev returns 409
- [x] board UI sends rev on save and drag-drop, refreshes on conflict
- [x] CLI/agents writing without rev keep last-write-wins (single-shot ops)
- [x] node --test suite covers parser roundtrip, id allocation, update rules, conflict, validate

## Open questions

## Log

- 2026-06-11: Done. Evidence: `node --test tools/taskboard/test.mjs` -> 8 pass / 0 fail;
  live API check: PATCH T0011 with stale rev -> HTTP 409. Tests caught a real
  quote-escaping bug in the frontmatter parser (fixed in lib.mjs unquote).
