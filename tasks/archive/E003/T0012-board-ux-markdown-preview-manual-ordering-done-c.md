---
id: T0012
title: "Board UX: markdown preview in editor"
status: done
epic: E003
priority: P3
tags: [ai-pipeline, tooling]
created: 2026-06-11
updated: 2026-06-12
---

## What

Add a live Markdown preview to the taskboard editor so agents and humans can
read task bodies, done-when checklists, logs, and epic scope sections without
mentally parsing raw Markdown.

Out of scope for this slice: manual ordering and separate done/archive column
behavior. Those affect board data/flow and should be split into separate tasks
if still needed.

## Done when

- [x] Editor shows raw Markdown and rendered preview at the same time.
- [x] Preview safely escapes HTML and renders common task syntax: headings,
  bullet lists, checkboxes, inline code, bold text, and fenced code blocks.
- [x] Preview updates when opening a task/epic and while typing.
- [x] Current repo passes taskboard validation.

## Open questions

Should manual ordering and explicit archive UX be kept as separate tasks, or is
the current status/priority ordering enough for now?

## Log

- 2026-06-12: Started T0012 with narrowed scope: Markdown preview only. Manual
  ordering and done-column archive UX are intentionally out of scope.
- 2026-06-12: Added side-by-side Markdown preview to task/epic editor and moved
  the safe renderer into `tools/taskboard/public/markdown_preview.js`.
- 2026-06-12: Evidence passed: `node --check tools/taskboard/public/app.js`;
  `node --check tools/taskboard/public/markdown_preview.js`;
  `node --test tools/taskboard/test.mjs`; `node tools/taskboard/cli.mjs validate`;
  fresh export to `tmp/export-taskboard-preview-test-20260612` with taskboard
  tests, taskboard validation, skill eval, and `markdown_preview.js` present.
  HTTP smoke against local server returned 200 for `/`, `/app.js`,
  `/markdown_preview.js`, and `/style.css`. Browser visual automation was not
  run because Playwright is not installed in this repo.
