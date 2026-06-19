---
id: T0006
title: "doc_reference_check: existence-check non-.md repo refs (.mjs/.py/.sh/.json)"
status: backlog
epic: E001
priority: P2
tags: [pipeline, validators]
created: 2026-06-19
updated: 2026-06-19
---

## What

`doc_reference_check` only existence-checks `.md` backtick refs, so the constant
`tools/*.mjs` and `scripts/*.py` path citations in agent docs drift uncaught — a
renamed tool leaves dead doc paths and nothing fails. Extend the existing
markdown walk to existence-check repo-relative backtick refs ending in
`.mjs/.py/.sh/.json` (tools/, scripts/, state/). ~10 lines, reuse the loop.

## Done when

- [ ] a renamed/removed tool cited in an agent doc fails doc_reference_check
- [ ] doc_reference_check.test.mjs covers the new ref types; validate green

## Open questions

## Log
